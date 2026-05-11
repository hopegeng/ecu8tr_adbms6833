#include "adbms6830_driver.h"
#include "adbms6830_reg.h"
#include "adbms6830_pec.h"
#include "adbms6830_shared.h"

#include <string.h>
#include "shell.h"

//ADBMS6830 datasheet: D:\NeutronControls\ADI\ADBMS6830\ds_ADBMS6830_Datasheet_RevSpA_2021-12.pdf
#define ADBMS6830_READY_DELAY_US	 		(10)			//rReady: 10us
#define ADBMS6830_WAKE_DELAY_US      		(400U)			//tWAKE = 500us,
#define ADBMS6830_LINK_IDLE_TIMEOUT  		(5U)			//tIDLE = 5ms, ADBMS6830 Datasheet P9
#define ADBMS6830_SLEEP_WATCHDOG_TIMEOUT	(2000U)			//the page 31
#define ADBMS6830_REG_GROUP_DATA_LEN 		(6U)
#define ADBMS6830_MEASUREMENT_TIME			(8)				//1ms needed to a averaged measurement; tested, 1ms just read out 0x8000, only 8ms works
#define ADBMS6830_AVG_MEASUREMENT_TIME		(8)				//8ms for averged measured time
#define ADBMS6830_CELL_CONVERSION_OFFSET	(1500U)			//Table 106: Result registes bit description
#if ADBMS6830_ENABLE_DEBUG_PRINTF
#define ADBMS6830_DEBUG_PRINTF(...)      PRINTF(__VA_ARGS__)
#else
#define ADBMS6830_DEBUG_PRINTF(...)      ((void)0)
#endif

#define ENABLE_PRECAUTION_WAKEUP			 0			//We may not need so many wakeup call


static Adbms6830_Status_t Adbms6830_ReadRegisterGroup(const Adbms6830_Hal_t *hal,
                                                      uint16_t cmd,
                                                      uint8_t *rxData,
                                                      uint16_t dataLen,
                                                      uint8_t icCount);
static void Adbms6830_ParseCellBlock(Adbms6830_CellData_t *cell, const uint8_t *block);

static void Adbms6830_ParseCellGroup(Adbms6830_CellData_t *cell,
                                     const uint8_t *block,
                                     uint8_t startCellIndex,
                                     uint8_t cellCount);
static void Adbms6830_ParseAuxGroup(Adbms6830_AuxData_t *aux,
                                    const uint8_t *block,
                                    uint8_t startAuxIndex,
                                    uint8_t auxCount);
static void Adbms6830_ParseAuxStatusA(Adbms6830_AuxStatusData_t *auxStatus, const uint8_t *block);
static void Adbms6830_ParseAuxStatusB(Adbms6830_AuxStatusData_t *auxStatus, const uint8_t *block);
static uint32_t Adbms6830_RawToPrimaryAuxMilliVolt(uint16_t raw, uint8_t auxIndex);
static int16_t Adbms6830_RawToItempDegC(uint16_t raw);

static Adbms6830_Status_t Adbms6830_BuildCmdFrame(uint16_t cmd, uint8_t frame[ADBMS6830_CMD_FRAME_SIZE])
{
    if (frame == NULL)
    {
        return ADBMS6830_ERR_PARAM;
    }

    frame[0] = (uint8_t)(cmd >> 8U);
    frame[1] = (uint8_t)(cmd & 0xFFU);

    {
        uint16_t pec = Adbms_Pec15_Calc(frame, 2U);
        frame[2] = (uint8_t)(pec >> 8U);
        frame[3] = (uint8_t)(pec & 0xFFU);
    }

    return ADBMS6830_OK;
}

