#include "adbms6830_driver.h"
#include "adbms6830_reg.h"
#include "adbms6830_pec.h"

#include <string.h>

#define ADBMS6830_WAKE_DELAY_US      (500U)
#define ADBMS6830_LINK_IDLE_TIMEOUT  (5U)
#define ADBMS6830_REG_GROUP_DATA_LEN (6U)

static Adbms6830_Status_t Adbms6830_ReadRegisterGroup(const Adbms6830_Hal_t *hal,
                                                      uint16_t cmd,
                                                      uint8_t *rxData,
                                                      uint16_t dataLen,
                                                      uint8_t icCount);

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

    st = Adbms6830_WakeUp(hal);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    return hal->spiTransfer(tx, rx, ADBMS6830_CMD_FRAME_SIZE);
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

    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

    if (hal->spiTransfer(tx, rx, totalLen) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

    offset = ADBMS6830_CMD_FRAME_SIZE;

    for (ic = 0U; ic < icCount; ic++)
    {
        uint8_t rxIndex = (uint8_t)((icCount - 1U) - ic);

        if (Adbms6830_VerifyRxBlockPec10(&rx[offset], dataLen) == false)
        {
            return ADBMS6830_ERR_PEC;
        }

        (void)memcpy(&rxData[(uint16_t)rxIndex * dataLen], &rx[offset], dataLen);
        offset = (uint16_t)(offset + dataLen + ADBMS6830_DATA_PEC_SIZE);
    }

    return ADBMS6830_OK;
}

static void Adbms6830_ParseCellBlock(Adbms6830_CellData_t *cell, const uint8_t *block)
{
    uint8_t i;

    for (i = 0U; i < ADBMS6830_CELLS_PER_IC; i++)
    {
        uint16_t raw = (uint16_t)(((uint16_t)block[(uint16_t)(2U * i) + 1U] << 8U) |
                                   (uint16_t)block[(uint16_t)(2U * i)]);
        cell->raw[i] = raw;
        cell->mV[i]  = Adbms6830_RawTo_mV(raw);
    }
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
    cmds->RDSTATC = ADBMS6830_RDSTATC_REG;
    cmds->RDSID = ADBMS6830_RDSID_REG;
    cmds->SRST = ADBMS6830_SRST_REG;
    cmds->CLRFLAG = ADBMS6830_CLRFLAG_REG;
    cmds->RDCVALL = ADBMS6830_RDCVALL_REG;
    cmds->ADCV = (uint16_t)(ADBMS6830_ADCV_BASE_REG | ADBMS6830_ADCV_CONT_OPTION);
    cmds->MUTE = ADBMS6830_MUTE_REG;
    cmds->UNMUTE = ADBMS6830_UNMUTE_REG;
}

uint16_t Adbms6830_RawTo_mV(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * 15U) / 100U);
}

uint32_t Adbms6830_RawTo_uV(uint16_t raw)
{
    return (uint32_t)raw * 150U;
}