static Adbms6830_Status_t Adbms6830_SendCommandOnly(const Adbms6830_Hal_t *hal, uint16_t cmd)
{
    uint8_t tx[ADBMS6830_CMD_FRAME_SIZE];
    uint8_t rx[ADBMS6830_CMD_FRAME_SIZE];
    Adbms6830_Status_t st;

    if ((hal == NULL) || (hal->spiTransfer == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    st = Adbms6830_BuildCmdFrame(cmd, tx);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

#if ENABLE_PRECAUTION_WAKEUP
    st = Adbms6830_WakeUp(hal);
    if (st != ADBMS6830_OK)
    {
        return st;
    }
#endif


    return hal->spiTransfer(tx, rx, ADBMS6830_CMD_FRAME_SIZE );
}

static uint8_t Adbms6830_ExtractCmdCounter(const uint8_t *payload, uint16_t dataLen)
{
    return (uint8_t)(payload[dataLen] >> 2U);
}

static uint16_t Adbms6830_ExtractRxPec10(const uint8_t *payload, uint16_t dataLen)
{
    return (uint16_t)((((uint16_t)payload[dataLen] & 0x0003U) << 8U) |
                      (uint16_t)payload[dataLen + 1U]);
}

static bool Adbms6830_VerifyRxBlockPec10(const uint8_t *payload, uint16_t dataLen)
{
    uint8_t cmdCounter;
    uint16_t rxPec;
    uint16_t calcPec;

    if (payload == NULL)
    {
        return false;
    }

    cmdCounter = Adbms6830_ExtractCmdCounter(payload, dataLen);
    rxPec = Adbms6830_ExtractRxPec10(payload, dataLen);
    calcPec = Adbms_Pec10_Calc(payload, cmdCounter, dataLen);

    return (rxPec == calcPec);
}

static Adbms6830_Status_t Adbms6830_ReadRegisterGroup(const Adbms6830_Hal_t *hal,
                                                      uint16_t cmd,
                                                      uint8_t *rxData,
                                                      uint16_t dataLen,
                                                      uint8_t icCount)
{
    uint16_t totalLen;
    uint8_t tx[ADBMS6830_CMD_FRAME_SIZE + ADBMS6830_MAX_ICS * (ADBMS6830_REG_GROUP_DATA_LEN + ADBMS6830_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t offset;
    uint8_t ic;

    if ((hal == NULL) || (hal->spiTransfer == NULL) || (rxData == NULL) || (dataLen != ADBMS6830_REG_GROUP_DATA_LEN))
    {
        return ADBMS6830_ERR_PARAM;
    }

    totalLen = (uint16_t)(ADBMS6830_CMD_FRAME_SIZE +
                          ((uint16_t)icCount * (dataLen + ADBMS6830_DATA_PEC_SIZE)));

    (void)memset(tx, 0, totalLen);
    (void)Adbms6830_BuildCmdFrame(cmd, tx);

#if ENABLE_PRECAUTION_WAKEUP
    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }
#endif

    if (hal->spiTransfer(tx, rx, totalLen) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

    offset = ADBMS6830_CMD_FRAME_SIZE;

    for (ic = 0U; ic < icCount; ic++)
    {
        //uint8_t rxIndex = (uint8_t)((icCount - 1U) - ic);

        if (Adbms6830_VerifyRxBlockPec10(&rx[offset], dataLen) == false)
        {
            return ADBMS6830_ERR_PEC;
        }

        (void)memcpy(&rxData[(uint16_t)ic * dataLen], &rx[offset], dataLen);
        offset = (uint16_t)(offset + dataLen + ADBMS6830_DATA_PEC_SIZE);
    }

    return ADBMS6830_OK;
}

static void Adbms6830_ParseCellBlock(Adbms6830_CellData_t *cell, const uint8_t *block)
{
    uint8_t i;

    if ((cell == NULL) || (block == NULL))
    {
        return;
    }

    for (i = 0U; i < ADBMS6830_CELLS_PER_IC; i++)
    {
        uint16_t raw = (uint16_t)(((uint16_t)block[(uint16_t)(2U * i) + 1U] << 8U) |
                                   (uint16_t)block[(uint16_t)(2U * i)]);

        cell->raw[i] = raw;
        cell->mV[i]  = Adbms6830_RawTo_mV(raw);
    }
}

static void Adbms6830_ParseCellGroup(Adbms6830_CellData_t *cell,
                                     const uint8_t *block,
                                     uint8_t startCellIndex,
                                     uint8_t cellCount)
{
    uint8_t i;

    if ((cell == NULL) || (block == NULL))
    {
        return;
    }

    for (i = 0U; i < cellCount; i++)
    {
        uint16_t raw = (uint16_t)(((uint16_t)block[(uint16_t)(2U * i) + 1U] << 8U) |
                                   (uint16_t)block[(uint16_t)(2U * i)]);

        cell->raw[(uint8_t)(startCellIndex + i)] = raw;
        cell->mV[(uint8_t)(startCellIndex + i)]  = Adbms6830_RawTo_mV(raw);
    }
}

static void Adbms6830_ParseAuxGroup(Adbms6830_AuxData_t *aux,
                                    const uint8_t *block,
                                    uint8_t startAuxIndex,
                                    uint8_t auxCount)
{
    uint8_t i;

    if ((aux == NULL) || (block == NULL))
    {
        return;
    }

    for (i = 0U; i < auxCount; i++)
    {
        uint8_t auxIndex = (uint8_t)(startAuxIndex + i);
        uint16_t raw = (uint16_t)(((uint16_t)block[(uint16_t)(2U * i) + 1U] << 8U) |
                                   (uint16_t)block[(uint16_t)(2U * i)]);

        aux->raw[auxIndex] = raw;
        aux->mV[auxIndex]  = Adbms6830_RawToPrimaryAuxMilliVolt(raw, auxIndex);
    }
}

static void Adbms6830_ParseAuxStatusA(Adbms6830_AuxStatusData_t *auxStatus, const uint8_t *block)
{
    if ((auxStatus == NULL) || (block == NULL))
    {
        return;
    }

    auxStatus->vref2Raw = (uint16_t)(((uint16_t)block[1] << 8U) | (uint16_t)block[0]);
    auxStatus->vref2mV = (uint32_t)Adbms6830_RawTo_mV(auxStatus->vref2Raw);
    auxStatus->itmpRaw = (uint16_t)(((uint16_t)block[3] << 8U) | (uint16_t)block[2]);
    auxStatus->itmpDegC = Adbms6830_RawToItempDegC(auxStatus->itmpRaw);
}

static void Adbms6830_ParseAuxStatusB(Adbms6830_AuxStatusData_t *auxStatus, const uint8_t *block)
{
    if ((auxStatus == NULL) || (block == NULL))
    {
        return;
    }

    auxStatus->vdRaw = (uint16_t)(((uint16_t)block[1] << 8U) | (uint16_t)block[0]);
    auxStatus->vdmV = (uint32_t)Adbms6830_RawTo_mV(auxStatus->vdRaw);
    auxStatus->vaRaw = (uint16_t)(((uint16_t)block[3] << 8U) | (uint16_t)block[2]);
    auxStatus->vamV = (uint32_t)Adbms6830_RawTo_mV(auxStatus->vaRaw);
    auxStatus->vresRaw = (uint16_t)(((uint16_t)block[5] << 8U) | (uint16_t)block[4]);
    auxStatus->vresmV = (uint32_t)Adbms6830_RawTo_mV(auxStatus->vresRaw);
}

static uint32_t Adbms6830_RawToPrimaryAuxMilliVolt(uint16_t raw, uint8_t auxIndex)
{
    uint32_t baseMilliVolt = (uint32_t)Adbms6830_RawTo_mV(raw);

    if (auxIndex == 11U)
    {
        return (baseMilliVolt * 25U);
    }

    return baseMilliVolt;
}

static int16_t Adbms6830_RawToItempDegC(uint16_t raw)
{
    uint32_t milliVolt = (uint32_t)Adbms6830_RawTo_mV(raw);
    int32_t tempDegC = (int32_t)(((milliVolt * 10U) + 37U) / 75U) - 273;

    return (int16_t)tempDegC;
}

void Adbms6830_Init(Adbms6830_Context_t *ctx, uint8_t icCount)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    if (icCount > ADBMS6830_MAX_ICS)
    {
        icCount = ADBMS6830_MAX_ICS;
    }

    ctx->icCount   = icCount;
    ctx->linkState = ADBMS6830_LINK_OFF;
    ctx->svcState  = ADBMS6830_SVC_BOOT;
    ctx->pecErrorCount = 0U;
    ctx->commErrorCount = 0U;
    ctx->measureCount = 0U;
}

void Adbms6830_SetDefaultCommands(Adbms6830_CommandSet_t *cmds)
{
    if (cmds == NULL)
    {
        return;
    }

    cmds->WRCFGA = ADBMS6830_WRCFGA_REG;
    cmds->WRCFGB = ADBMS6830_WRCFGB_REG;
    cmds->RDCFGA = ADBMS6830_RDCFGA_REG;
    cmds->RDCFGB = ADBMS6830_RDCFGB_REG;
    cmds->RDSTATA = ADBMS6830_RDSTATA_REG;
    cmds->RDSTATB = ADBMS6830_RDSTATB_REG;
    cmds->RDSTATC = ADBMS6830_RDSTATC_REG;
    cmds->RDSID = ADBMS6830_RDSID_REG;
    cmds->SRST = ADBMS6830_SRST_REG;
    cmds->CLRFLAG = ADBMS6830_CLRFLAG_REG;
    cmds->RDCVA = ADBMS6830_RDCVA_REG;
    cmds->RDCVB = ADBMS6830_RDCVB_REG;
    cmds->RDCVC = ADBMS6830_RDCVC_REG;
    cmds->RDCVD = ADBMS6830_RDCVD_REG;
    cmds->RDCVE = ADBMS6830_RDCVE_REG;
    cmds->RDCVF = ADBMS6830_RDCVF_REG;
    cmds->RDCVALL = ADBMS6830_RDCVALL_REG;
    cmds->ADCV = (uint16_t)(ADBMS6830_ADCV_BASE_REG);		// 0x260 = MD = 1(normal speed), DCP = 0( no discharge ), CH = 0(all cells )
    cmds->ADAX = ADBMS6830_ADAX_BASE_REG;
    cmds->RDAUXA = ADBMS6830_RDAUXA_REG;
    cmds->RDAUXB = ADBMS6830_RDAUXB_REG;
    cmds->RDAUXC = ADBMS6830_RDAUXC_REG;
    cmds->RDAUXD = ADBMS6830_RDAUXD_REG;
    cmds->MUTE = ADBMS6830_MUTE_REG;
    cmds->UNMUTE = ADBMS6830_UNMUTE_REG;
}

uint16_t Adbms6830_RawTo_mV(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * 15U) / 100U) + ADBMS6830_CELL_CONVERSION_OFFSET;
}

uint32_t Adbms6830_RawTo_uV(uint16_t raw)
{
    return (uint32_t)raw * 150U;
}

Adbms6830_Status_t Adbms6830_WakeUp(const Adbms6830_Context_t *ctx, const Adbms6830_Hal_t *hal)
{
    uint8_t tx[2] = {0x00U, 0x00U};
    uint8_t rx[2];
    Adbms6830_Status_t st;

    if (( ctx == NULL ) || (hal == NULL) || (hal->spiTransfer == NULL) || (hal->delayUs == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    for( uint8_t ic = 0; ic < ctx->icCount; ic++ )
    {
		st = hal->spiTransfer(tx, rx, 2U);
		if (st != ADBMS6830_OK)
		{
			return st;
		}

		hal->delayUs(ADBMS6830_WAKE_DELAY_US);
    }

    return ADBMS6830_OK;
}

Adbms6830_Status_t Adbms6830_StartCellConversion(const Adbms6830_Hal_t *hal,
                                                 const Adbms6830_CommandSet_t *cmds)
{
    if (cmds == NULL)
    {
        return ADBMS6830_ERR_PARAM;
    }

    return Adbms6830_SendCommandOnly(hal, cmds->ADCV);
}

Adbms6830_Status_t Adbms6830_StartAuxConversion(const Adbms6830_Hal_t *hal,
                                                const Adbms6830_CommandSet_t *cmds)
{
    if (cmds == NULL)
    {
        return ADBMS6830_ERR_PARAM;
    }

    return Adbms6830_SendCommandOnly(hal, cmds->ADAX);
}

Adbms6830_Status_t Adbms6830_WriteCfga(Adbms6830_Context_t *ctx,
                                       const Adbms6830_Hal_t *hal,
                                       const Adbms6830_CommandSet_t *cmds)
{
    uint8_t tx[ADBMS6830_CMD_FRAME_SIZE + ADBMS6830_MAX_ICS * (ADBMS6830_BYTES_PER_CFGR + ADBMS6830_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t idx = 0U;
    int32_t ic;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL) || (hal->spiTransfer == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    (void)Adbms6830_BuildCmdFrame(cmds->WRCFGA, tx);
    idx = ADBMS6830_CMD_FRAME_SIZE;

    for (ic = (int32_t)ctx->icCount - 1; ic >= 0; ic--)
    {
        uint16_t pec;
        memcpy(&tx[idx], ctx->cfga[ic].data, ADBMS6830_BYTES_PER_CFGR);
        pec = Adbms_Pec10_Calc(ctx->cfga[ic].data, 0, ADBMS6830_BYTES_PER_CFGR);
        idx += ADBMS6830_BYTES_PER_CFGR;
        tx[idx++] = (uint8_t)(pec >> 8U);
        tx[idx++] = (uint8_t)(pec & 0xFFU);
    }

#if ENABLE_PRECAUTION_WAKEUP
    if (Adbms6830_WakeUp(ctx, hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }
#endif

    return hal->spiTransfer(tx, rx, idx);
}

Adbms6830_Status_t Adbms6830_WriteCfgb(Adbms6830_Context_t *ctx,
                                       const Adbms6830_Hal_t *hal,
                                       const Adbms6830_CommandSet_t *cmds)
{
    uint8_t tx[ADBMS6830_CMD_FRAME_SIZE + ADBMS6830_MAX_ICS * (ADBMS6830_BYTES_PER_CFGR + ADBMS6830_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t idx = 0U;
    int32_t ic;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL) || (hal->spiTransfer == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    (void)Adbms6830_BuildCmdFrame(cmds->WRCFGB, tx);
    idx = ADBMS6830_CMD_FRAME_SIZE;

    for (ic = (int32_t)ctx->icCount - 1; ic >= 0; ic--)
    {
        uint16_t pec;
        memcpy(&tx[idx], ctx->cfgb[ic].data, ADBMS6830_BYTES_PER_CFGR);
        pec = Adbms_Pec10_Calc(ctx->cfgb[ic].data, 0, ADBMS6830_BYTES_PER_CFGR);
        idx += ADBMS6830_BYTES_PER_CFGR;
        tx[idx++] = (uint8_t)(pec >> 8U);
        tx[idx++] = (uint8_t)(pec & 0xFFU);
    }

#if ENABLE_PRECAUTION_WAKEUP
    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }
#endif


    return hal->spiTransfer(tx, rx, idx);
}

Adbms6830_Status_t Adbms6830_ReadCfgb(Adbms6830_Context_t *ctx,
                                      const Adbms6830_Hal_t *hal,
                                      const Adbms6830_CommandSet_t *cmds)
{
    uint8_t readCfgb[ADBMS6830_MAX_ICS * ADBMS6830_REG_GROUP_DATA_LEN];
    uint8_t ic;
    Adbms6830_Status_t st;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    st = Adbms6830_ReadRegisterGroup(hal, cmds->RDCFGB, readCfgb, ADBMS6830_REG_GROUP_DATA_LEN, ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        (void)memcpy(ctx->cfgb[ic].data,
                     &readCfgb[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN],
                     ADBMS6830_REG_GROUP_DATA_LEN);
    }

    return ADBMS6830_OK;
}

Adbms6830_Status_t Adbms6830_ReadCellVoltagesAll(Adbms6830_Context_t *ctx,
                                                 const Adbms6830_Hal_t *hal,
                                                 const Adbms6830_CommandSet_t *cmds)
{
    uint16_t totalLen;
    uint8_t tx[ADBMS6830_CMD_FRAME_SIZE + ADBMS6830_MAX_ICS * (ADBMS6830_RDCVALL_DATA_BYTES + ADBMS6830_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t offset;
    int32_t ic;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL) || (hal->spiTransfer == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    totalLen = (uint16_t)(ADBMS6830_CMD_FRAME_SIZE +
                          ((uint16_t)ctx->icCount *
                           (ADBMS6830_RDCVALL_DATA_BYTES + ADBMS6830_DATA_PEC_SIZE)));

    memset(tx, 0, totalLen);
    (void)Adbms6830_BuildCmdFrame(cmds->RDCVALL, tx);

#if ENABLE_PRECAUTION_WAKEUP
    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }
#endif

    if (hal->spiTransfer(tx, rx, totalLen) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

    offset = ADBMS6830_CMD_FRAME_SIZE;


    for (ic = (int32_t)ctx->icCount - 1; ic >= 0; ic--)
    {
        if (Adbms6830_VerifyRxBlockPec10(&rx[offset], ADBMS6830_RDCVALL_DATA_BYTES) == false)
        {
            ctx->pecErrorCount++;
            return ADBMS6830_ERR_PEC;
        }

        Adbms6830_ParseCellBlock(&ctx->cell[ic], &rx[offset]);
        offset = (uint16_t)(offset + ADBMS6830_RDCVALL_DATA_BYTES + ADBMS6830_DATA_PEC_SIZE);
    }

    return ADBMS6830_OK;
}

Adbms6830_Status_t Adbms6830_ReadCellVoltagesByGroup(Adbms6830_Context_t *ctx,
                                                     const Adbms6830_Hal_t *hal,
                                                     const Adbms6830_CommandSet_t *cmds)
{
    static const uint8_t groupStartIndex[6] = { 0U, 3U, 6U, 9U, 12U, 15U };
    static const uint8_t groupCellCount[6]  = { 3U, 3U, 3U, 3U, 3U, 1U };
    uint8_t groupData[ADBMS6830_MAX_ICS * ADBMS6830_CELL_GROUP_DATA_BYTES];
    uint16_t groupCmds[6];
    uint8_t group;
    uint8_t ic;
    Adbms6830_Status_t st;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL) || (hal->spiTransfer == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    groupCmds[0] = cmds->RDCVA;
    groupCmds[1] = cmds->RDCVB;
    groupCmds[2] = cmds->RDCVC;
    groupCmds[3] = cmds->RDCVD;
    groupCmds[4] = cmds->RDCVE;
    groupCmds[5] = cmds->RDCVF;

    for (group = 0U; group < 6U; group++)
    {
        st = Adbms6830_ReadRegisterGroup(hal,
                                         groupCmds[group],
                                         groupData,
                                         ADBMS6830_CELL_GROUP_DATA_BYTES,
                                         ctx->icCount);
        if (st != ADBMS6830_OK)
        {
            if (st == ADBMS6830_ERR_PEC)
            {
                ctx->pecErrorCount++;
            }
            return st;
        }

        for (ic = 0U; ic < ctx->icCount; ic++)
        {
            Adbms6830_ParseCellGroup(&ctx->cell[ic],
                                     &groupData[(uint16_t)ic * ADBMS6830_CELL_GROUP_DATA_BYTES],
                                     groupStartIndex[group],
                                     groupCellCount[group]);
        }
    }

    return ADBMS6830_OK;
}

Adbms6830_Status_t Adbms6830_ReadAuxVoltagesByGroup(Adbms6830_Context_t *ctx,
                                                    const Adbms6830_Hal_t *hal,
                                                    const Adbms6830_CommandSet_t *cmds)
{
    static const uint8_t groupStartIndex[4] = { 0U, 3U, 6U, 9U };
    uint8_t groupData[ADBMS6830_MAX_ICS * ADBMS6830_CELL_GROUP_DATA_BYTES];
    uint8_t statusData[ADBMS6830_MAX_ICS * ADBMS6830_CELL_GROUP_DATA_BYTES];
    uint16_t groupCmds[4];
    uint8_t group;
    uint8_t ic;
    Adbms6830_Status_t st;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL) || (hal->spiTransfer == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    groupCmds[0] = cmds->RDAUXA;
    groupCmds[1] = cmds->RDAUXB;
    groupCmds[2] = cmds->RDAUXC;
    groupCmds[3] = cmds->RDAUXD;

    if ((ctx->tickMs - ctx->lastCommMs) >= ADBMS6830_LINK_IDLE_TIMEOUT)
    {
        st = Adbms6830_WakeUp(ctx, hal);
        if (st != ADBMS6830_OK)
        {
            ctx->commErrorCount++;
            ctx->linkState = ADBMS6830_LINK_ERROR;
            return st;
        }

        ctx->linkState  = ADBMS6830_LINK_READY;
        ctx->lastCommMs = ctx->tickMs;
    }

    for (group = 0U; group < 4U; group++)
    {
        st = Adbms6830_ReadRegisterGroup(hal,
                                         groupCmds[group],
                                         groupData,
                                         ADBMS6830_CELL_GROUP_DATA_BYTES,
                                         ctx->icCount);
        if (st != ADBMS6830_OK)
        {
        	ADBMS6830_DEBUG_PRINTF( "Aux reading failed group %d, st = %d\r\n", group, st );
            if (st == ADBMS6830_ERR_PEC)
            {
                ctx->pecErrorCount++;
            }
            return st;
        }

        ctx->linkState  = ADBMS6830_LINK_READY;
        ctx->lastCommMs = ctx->tickMs;

        for (ic = 0U; ic < ctx->icCount; ic++)
        {
            Adbms6830_ParseAuxGroup(&ctx->aux[ic],
                                    &groupData[(uint16_t)ic * ADBMS6830_CELL_GROUP_DATA_BYTES],
                                    groupStartIndex[group],
                                    3U);
        }
    }

    st = Adbms6830_ReadRegisterGroup(hal,
                                     cmds->RDSTATA,
                                     statusData,
                                     ADBMS6830_CELL_GROUP_DATA_BYTES,
                                     ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        if (st == ADBMS6830_ERR_PEC)
        {
            ctx->pecErrorCount++;
        }
        return st;
    }

    ctx->linkState  = ADBMS6830_LINK_READY;
    ctx->lastCommMs = ctx->tickMs;

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        Adbms6830_ParseAuxStatusA(&ctx->auxStatus[ic],
                                  &statusData[(uint16_t)ic * ADBMS6830_CELL_GROUP_DATA_BYTES]);
    }

    st = Adbms6830_ReadRegisterGroup(hal,
                                     cmds->RDSTATB,
                                     statusData,
                                     ADBMS6830_CELL_GROUP_DATA_BYTES,
                                     ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        if (st == ADBMS6830_ERR_PEC)
        {
            ctx->pecErrorCount++;
        }
        return st;
    }

    ctx->linkState  = ADBMS6830_LINK_READY;
    ctx->lastCommMs = ctx->tickMs;

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        Adbms6830_ParseAuxStatusB(&ctx->auxStatus[ic],
                                  &statusData[(uint16_t)ic * ADBMS6830_CELL_GROUP_DATA_BYTES]);
    }

    return ADBMS6830_OK;
}


Adbms6830_Status_t Adbms6830_FullInitialize(Adbms6830_Context_t *ctx,
                                            const Adbms6830_Hal_t *hal,
                                            const Adbms6830_CommandSet_t *cmds)
{
    uint8_t readCfga[ADBMS6830_MAX_ICS * ADBMS6830_REG_GROUP_DATA_LEN];
    uint8_t readCfgb[ADBMS6830_MAX_ICS * ADBMS6830_REG_GROUP_DATA_LEN];
    uint8_t readStatc[ADBMS6830_MAX_ICS * ADBMS6830_REG_GROUP_DATA_LEN];
    uint8_t readSid[ADBMS6830_MAX_ICS * ADBMS6830_REG_GROUP_DATA_LEN];
    uint8_t clrFlag[ADBMS6830_BYTES_PER_CFGR];
    uint8_t ic;
    Adbms6830_Status_t st;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }


#if 0
    st = Adbms6830_SendCommandOnly(hal, cmds->SRST);
    if (st != ADBMS6830_OK)
    {
        return st;
    }


    if (hal->delayMs != NULL)
    {
        hal->delayMs(1U);
    }
#endif

    st = Adbms6830_WriteCfga(ctx, hal, cmds);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    st = Adbms6830_ReadRegisterGroup(hal, cmds->RDCFGA, readCfga, ADBMS6830_REG_GROUP_DATA_LEN, ctx->icCount);
    if (st != ADBMS6830_OK)
    {
    	if( st == ADBMS6830_ERR_PEC )
    	{
    		ctx->pecErrorCount++;
    	}
    	if( st == ADBMS6830_ERR_COMM )
    	{
    		ctx->commErrorCount++;
    	}
        return st;
    }


#if 0
    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        if (memcmp(&ctx->cfga[ic].data[0], &readCfga[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN], ADBMS6830_REG_GROUP_DATA_LEN) != 0)
        {
    		ctx->commErrorCount++;
            return ADBMS6830_ERR_COMM;
        }
    }
#endif


    st = Adbms6830_WriteCfgb(ctx, hal, cmds);
    if (st != ADBMS6830_OK)
    {
    	if( st == ADBMS6830_ERR_PEC )
    	{
    		ctx->pecErrorCount++;
    	}
    	if( st == ADBMS6830_ERR_COMM )
    	{
    		ctx->commErrorCount++;
    	}
        return st;
    }

    st = Adbms6830_ReadRegisterGroup(hal, cmds->RDCFGB, readCfgb, ADBMS6830_REG_GROUP_DATA_LEN, ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        if (memcmp(&ctx->cfgb[ic].data[0], &readCfgb[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN], ADBMS6830_REG_GROUP_DATA_LEN) != 0)
        {
    		ctx->commErrorCount++;
            return ADBMS6830_ERR_COMM;
        }
    }

    (void)memset(clrFlag, 0, sizeof(clrFlag));
    clrFlag[0] = 0xFFU;
    clrFlag[1] = 0xFFU;
    clrFlag[4] = 0xFFU;
    clrFlag[5] = 0xFFU;

    {
        uint8_t tx[ADBMS6830_CMD_FRAME_SIZE + ADBMS6830_MAX_ICS * (ADBMS6830_BYTES_PER_CFGR + ADBMS6830_DATA_PEC_SIZE)];
        uint8_t rx[sizeof(tx)];
        uint16_t idx = 0U;
        int32_t chainIc;

        (void)Adbms6830_BuildCmdFrame(cmds->CLRFLAG, tx);
        idx = ADBMS6830_CMD_FRAME_SIZE;

        for (chainIc = (int32_t)ctx->icCount - 1; chainIc >= 0; chainIc--)
        {
            uint16_t pec;
            (void)memcpy(&tx[idx], clrFlag, ADBMS6830_BYTES_PER_CFGR);
            pec = Adbms_Pec10_Calc(clrFlag, (uint8_t)0, ADBMS6830_BYTES_PER_CFGR);
            idx += ADBMS6830_BYTES_PER_CFGR;
            tx[idx++] = (uint8_t)(pec >> 8U);
            tx[idx++] = (uint8_t)(pec & 0xFFU);
        }

#if 0
        if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
        {
            return ADBMS6830_ERR_COMM;
        }
#endif

        if (hal->spiTransfer(tx, rx, idx) != ADBMS6830_OK)
        {
            return ADBMS6830_ERR_COMM;
        }
    }

    st = Adbms6830_ReadRegisterGroup(hal, cmds->RDSTATC, readStatc, ADBMS6830_REG_GROUP_DATA_LEN, ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        if ((readStatc[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN + 0U] != 0U) ||
            (readStatc[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN + 1U] != 0U) ||
            (readStatc[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN + 4U] != 0U) ||
            (readStatc[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN + 5U] != 0U))
        {
            return ADBMS6830_ERR_COMM;
        }
    }

    st = Adbms6830_ReadRegisterGroup(hal, cmds->RDSID, readSid, ADBMS6830_REG_GROUP_DATA_LEN, ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        uint8_t byteIdx;
        bool sidIsAllZero = true;

        for (byteIdx = 0U; byteIdx < ADBMS6830_REG_GROUP_DATA_LEN; byteIdx++)
        {
        	ADBMS6830_DEBUG_PRINTF( "The SID %d = 0x%x ", byteIdx, readSid[byteIdx+ic*ADBMS6830_REG_GROUP_DATA_LEN] );
            if (readSid[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN + byteIdx] != 0U)
            {
                sidIsAllZero = false;
            }
        }

        ADBMS6830_DEBUG_PRINTF( "\r\n" );
        if (sidIsAllZero == true)
        {
            return ADBMS6830_ERR_COMM;
        }


    }

    st = Adbms6830_SendMute(hal, cmds);
    if (st != ADBMS6830_OK)
    {
        return st;
    }


#if 0
    st = Adbms6830_StartCellConversion(hal, cmds);
    if (st != ADBMS6830_OK)
    {
        return st;
    }


    ctx->configured = true;
    ctx->lastCommMs = ctx->tickMs;
    ctx->lastMeasureMs = ctx->tickMs;
    ctx->linkState = ADBMS6830_LINK_READY;
#else
    ctx->configured = true;
    ctx->lastCommMs = ctx->tickMs;
    ctx->linkState = ADBMS6830_LINK_READY;
#endif
    return ADBMS6830_OK;
}

Adbms6830_Status_t Adbms6830_SendMute(const Adbms6830_Hal_t *hal,
                                      const Adbms6830_CommandSet_t *cmds)
{
    if (cmds == NULL)
    {
        return ADBMS6830_ERR_PARAM;
    }

    return Adbms6830_SendCommandOnly(hal, cmds->MUTE);
}

Adbms6830_Status_t Adbms6830_SendUnmute(const Adbms6830_Hal_t *hal,
                                        const Adbms6830_CommandSet_t *cmds)
{
    if (cmds == NULL)
    {
        return ADBMS6830_ERR_PARAM;
    }

    return Adbms6830_SendCommandOnly(hal, cmds->UNMUTE);
}

Adbms6830_Status_t Adbms6830_Task(Adbms6830_Context_t *ctx,
                                  const Adbms6830_Hal_t *hal,
                                  const Adbms6830_CommandSet_t *cmds,
                                  uint32_t measurementPeriodMs)
{
    uint32_t timeSinceLastComm;
    uint32_t timeSinceLastMeasure;

    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    timeSinceLastComm    = ctx->tickMs - ctx->lastCommMs;
    timeSinceLastMeasure = ctx->tickMs - ctx->lastMeasureMs;

    switch (ctx->svcState)
    {
        case ADBMS6830_SVC_BOOT:
            ctx->configured   = false;
            ctx->linkState    = ADBMS6830_LINK_IDLE;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->svcState     = ADBMS6830_SVC_WAKE_STACK;
            break;

        case ADBMS6830_SVC_STANDBY:
            if (timeSinceLastMeasure >= measurementPeriodMs)
            {
                if (timeSinceLastComm >= ADBMS6830_SLEEP_WATCHDOG_TIMEOUT)
                {
                    /* Core likely entered sleep; CFGA/CFGB may be reset */
                    ctx->configured = false;
                    ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
                }
                else if (timeSinceLastComm >= ADBMS6830_LINK_IDLE_TIMEOUT)
                {
                    /* isoSPI idle only; config still valid */
                    ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
                }
                else
                {
                    /* Link and core are still ready */
                    ctx->svcState = ADBMS6830_SVC_START_MEASURE;
                }

                ctx->stateEntryMs = ctx->tickMs;
            }
            break;

        case ADBMS6830_SVC_WAKE_STACK:
            if (Adbms6830_WakeUp(ctx, hal) == ADBMS6830_OK)
            {
                ctx->linkState    = ADBMS6830_LINK_READY;
                ctx->lastCommMs   = ctx->tickMs;
                ctx->stateEntryMs = ctx->tickMs;

                if (ctx->configured == false)
                {
                    ctx->svcState = ADBMS6830_SVC_CONFIGURE;
                }
                else
                {
                    ctx->svcState = ADBMS6830_SVC_START_MEASURE;
                }
                ADBMS6830_DEBUG_PRINTF( "Start Wake @%d\r\n", ctx->tickMs );
            }
            else
            {
            	ADBMS6830_DEBUG_PRINTF( "********* Wake up failed\r\n" );
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState  = ADBMS6830_SVC_FAULT;
                return ADBMS6830_ERR_COMM;
            }
            break;

        case ADBMS6830_SVC_CONFIGURE:
            if (Adbms6830_FullInitialize(ctx, hal, cmds) == ADBMS6830_OK)
            {
                ctx->configured   = true;
                ctx->linkState    = ADBMS6830_LINK_READY;
                ctx->lastCommMs   = ctx->tickMs;
                ctx->stateEntryMs = ctx->tickMs;
                ctx->svcState     = ADBMS6830_SVC_START_MEASURE;
                ADBMS6830_DEBUG_PRINTF( "Start Configure @%d\r\n", ctx->tickMs );

            }
            else
            {
            	ADBMS6830_DEBUG_PRINTF( "********************Configuration is failed\r\n" );
                ctx->configured = false;
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState  = ADBMS6830_SVC_FAULT;
                return ADBMS6830_ERR_COMM;
            }
            break;

        case ADBMS6830_SVC_START_MEASURE:
            /*
             * Safety check: if this state is reached after a long delay,
             * re-route through wake/configure logic.
             */
            timeSinceLastComm = ctx->tickMs - ctx->lastCommMs;

            if (timeSinceLastComm >= ADBMS6830_SLEEP_WATCHDOG_TIMEOUT)
            {
                ctx->configured = false;
                ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
                break;
            }

            if (timeSinceLastComm >= ADBMS6830_LINK_IDLE_TIMEOUT)
            {
                ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
                break;
            }

            if (Adbms6830_StartCellConversion(hal, cmds) == ADBMS6830_OK)
            {
                ctx->linkState    = ADBMS6830_LINK_XFER;
                ctx->lastCommMs   = ctx->tickMs;
                ctx->stateEntryMs = ctx->tickMs;
                ctx->measureCount++;
                ctx->svcState     = ADBMS6830_SVC_WAIT_MEASURE;
                ADBMS6830_DEBUG_PRINTF( "Start Measure @%d\r\n", ctx->tickMs );
            }
            else
            {
            	ADBMS6830_DEBUG_PRINTF( "Failed to start measument ***********\r\n" );
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState  = ADBMS6830_SVC_FAULT;
                return ADBMS6830_ERR_COMM;
            }
            break;

        case ADBMS6830_SVC_WAIT_MEASURE:
            if ((ctx->tickMs - ctx->stateEntryMs) > ADBMS6830_MEASUREMENT_TIME )
            {
                /*
                 * Usually ADBMS6830_MEASUREMENT_TIME is less than tIDLE.
                 * If not, wake the isoSPI link before reading.
                 *
                 * NOTE:
                 * This assumes Adbms6830_WakeUp() does not disturb the completed
                 * conversion result.
                 */
                if ((ctx->tickMs - ctx->lastCommMs) >= ADBMS6830_LINK_IDLE_TIMEOUT)
                {
                    if (Adbms6830_WakeUp(ctx, hal) != ADBMS6830_OK)
                    {
                        ctx->commErrorCount++;
                        ctx->linkState = ADBMS6830_LINK_ERROR;
                        ctx->svcState  = ADBMS6830_SVC_FAULT;
                        return ADBMS6830_ERR_COMM;
                    }

                    ctx->linkState    = ADBMS6830_LINK_READY;
                    ctx->lastCommMs   = ctx->tickMs;
                    ctx->stateEntryMs = ctx->tickMs;
                }

                ctx->svcState = ADBMS6830_SVC_READ_RESULTS;
            }
            break;

        case ADBMS6830_SVC_READ_RESULTS:
            /*
             * Safety check again before read.
             */
            timeSinceLastComm = ctx->tickMs - ctx->lastCommMs;

            if (timeSinceLastComm >= ADBMS6830_SLEEP_WATCHDOG_TIMEOUT)
            {
                ctx->configured = false;
                ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
                break;
            }

            if (timeSinceLastComm >= ADBMS6830_LINK_IDLE_TIMEOUT)
            {
                if (Adbms6830_WakeUp(ctx, hal) != ADBMS6830_OK)
                {
                    ctx->commErrorCount++;
                    ctx->linkState = ADBMS6830_LINK_ERROR;
                    ctx->svcState  = ADBMS6830_SVC_FAULT;
                    ADBMS6830_DEBUG_PRINTF( "***&&Start wake line 796 @%d\r\n", ctx->tickMs );
                    return ADBMS6830_ERR_COMM;
                }

                ctx->linkState  = ADBMS6830_LINK_READY;
                ctx->lastCommMs = ctx->tickMs;
            }

            ADBMS6830_DEBUG_PRINTF( "Start Read Result @%d\r\n", ctx->tickMs );

#if ADBMS6830_USE_RDCVALL
            if (Adbms6830_ReadCellVoltagesAll(ctx, hal, cmds) == ADBMS6830_OK)
#else
            if (Adbms6830_ReadCellVoltagesByGroup(ctx, hal, cmds) == ADBMS6830_OK)
#endif
            {
                ctx->linkState     = ADBMS6830_LINK_READY;
                ctx->lastCommMs    = ctx->tickMs;
                ctx->lastMeasureMs = ctx->tickMs;
                ctx->svcState      = ADBMS6830_SVC_PROCESS_RESULTS;
                ADBMS6830_DEBUG_PRINTF( "Start Read @%d\r\n", ctx->tickMs );

            }
            else
            {
            	ADBMS6830_DEBUG_PRINTF( "****************** Read Result failed\r\n" );
            	hal->delayMs( 3000 );

                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState  = ADBMS6830_SVC_FAULT;
                return ADBMS6830_ERR_COMM;
            }
            break;

        case ADBMS6830_SVC_PROCESS_RESULTS:
            /*
             * Add application-level checks here if needed.
             * Example: voltage plausibility, PEC error handling, fault flags.
             */
            ctx->svcState = ADBMS6830_SVC_STANDBY;
            break;

        case ADBMS6830_SVC_SLEEP:
            /*
             * Software sleep state.
             * For next measurement, force full wake + reconfigure.
             */
            if (timeSinceLastMeasure >= measurementPeriodMs)
            {
                ctx->configured = false;
                ctx->svcState   = ADBMS6830_SVC_WAKE_STACK;
            }
            break;

        case ADBMS6830_SVC_FAULT:
            /*
             * Simple recovery policy:
             * force full reconfiguration after any fault.
             */
            ctx->configured   = false;
            ctx->linkState    = ADBMS6830_LINK_ERROR;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->svcState     = ADBMS6830_SVC_WAKE_STACK;
            break;

        default:
            ctx->configured = false;
            ctx->svcState   = ADBMS6830_SVC_FAULT;
            return ADBMS6830_ERR_STATE;;
    }

    return ADBMS6830_OK;
}

void Adbms6830_CfgbPackDccPlaceholder(uint16_t dccMask, uint8_t cfgb[ADBMS6830_BYTES_PER_CFGR])
{
    if (cfgb == NULL)
    {
        return;
    }

    cfgb[4] = (uint8_t)(dccMask & 0x00FFU);
    cfgb[5] = (uint8_t)((dccMask >> 8U) & 0x00FFU);
}