volatile static uint32_t ray = 0;
Adbms6830_Status_t Adbms6830_WakeUp(const Adbms6830_Hal_t *hal)
{
    uint8_t tx[2] = {0x00U, 0x00U};
    uint8_t rx[2];
    Adbms6830_Status_t st;

    if ((hal == NULL) || (hal->spiTransfer == NULL) || (hal->delayUs == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    ray = 1;
    st = hal->spiTransfer(tx, rx, 2U);
    if (st != ADBMS6830_OK)
    {
    	ray = 2;
        return st;
    }

    ray = 3;

    hal->delayUs(ADBMS6830_WAKE_DELAY_US);
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
        pec = Adbms_Pec10_Calc(ctx->cfga[ic].data, ic, ADBMS6830_BYTES_PER_CFGR);
        idx += ADBMS6830_BYTES_PER_CFGR;
        tx[idx++] = (uint8_t)(pec >> 8U);
        tx[idx++] = (uint8_t)(pec & 0xFFU);
    }

    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

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
        pec = Adbms_Pec10_Calc(ctx->cfgb[ic].data, ic, ADBMS6830_BYTES_PER_CFGR);
        idx += ADBMS6830_BYTES_PER_CFGR;
        tx[idx++] = (uint8_t)(pec >> 8U);
        tx[idx++] = (uint8_t)(pec & 0xFFU);
    }

    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

    return hal->spiTransfer(tx, rx, idx);
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

    if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
    {
        return ADBMS6830_ERR_COMM;
    }

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

    ctx->pecErrorCount = 0U;
    ctx->commErrorCount = 0U;

    st = Adbms6830_SendCommandOnly(hal, cmds->SRST);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    if (hal->delayMs != NULL)
    {
        hal->delayMs(1U);
    }

    st = Adbms6830_WriteCfga(ctx, hal, cmds);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    st = Adbms6830_ReadRegisterGroup(hal, cmds->RDCFGA, readCfga, ADBMS6830_REG_GROUP_DATA_LEN, ctx->icCount);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        if (memcmp(&ctx->cfga[ic].data[0], &readCfga[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN], ADBMS6830_REG_GROUP_DATA_LEN) != 0)
        {
            return ADBMS6830_ERR_COMM;
        }
    }

    st = Adbms6830_WriteCfgb(ctx, hal, cmds);
    if (st != ADBMS6830_OK)
    {
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
            pec = Adbms_Pec10_Calc(clrFlag, (uint8_t)chainIc, ADBMS6830_BYTES_PER_CFGR);
            idx += ADBMS6830_BYTES_PER_CFGR;
            tx[idx++] = (uint8_t)(pec >> 8U);
            tx[idx++] = (uint8_t)(pec & 0xFFU);
        }

        if (Adbms6830_WakeUp(hal) != ADBMS6830_OK)
        {
            return ADBMS6830_ERR_COMM;
        }

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
            if (readSid[(uint16_t)ic * ADBMS6830_REG_GROUP_DATA_LEN + byteIdx] != 0U)
            {
                sidIsAllZero = false;
                break;
            }
        }

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

    st = Adbms6830_StartCellConversion(hal, cmds);
    if (st != ADBMS6830_OK)
    {
        return st;
    }

    ctx->configured = true;
    ctx->lastCommMs = ctx->tickMs;
    ctx->lastMeasureMs = ctx->tickMs;
    ctx->linkState = ADBMS6830_LINK_READY;

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
                                  uint32_t measurementPeriodMs,
                                  uint32_t conversionDelayMs)
{
    if ((ctx == NULL) || (hal == NULL) || (cmds == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    switch (ctx->svcState)
    {
        case ADBMS6830_SVC_BOOT:
            ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
            break;

        case ADBMS6830_SVC_WAKE_STACK:
            if (Adbms6830_WakeUp(hal) == ADBMS6830_OK)
            {
                ctx->linkState = ADBMS6830_LINK_READY;
                ctx->lastCommMs = ctx->tickMs;
                ctx->stateEntryMs = ctx->tickMs;
                ctx->svcState = (ctx->configured == true) ? ADBMS6830_SVC_STANDBY : ADBMS6830_SVC_CONFIGURE;
            }
            else
            {
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState = ADBMS6830_SVC_FAULT;
                __debug();
            }
            break;

        case ADBMS6830_SVC_CONFIGURE:
            if (Adbms6830_FullInitialize(ctx, hal, cmds) == ADBMS6830_OK)
            {
                ctx->svcState = ADBMS6830_SVC_STANDBY;
            }
            else
            {
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState = ADBMS6830_SVC_FAULT;
                __debug();

            }
            break;

        case ADBMS6830_SVC_STANDBY:
            if ((ctx->tickMs - ctx->lastMeasureMs) >= measurementPeriodMs)
            {
                if ((ctx->tickMs - ctx->lastCommMs) > ADBMS6830_LINK_IDLE_TIMEOUT)
                {
                    ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
                }
                else
                {
                    ctx->svcState = ADBMS6830_SVC_START_MEASURE;
                }
            }
            break;

        case ADBMS6830_SVC_START_MEASURE:
            if (Adbms6830_StartCellConversion(hal, cmds) == ADBMS6830_OK)
            {
                ctx->linkState = ADBMS6830_LINK_XFER;
                ctx->lastCommMs = ctx->tickMs;
                ctx->stateEntryMs = ctx->tickMs;
                ctx->svcState = ADBMS6830_SVC_WAIT_MEASURE;
            }
            else
            {
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState = ADBMS6830_SVC_FAULT;
            }
            break;

        case ADBMS6830_SVC_WAIT_MEASURE:
            if ((ctx->tickMs - ctx->stateEntryMs) >= conversionDelayMs)
            {
                ctx->svcState = ADBMS6830_SVC_READ_RESULTS;
            }
            break;

        case ADBMS6830_SVC_READ_RESULTS:
            if (Adbms6830_ReadCellVoltagesAll(ctx, hal, cmds) == ADBMS6830_OK)
            {
                ctx->linkState = ADBMS6830_LINK_READY;
                ctx->lastCommMs = ctx->tickMs;
                ctx->svcState = ADBMS6830_SVC_PROCESS_RESULTS;
            }
            else
            {
                ctx->commErrorCount++;
                ctx->linkState = ADBMS6830_LINK_ERROR;
                ctx->svcState = ADBMS6830_SVC_FAULT;
            }
            break;

        case ADBMS6830_SVC_PROCESS_RESULTS:
            ctx->lastMeasureMs = ctx->tickMs;
            ctx->svcState = ADBMS6830_SVC_STANDBY;
            break;

        case ADBMS6830_SVC_SLEEP:
            break;

        case ADBMS6830_SVC_FAULT:
            ctx->svcState = ADBMS6830_SVC_WAKE_STACK;
            break;

        default:
            ctx->svcState = ADBMS6830_SVC_FAULT;
            break;
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
