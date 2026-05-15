#include "ltc6812_driver.h"
#include "ltc6812_pec.h"
#include "ltc6812_reg.h"
#include "shell.h"
#include <string.h>

#ifndef LTC6812_WAKE_THROUGH_ADBMS6822
#define LTC6812_WAKE_THROUGH_ADBMS6822     (1u)
#endif

#if LTC6812_WAKE_THROUGH_ADBMS6822
#define LTC6812_WAKE_SLEEP_BYTES           (3u)
#define LTC6812_WAKE_SLEEP_BYTE            (0x00u)
#define LTC6812_WAKE_SLEEP_POST_DELAY_US   (400u)
#else
#define LTC6812_WAKE_SLEEP_BYTES           (40u)
#define LTC6812_WAKE_SLEEP_BYTE            (0xFFu)
#define LTC6812_WAKE_SLEEP_POST_DELAY_US   (10u)
#endif
#define LTC6812_LINK_IDLE_TIMEOUT_MS       (5u)
#define LTC6812_SLEEP_WATCHDOG_TIMEOUT_MS  (2000u)
#define LTC6812_CELL_MEASUREMENT_TIME_MS   (8u)
#define LTC6812_AUX_MEASUREMENT_TIME_MS    (8u)
#define LTC6812_AUX_SAMPLE_PERIOD_MS       (2000u)
#define LTC6812_STCOMM_CLOCK_BYTES         (9u)
#define LTC6812_EEPROM_I2C_ADDR_BASE       (0x50u)
#define LTC6812_EEPROM_ODP_I2C_ADDR_BASE   (0x58u)  /* M24C08 ID page: 0x58<<1 = 0xB0 */
#define LTC6812_EEPROM_WRITE_DELAY_MS      (5u)
#define LTC6812_COMM_ICOM_START            (0x6u)
#define LTC6812_COMM_ICOM_STOP             (0x1u)
#define LTC6812_COMM_ICOM_BLANK            (0x0u)
#define LTC6812_COMM_ICOM_NO_TX            (0x7u)
#define LTC6812_COMM_FCOM_ACK              (0x0u)
#define LTC6812_COMM_FCOM_NACK             (0x8u)
#define LTC6812_COMM_FCOM_NACK_STOP        (0x9u)
#define LTC6812_BOARD_LED_IC               (1u)
#define LTC6812_BOARD_LED_GPIO             (5u)

#ifndef LTC6812_BALANCE_DCTO_MINUTES_CODE
#define LTC6812_BALANCE_DCTO_MINUTES_CODE     (1u)
#endif

static Ltc6812_Status_t Ltc6812_BuildCmdFrame(uint16_t cmd, uint8_t frame[LTC6812_CMD_FRAME_SIZE])
{
    uint16_t pec;

    if (frame == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    frame[0] = (uint8_t)(cmd >> 8u);
    frame[1] = (uint8_t)(cmd & 0xFFu);
    pec = Ltc6812_Pec15Calc(frame, 2u);
    frame[2] = (uint8_t)(pec >> 8u);
    frame[3] = (uint8_t)(pec & 0xFFu);

    return LTC6812_OK;
}

static Ltc6812_Status_t Ltc6812_SendCommandOnly(const Ltc6812_Hal_t *hal, uint16_t cmd)
{
    uint8_t tx[LTC6812_CMD_FRAME_SIZE];
    uint8_t rx[LTC6812_CMD_FRAME_SIZE];

    if ((hal == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    (void)Ltc6812_BuildCmdFrame(cmd, tx);

    return hal->spiTransfer(tx, rx, LTC6812_CMD_FRAME_SIZE);
}

static bool Ltc6812_VerifyRxBlockPec15(const uint8_t *payload, uint16_t dataLen)
{
    uint16_t rxPec;
    uint16_t calcPec;

    if (payload == 0)
    {
        return false;
    }

    rxPec = (uint16_t)(((uint16_t)payload[dataLen] << 8u) | payload[dataLen + 1u]);
    calcPec = Ltc6812_Pec15Calc(payload, dataLen);

    return (rxPec == calcPec);
}

static void Ltc6812_CaptureRxPec(const uint8_t *payload,
                                 uint16_t dataLen,
                                 uint16_t *rxPec,
                                 uint16_t *calcPec)
{
    if ((payload == 0) || (rxPec == 0) || (calcPec == 0))
    {
        return;
    }

    *rxPec = (uint16_t)(((uint16_t)payload[dataLen] << 8u) | payload[dataLen + 1u]);
    *calcPec = Ltc6812_Pec15Calc(payload, dataLen);
}

static Ltc6812_Status_t Ltc6812_ReadRegisterGroup(const Ltc6812_Hal_t *hal,
                                                  uint16_t cmd,
                                                  uint8_t *rxData,
                                                  uint16_t dataLen,
                                                  uint8_t icCount,
                                                  Ltc6812_Context_t *ctx,
                                                  uint8_t group)
{
    uint16_t totalLen;
    uint16_t offset;
    uint8_t ic;
    uint8_t tx[LTC6812_CMD_FRAME_SIZE + LTC6812_MAX_ICS * (LTC6812_REG_GROUP_DATA_LEN + LTC6812_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];

    if ((hal == 0) || (hal->spiTransfer == 0) || (rxData == 0) ||
        (dataLen != LTC6812_REG_GROUP_DATA_LEN) || (icCount == 0u) || (icCount > LTC6812_MAX_ICS))
    {
        return LTC6812_ERR_PARAM;
    }

    totalLen = (uint16_t)(LTC6812_CMD_FRAME_SIZE + ((uint16_t)icCount * (dataLen + LTC6812_DATA_PEC_SIZE)));
    (void)memset(tx, 0, totalLen);
    (void)Ltc6812_BuildCmdFrame(cmd, tx);

    if (hal->spiTransfer(tx, rx, totalLen) != LTC6812_OK)
    {
        if (ctx != 0)
        {
            ctx->lastDiagStep = LTC6812_DIAG_RDCV;
            ctx->lastDiagCmd = cmd;
            ctx->lastDiagGroup = group;
        }
        return LTC6812_ERR_COMM;
    }


    offset = LTC6812_CMD_FRAME_SIZE;
    for (ic = 0u; ic < icCount; ic++)
    {
        if (Ltc6812_VerifyRxBlockPec15(&rx[offset], dataLen) == false)
        {
            if (ctx != 0)
            {
                ctx->lastDiagStep = LTC6812_DIAG_RDCV_PEC;
                ctx->lastDiagCmd = cmd;
                ctx->lastDiagIc = ic;
                ctx->lastDiagGroup = group;
                Ltc6812_CaptureRxPec(&rx[offset], dataLen, &ctx->lastRxPec, &ctx->lastCalcPec);
            }
            return LTC6812_ERR_PEC;
        }

        (void)memcpy(&rxData[(uint16_t)ic * dataLen], &rx[offset], dataLen);
        if (ctx != 0)
        {
            Ltc6812_CaptureRxPec(&rx[offset], dataLen, &ctx->lastRxPec, &ctx->lastCalcPec);
        }
        offset = (uint16_t)(offset + dataLen + LTC6812_DATA_PEC_SIZE);
    }


    return LTC6812_OK;
}

static void Ltc6812_ParseCellGroup(Ltc6812_CellData_t *cell,
                                   const uint8_t *block,
                                   uint8_t startCellIndex)
{
    uint8_t i;

    if ((cell == 0) || (block == 0))
    {
        return;
    }

    for (i = 0u; i < 3u; i++)
    {
        uint8_t cellIndex = (uint8_t)(startCellIndex + i);
        uint16_t raw = (uint16_t)(((uint16_t)block[(uint16_t)(2u * i) + 1u] << 8u) |
                                  block[(uint16_t)(2u * i)]);

        if (cellIndex < LTC6812_CELLS_PER_IC)
        {
            cell->raw[cellIndex] = raw;
            cell->mV[cellIndex] = Ltc6812_RawTo_mV(raw);
        }
    }
}

static uint16_t Ltc6812_ParseRaw16(const uint8_t *block, uint8_t index)
{
    return (uint16_t)(((uint16_t)block[(uint16_t)(2u * index) + 1u] << 8u) |
                      (uint16_t)block[(uint16_t)(2u * index)]);
}

static void Ltc6812_ParseAuxGroup(Ltc6812_AuxData_t *aux,
                                  const uint8_t *block,
                                  uint8_t group)
{
    uint8_t i;

    if ((aux == 0) || (block == 0))
    {
        return;
    }

    for (i = 0u; i < 3u; i++)
    {
        uint16_t raw = Ltc6812_ParseRaw16(block, i);
        uint8_t auxIndex = 0xFFu;

        if (group == 0u)
        {
            auxIndex = i;
        }
        else if (group == 1u)
        {
            if (i < 2u)
            {
                auxIndex = (uint8_t)(3u + i);
            }
            else
            {
                aux->vref2Raw = raw;
                aux->vref2mV = Ltc6812_RawTo_mV(raw);
            }
        }
        else if (group == 2u)
        {
            auxIndex = (uint8_t)(5u + i);
        }
        else if ((group == 3u) && (i == 0u))
        {
            auxIndex = 8u;
        }

        if (auxIndex < LTC6812_AUX_CHANNELS_PER_IC)
        {
            aux->raw[auxIndex] = raw;
            aux->mV[auxIndex] = Ltc6812_RawTo_mV(raw);
        }
    }
}

static uint16_t Ltc6812_ScRawToSumRaw_0p01V(uint16_t scRaw)
{
    return (uint16_t)(((uint32_t)scRaw * 3u + 5u) / 10u);
}

static int16_t Ltc6812_ItmpRawToTempRaw_0p01C(uint16_t itmpRaw)
{
    int32_t tempRaw_0p01C = (((int32_t)itmpRaw * 100) / 76) - 27600;

    if (tempRaw_0p01C > 32767)
    {
        return 32767;
    }
    if (tempRaw_0p01C < -32768)
    {
        return -32768;
    }
    return (int16_t)tempRaw_0p01C;
}

static void Ltc6812_ParseStatusGroups(Ltc6812_StatusData_t *status,
                                      const uint8_t *stata,
                                      const uint8_t *statb)
{
    if ((status == 0) || (stata == 0) || (statb == 0))
    {
        return;
    }

    status->scRaw = Ltc6812_ParseRaw16(stata, 0u);
    status->itmpRaw = Ltc6812_ParseRaw16(stata, 1u);
    status->vaRaw = Ltc6812_ParseRaw16(stata, 2u);
    status->vdRaw = Ltc6812_ParseRaw16(statb, 0u);
    status->sumCellRaw_0p01V = Ltc6812_ScRawToSumRaw_0p01V(status->scRaw);
    status->internalTempRaw_0p01C = Ltc6812_ItmpRawToTempRaw_0p01C(status->itmpRaw);
    status->valid = true;
}

static void Ltc6812_PackCommByte(uint8_t icom, uint8_t data, uint8_t fcom, uint8_t *commHi, uint8_t *commLo)
{
    if ((commHi == 0) || (commLo == 0))
    {
        return;
    }

    *commHi = (uint8_t)(((icom & 0x0Fu) << 4u) | ((data >> 4u) & 0x0Fu));
    *commLo = (uint8_t)((data << 4u) | (fcom & 0x0Fu));
}

static uint8_t Ltc6812_UnpackCommData(const uint8_t *comm, uint8_t byteIndex)
{
    uint16_t offset = (uint16_t)byteIndex * 2u;

    return (uint8_t)(((comm[offset] & 0x0Fu) << 4u) | ((comm[offset + 1u] >> 4u) & 0x0Fu));
}

static void Ltc6812_FillNoTransmitComm(uint8_t comm[LTC6812_COMM_BYTES_PER_IC])
{
    uint8_t byteIndex;

    if (comm == 0)
    {
        return;
    }

    for (byteIndex = 0u; byteIndex < 3u; byteIndex++)
    {
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_NO_TX, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP,
                             &comm[(uint16_t)byteIndex * 2u],
                             &comm[(uint16_t)byteIndex * 2u + 1u]);
    }
}

void Ltc6812_Init(Ltc6812_Context_t *ctx, uint8_t icCount)
{
    if (ctx == 0)
    {
        return;
    }

    (void)memset(ctx, 0, sizeof(*ctx));

    if (icCount > LTC6812_MAX_ICS)
    {
        icCount = LTC6812_MAX_ICS;
    }

    ctx->icCount = icCount;
    ctx->linkState = LTC6812_LINK_OFF;
    ctx->svcState = LTC6812_SVC_BOOT;
}

void Ltc6812_SetDefaultCommands(Ltc6812_CommandSet_t *cmds)
{
    if (cmds == 0)
    {
        return;
    }

    cmds->WRCFGA = LTC6812_WRCFGA_REG;
    cmds->WRCFGB = LTC6812_WRCFGB_REG;
    cmds->RDCFGA = LTC6812_RDCFGA_REG;
    cmds->RDCFGB = LTC6812_RDCFGB_REG;
    cmds->WRCOMM = LTC6812_WRCOMM_REG;
    cmds->RDCOMM = LTC6812_RDCOMM_REG;
    cmds->STCOMM = LTC6812_STCOMM_REG;
    cmds->RDCVA = LTC6812_RDCVA_REG;
    cmds->RDCVB = LTC6812_RDCVB_REG;
    cmds->RDCVC = LTC6812_RDCVC_REG;
    cmds->RDCVD = LTC6812_RDCVD_REG;
    cmds->RDCVE = LTC6812_RDCVE_REG;
    cmds->ADCV = LTC6812_ADCV_BASE_REG;
    cmds->ADAX = LTC6812_ADAX_BASE_REG;
    cmds->ADSTAT = LTC6812_ADSTAT_BASE_REG;
    cmds->RDAUXA = LTC6812_RDAUXA_REG;
    cmds->RDAUXB = LTC6812_RDAUXB_REG;
    cmds->RDAUXC = LTC6812_RDAUXC_REG;
    cmds->RDAUXD = LTC6812_RDAUXD_REG;
    cmds->RDSTATA = LTC6812_RDSTATA_REG;
    cmds->RDSTATB = LTC6812_RDSTATB_REG;
    cmds->CLRCELL = LTC6812_CLRCELL_REG;
    cmds->MUTE = LTC6812_MUTE_REG;
    cmds->UNMUTE = LTC6812_UNMUTE_REG;
}

uint16_t Ltc6812_RawTo_mV(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw + 5u) / 10u);
}

uint32_t Ltc6812_RawTo_uV(uint16_t raw)
{
    return (uint32_t)raw * 100u;
}

Ltc6812_Status_t Ltc6812_WakeUp(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal)
{
    uint8_t ic;
    uint8_t byteIdx;
    uint8_t tx[LTC6812_WAKE_SLEEP_BYTES];
    uint8_t rx[LTC6812_WAKE_SLEEP_BYTES];

    if ((ctx == 0) || (hal == 0) || (hal->spiTransfer == 0) || (hal->delayUs == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    for (byteIdx = 0u; byteIdx < LTC6812_WAKE_SLEEP_BYTES; byteIdx++)
    {
        tx[byteIdx] = LTC6812_WAKE_SLEEP_BYTE;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        Ltc6812_Status_t st;

        st = hal->spiTransfer(tx, rx, LTC6812_WAKE_SLEEP_BYTES);
        ctx->lastDiagStep = LTC6812_DIAG_WAKE;
        ctx->lastDiagIc = ic;
        if (st != LTC6812_OK)
        {
            return st;
        }

        hal->delayUs(LTC6812_WAKE_SLEEP_POST_DELAY_US);
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_StartCellConversion(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    if (cmds == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    return Ltc6812_SendCommandOnly(hal, cmds->ADCV);
}

Ltc6812_Status_t Ltc6812_StartAuxConversion(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    if (cmds == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    return Ltc6812_SendCommandOnly(hal, cmds->ADAX);
}

Ltc6812_Status_t Ltc6812_StartStatusConversion(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    if (cmds == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    return Ltc6812_SendCommandOnly(hal, cmds->ADSTAT);
}

Ltc6812_Status_t Ltc6812_WriteCfga(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint8_t tx[LTC6812_CMD_FRAME_SIZE + LTC6812_MAX_ICS * (LTC6812_BYTES_PER_CFGR + LTC6812_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t idx;
    int32_t ic;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    (void)Ltc6812_BuildCmdFrame(cmds->WRCFGA, tx);
    ctx->lastDiagStep = LTC6812_DIAG_WRCFGA;
    ctx->lastDiagCmd = cmds->WRCFGA;
    idx = LTC6812_CMD_FRAME_SIZE;

    for (ic = (int32_t)ctx->icCount - 1; ic >= 0; ic--)
    {
        uint16_t pec;

        (void)memcpy(&tx[idx], ctx->cfga[ic].data, LTC6812_BYTES_PER_CFGR);
        pec = Ltc6812_Pec15Calc(ctx->cfga[ic].data, LTC6812_BYTES_PER_CFGR);
        idx = (uint16_t)(idx + LTC6812_BYTES_PER_CFGR);
        tx[idx++] = (uint8_t)(pec >> 8u);
        tx[idx++] = (uint8_t)(pec & 0xFFu);
    }

    return hal->spiTransfer(tx, rx, idx);
}

Ltc6812_Status_t Ltc6812_WriteCfgb(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint8_t tx[LTC6812_CMD_FRAME_SIZE + LTC6812_MAX_ICS * (LTC6812_BYTES_PER_CFGR + LTC6812_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t idx;
    int32_t ic;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    (void)Ltc6812_BuildCmdFrame(cmds->WRCFGB, tx);
    ctx->lastDiagStep = LTC6812_DIAG_WRCFGB;
    ctx->lastDiagCmd = cmds->WRCFGB;
    idx = LTC6812_CMD_FRAME_SIZE;

    for (ic = (int32_t)ctx->icCount - 1; ic >= 0; ic--)
    {
        uint16_t pec;

        (void)memcpy(&tx[idx], ctx->cfgb[ic].data, LTC6812_BYTES_PER_CFGR);
        pec = Ltc6812_Pec15Calc(ctx->cfgb[ic].data, LTC6812_BYTES_PER_CFGR);
        idx = (uint16_t)(idx + LTC6812_BYTES_PER_CFGR);
        tx[idx++] = (uint8_t)(pec >> 8u);
        tx[idx++] = (uint8_t)(pec & 0xFFu);
    }

    return hal->spiTransfer(tx, rx, idx);
}

Ltc6812_Status_t Ltc6812_ReadCfga(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint8_t readCfga[LTC6812_MAX_ICS * LTC6812_REG_GROUP_DATA_LEN];
    uint8_t ic;
    Ltc6812_Status_t st;

    if ((ctx == 0) || (hal == 0) || (cmds == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    st = Ltc6812_ReadRegisterGroup(hal, cmds->RDCFGA, readCfga, LTC6812_REG_GROUP_DATA_LEN, ctx->icCount, ctx, 0u);
    if (st != LTC6812_OK)
    {
        return st;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        (void)memcpy(ctx->cfga[ic].data,
                     &readCfga[(uint16_t)ic * LTC6812_REG_GROUP_DATA_LEN],
                     LTC6812_REG_GROUP_DATA_LEN);
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_ReadCfgb(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint8_t readCfgb[LTC6812_MAX_ICS * LTC6812_REG_GROUP_DATA_LEN];
    uint8_t ic;
    Ltc6812_Status_t st;

    if ((ctx == 0) || (hal == 0) || (cmds == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    st = Ltc6812_ReadRegisterGroup(hal, cmds->RDCFGB, readCfgb, LTC6812_REG_GROUP_DATA_LEN, ctx->icCount, ctx, 0u);
    if (st != LTC6812_OK)
    {
        return st;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        (void)memcpy(ctx->cfgb[ic].data,
                     &readCfgb[(uint16_t)ic * LTC6812_REG_GROUP_DATA_LEN],
                     LTC6812_REG_GROUP_DATA_LEN);
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_ReadCellVoltagesByGroup(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    static const uint8_t groupStartIndex[LTC6812_CELL_GROUP_COUNT] = { 0u, 3u, 6u, 9u, 12u };
    uint16_t groupCmds[LTC6812_CELL_GROUP_COUNT];
    uint8_t groupData[LTC6812_MAX_ICS * LTC6812_CELL_GROUP_DATA_BYTES];
    uint8_t group;
    uint8_t ic;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    groupCmds[0] = cmds->RDCVA;
    groupCmds[1] = cmds->RDCVB;
    groupCmds[2] = cmds->RDCVC;
    groupCmds[3] = cmds->RDCVD;
    groupCmds[4] = cmds->RDCVE;

    for (group = 0u; group < LTC6812_CELL_GROUP_COUNT; group++)
    {
        Ltc6812_Status_t st = Ltc6812_ReadRegisterGroup(hal,
                                                        groupCmds[group],
                                                        groupData,
                                                        LTC6812_CELL_GROUP_DATA_BYTES,
                                                        ctx->icCount,
                                                        ctx,
                                                        group);
        if (st != LTC6812_OK)
        {
            if (st == LTC6812_ERR_PEC)
            {
                ctx->pecErrorCount++;
            }
            return st;
        }

        for (ic = 0u; ic < ctx->icCount; ic++)
        {
            Ltc6812_ParseCellGroup(&ctx->cell[ic],
                                   &groupData[(uint16_t)ic * LTC6812_CELL_GROUP_DATA_BYTES],
                                   groupStartIndex[group]);
        }

        ctx->lastDiagStep = LTC6812_DIAG_RDCV;
        ctx->lastDiagCmd = groupCmds[group];
        ctx->lastDiagGroup = group;
    }

    ctx->lastRawCell0 = ctx->cell[0].raw[0];
    ctx->lastRawCell1 = ctx->cell[0].raw[1];
    ctx->lastRawCell2 = ctx->cell[0].raw[2];

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_ReadAuxVoltagesByGroup(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint16_t groupCmds[LTC6812_AUX_GROUP_COUNT];
    uint8_t groupData[LTC6812_MAX_ICS * LTC6812_CELL_GROUP_DATA_BYTES];
    uint8_t group;
    uint8_t ic;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    groupCmds[0] = cmds->RDAUXA;
    groupCmds[1] = cmds->RDAUXB;
    groupCmds[2] = cmds->RDAUXC;
    groupCmds[3] = cmds->RDAUXD;

    for (group = 0u; group < LTC6812_AUX_GROUP_COUNT; group++)
    {
        Ltc6812_Status_t st = Ltc6812_ReadRegisterGroup(hal,
                                                        groupCmds[group],
                                                        groupData,
                                                        LTC6812_CELL_GROUP_DATA_BYTES,
                                                        ctx->icCount,
                                                        ctx,
                                                        group);
        if (st != LTC6812_OK)
        {
            if (st == LTC6812_ERR_PEC)
            {
                ctx->pecErrorCount++;
                ctx->lastDiagStep = LTC6812_DIAG_RDAUX_PEC;
            }
            return st;
        }

        for (ic = 0u; ic < ctx->icCount; ic++)
        {
            Ltc6812_ParseAuxGroup(&ctx->aux[ic],
                                  &groupData[(uint16_t)ic * LTC6812_CELL_GROUP_DATA_BYTES],
                                  group);
        }

        ctx->lastDiagStep = LTC6812_DIAG_RDAUX;
        ctx->lastDiagCmd = groupCmds[group];
        ctx->lastDiagGroup = group;
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_ReadStatus(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint8_t stata[LTC6812_MAX_ICS * LTC6812_REG_GROUP_DATA_LEN];
    uint8_t statb[LTC6812_MAX_ICS * LTC6812_REG_GROUP_DATA_LEN];
    uint8_t ic;
    Ltc6812_Status_t st;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    st = Ltc6812_ReadRegisterGroup(hal,
                                   cmds->RDSTATA,
                                   stata,
                                   LTC6812_REG_GROUP_DATA_LEN,
                                   ctx->icCount,
                                   ctx,
                                   0u);
    if (st != LTC6812_OK)
    {
        if (st == LTC6812_ERR_PEC)
        {
            ctx->pecErrorCount++;
            ctx->lastDiagStep = LTC6812_DIAG_RDSTAT_PEC;
        }
        return st;
    }

    st = Ltc6812_ReadRegisterGroup(hal,
                                   cmds->RDSTATB,
                                   statb,
                                   LTC6812_REG_GROUP_DATA_LEN,
                                   ctx->icCount,
                                   ctx,
                                   1u);
    if (st != LTC6812_OK)
    {
        if (st == LTC6812_ERR_PEC)
        {
            ctx->pecErrorCount++;
            ctx->lastDiagStep = LTC6812_DIAG_RDSTAT_PEC;
        }
        return st;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        Ltc6812_ParseStatusGroups(&ctx->status[ic],
                                  &stata[(uint16_t)ic * LTC6812_REG_GROUP_DATA_LEN],
                                  &statb[(uint16_t)ic * LTC6812_REG_GROUP_DATA_LEN]);
    }

    ctx->lastDiagStep = LTC6812_DIAG_RDSTAT;
    ctx->lastDiagCmd = cmds->RDSTATB;
    ctx->lastDiagGroup = 1u;

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_FullInitialize(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    uint8_t readCfga[LTC6812_MAX_ICS * LTC6812_REG_GROUP_DATA_LEN];
    uint8_t readCfgb[LTC6812_MAX_ICS * LTC6812_REG_GROUP_DATA_LEN];
    uint8_t ic;
    Ltc6812_Status_t st;

    if ((ctx == 0) || (hal == 0) || (cmds == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    (void)Ltc6812_SetBoardLed(ctx, hal, cmds, true);

	st = Ltc6812_WriteCfga(ctx, hal, cmds);
	if (st != LTC6812_OK)
	{
		return st;
	}



	ctx->lastDiagStep = LTC6812_DIAG_RDCFGA;
	ctx->lastDiagCmd = cmds->RDCFGA;
	st = Ltc6812_ReadRegisterGroup(hal, cmds->RDCFGA, readCfga, LTC6812_REG_GROUP_DATA_LEN, ctx->icCount, ctx, 0u);
	if (st != LTC6812_OK)
	{
		if (st == LTC6812_ERR_PEC)
		{
			ctx->pecErrorCount++;
		}
		else if (st == LTC6812_ERR_COMM)
		{
			ctx->commErrorCount++;
		}
		return st;
	}


	for (ic = 0u; ic < ctx->icCount; ic++)
	{
#if 0
		if (memcmp(ctx->cfga[ic].data, &readCfga[(uint16_t)ic * LTC6812_REG_GROUP_DATA_LEN], LTC6812_REG_GROUP_DATA_LEN) != 0)
		{
			ctx->commErrorCount++;
			ctx->lastDiagStep = LTC6812_DIAG_CFGA_COMPARE;
			return LTC6812_ERR_COMM;
		}
		for( int idx = 0; idx < 6; idx++ )
		{
			PRINTF( "The tx cfga[%d] = 0x%x, the rx cfga[%d] = 0x%x\r\n", idx, ctx->cfga[ic].data[idx], idx, readCfga[idx] );
		}
#endif

	}


    st = Ltc6812_WriteCfgb(ctx, hal, cmds);
    if (st != LTC6812_OK)
    {
        return st;
    }

    ctx->lastDiagStep = LTC6812_DIAG_RDCFGB;
    ctx->lastDiagCmd = cmds->RDCFGB;
    st = Ltc6812_ReadRegisterGroup(hal, cmds->RDCFGB, readCfgb, LTC6812_REG_GROUP_DATA_LEN, ctx->icCount, ctx, 0u);
    if (st != LTC6812_OK)
    {
        if (st == LTC6812_ERR_PEC)
        {
            ctx->pecErrorCount++;
        }
        else if (st == LTC6812_ERR_COMM)
        {
            ctx->commErrorCount++;
        }
        return st;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
#if 0
        if (memcmp(ctx->cfgb[ic].data, &readCfgb[(uint16_t)ic * LTC6812_REG_GROUP_DATA_LEN], LTC6812_REG_GROUP_DATA_LEN) != 0)
        {
            ctx->commErrorCount++;
            ctx->lastDiagStep = LTC6812_DIAG_CFGB_COMPARE;
            return LTC6812_ERR_COMM;
        }
#else

#endif
    }

    ctx->configured = true;
    ctx->linkState = LTC6812_LINK_READY;
    ctx->lastCommMs = ctx->tickMs;

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_SendMute(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    if (cmds == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    return Ltc6812_SendCommandOnly(hal, cmds->MUTE);
}

Ltc6812_Status_t Ltc6812_SendUnmute(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds)
{
    if (cmds == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    return Ltc6812_SendCommandOnly(hal, cmds->UNMUTE);
}

static Ltc6812_Status_t Ltc6812_WriteComm(Ltc6812_Context_t *ctx,
                                          const Ltc6812_Hal_t *hal,
                                          const Ltc6812_CommandSet_t *cmds,
                                          const uint8_t comm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC])
{
    uint8_t tx[LTC6812_CMD_FRAME_SIZE + LTC6812_MAX_ICS * (LTC6812_COMM_BYTES_PER_IC + LTC6812_DATA_PEC_SIZE)];
    uint8_t rx[sizeof(tx)];
    uint16_t idx;
    int32_t ic;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (comm == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    (void)Ltc6812_BuildCmdFrame(cmds->WRCOMM, tx);
    ctx->lastDiagStep = LTC6812_DIAG_WRCOMM;
    ctx->lastDiagCmd = cmds->WRCOMM;
    idx = LTC6812_CMD_FRAME_SIZE;

    for (ic = (int32_t)ctx->icCount - 1; ic >= 0; ic--)
    {
        uint16_t pec;

        (void)memcpy(&tx[idx], comm[ic], LTC6812_COMM_BYTES_PER_IC);
        pec = Ltc6812_Pec15Calc(comm[ic], LTC6812_COMM_BYTES_PER_IC);
        idx = (uint16_t)(idx + LTC6812_COMM_BYTES_PER_IC);
        tx[idx++] = (uint8_t)(pec >> 8u);
        tx[idx++] = (uint8_t)(pec & 0xFFu);
    }

    return hal->spiTransfer(tx, rx, idx);
}

static Ltc6812_Status_t Ltc6812_StartComm(Ltc6812_Context_t *ctx,
                                          const Ltc6812_Hal_t *hal,
                                          const Ltc6812_CommandSet_t *cmds)
{
    uint8_t tx[LTC6812_CMD_FRAME_SIZE + LTC6812_STCOMM_CLOCK_BYTES];
    uint8_t rx[sizeof(tx)];
    uint8_t i;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (hal->spiTransfer == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    (void)Ltc6812_BuildCmdFrame(cmds->STCOMM, tx);
    for (i = LTC6812_CMD_FRAME_SIZE; i < sizeof(tx); i++)
    {
        tx[i] = 0xFFu;
    }

    ctx->lastDiagStep = LTC6812_DIAG_STCOMM;
    ctx->lastDiagCmd = cmds->STCOMM;

    return hal->spiTransfer(tx, rx, (uint16_t)sizeof(tx));
}

static Ltc6812_Status_t Ltc6812_ReadComm(Ltc6812_Context_t *ctx,
                                         const Ltc6812_Hal_t *hal,
                                         const Ltc6812_CommandSet_t *cmds,
                                         uint8_t comm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC])
{
    Ltc6812_Status_t st;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (comm == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    ctx->lastDiagStep = LTC6812_DIAG_RDCOMM;
    ctx->lastDiagCmd = cmds->RDCOMM;
    st = Ltc6812_ReadRegisterGroup(hal,
                                   cmds->RDCOMM,
                                   (uint8_t *)comm,
                                   LTC6812_COMM_BYTES_PER_IC,
                                   ctx->icCount,
                                   ctx,
                                   0u);
    if (st == LTC6812_ERR_PEC)
    {
        ctx->pecErrorCount++;
    }

    return st;
}

static Ltc6812_Status_t Ltc6812_RunComm(Ltc6812_Context_t *ctx,
                                        const Ltc6812_Hal_t *hal,
                                        const Ltc6812_CommandSet_t *cmds,
                                        const uint8_t txComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC],
                                        uint8_t rxComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC])
{
    Ltc6812_Status_t st;

    st = Ltc6812_WriteComm(ctx, hal, cmds, txComm);
    if (st != LTC6812_OK)
    {
        return st;
    }

    st = Ltc6812_StartComm(ctx, hal, cmds);
    if (st != LTC6812_OK)
    {
        return st;
    }

    return Ltc6812_ReadComm(ctx, hal, cmds, rxComm);
}

static uint8_t Ltc6812_EepromControlByte(uint16_t address, bool read)
{
    uint8_t block = (uint8_t)((address >> 8u) & 0x03u);
    uint8_t control = (uint8_t)(((LTC6812_EEPROM_I2C_ADDR_BASE | block) << 1u) & 0xFEu);

    if (read == true)
    {
        control |= 0x01u;
    }

    return control;
}

static void Ltc6812_PrepareCommForIc(uint8_t comm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC],
                                     uint8_t icCount,
                                     uint8_t activeIc)
{
    uint8_t ic;

    for (ic = 0u; ic < LTC6812_MAX_ICS; ic++)
    {
        Ltc6812_FillNoTransmitComm(comm[ic]);
    }

    if ((activeIc < icCount) && (activeIc < LTC6812_MAX_ICS))
    {
        (void)memset(comm[activeIc], 0, LTC6812_COMM_BYTES_PER_IC);
    }
}

Ltc6812_Status_t Ltc6812_EepromRead(Ltc6812_Context_t *ctx,
                                    const Ltc6812_Hal_t *hal,
                                    const Ltc6812_CommandSet_t *cmds,
                                    uint16_t address,
                                    uint8_t *data,
                                    uint16_t len)
{
    uint16_t pos;
    uint8_t txComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    uint8_t rxComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (data == 0) || (len == 0u) || (ctx->icCount == 0u))
    {
        return LTC6812_ERR_PARAM;
    }

    for (pos = 0u; pos < len; pos++)
    {
        Ltc6812_Status_t st;
        uint16_t eepromAddr = (uint16_t)(address + pos);

        Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, Ltc6812_EepromControlByte(eepromAddr, false), LTC6812_COMM_FCOM_ACK, &txComm[0][0], &txComm[0][1]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, (uint8_t)(eepromAddr & 0xFFu), LTC6812_COMM_FCOM_ACK, &txComm[0][2], &txComm[0][3]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, Ltc6812_EepromControlByte(eepromAddr, true), LTC6812_COMM_FCOM_ACK, &txComm[0][4], &txComm[0][5]);

        st = Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);
        if (st != LTC6812_OK)
        {
            return st;
        }

        Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][0], &txComm[0][1]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_NO_TX, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][2], &txComm[0][3]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_NO_TX, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][4], &txComm[0][5]);

        st = Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);
        if (st != LTC6812_OK)
        {
            return st;
        }

        data[pos] = Ltc6812_UnpackCommData(rxComm[0], 0u);
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_EepromWrite(Ltc6812_Context_t *ctx,
                                     const Ltc6812_Hal_t *hal,
                                     const Ltc6812_CommandSet_t *cmds,
                                     uint16_t address,
                                     const uint8_t *data,
                                     uint16_t len)
{
    uint16_t pos = 0u;
    uint8_t txComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    uint8_t rxComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (data == 0) || (len == 0u) || (ctx->icCount == 0u))
    {
        return LTC6812_ERR_PARAM;
    }

    while (pos < len)
    {
        Ltc6812_Status_t st;
        uint16_t eepromAddr = (uint16_t)(address + pos);

        Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, Ltc6812_EepromControlByte(eepromAddr, false), LTC6812_COMM_FCOM_ACK, &txComm[0][0], &txComm[0][1]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, (uint8_t)(eepromAddr & 0xFFu), LTC6812_COMM_FCOM_ACK, &txComm[0][2], &txComm[0][3]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, data[pos], LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][4], &txComm[0][5]);

        st = Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);
        if (st != LTC6812_OK)
        {
            return st;
        }

        if (hal->delayMs != 0)
        {
            hal->delayMs(LTC6812_EEPROM_WRITE_DELAY_MS);
        }
        pos++;
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_EepromWriteOdp(Ltc6812_Context_t *ctx,
                                         const Ltc6812_Hal_t *hal,
                                         const Ltc6812_CommandSet_t *cmds,
                                         uint8_t address,
                                         const uint8_t *data,
                                         uint8_t len)
{
    uint8_t pos;
    uint8_t txComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    uint8_t rxComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    /* ODP write device address: 0xB0 (0x58 << 1, R/W=0) */
    const uint8_t odp_dev_write = (uint8_t)((LTC6812_EEPROM_ODP_I2C_ADDR_BASE << 1u) & 0xFEu);

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (data == 0) || (len == 0u) || (ctx->icCount == 0u))
    {
        return LTC6812_ERR_PARAM;
    }

    for (pos = 0u; pos < len; pos++)
    {
        Ltc6812_Status_t st;
        /* A7=0 selects the data area; mask to 4 bits (ODP is 16 bytes, 0x00-0x0F) */
        uint8_t odp_addr = (uint8_t)((address + pos) & 0x0Fu);

        Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, odp_dev_write, LTC6812_COMM_FCOM_ACK,       &txComm[0][0], &txComm[0][1]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, odp_addr,       LTC6812_COMM_FCOM_ACK,       &txComm[0][2], &txComm[0][3]);
        Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, data[pos],      LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][4], &txComm[0][5]);

        st = Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);
        if (st != LTC6812_OK)
        {
            return st;
        }

        if (hal->delayMs != 0)
        {
            hal->delayMs(LTC6812_EEPROM_WRITE_DELAY_MS);
        }
    }

    return LTC6812_OK;
}

Ltc6812_Status_t Ltc6812_EepromLockOdp(Ltc6812_Context_t *ctx,
                                        const Ltc6812_Hal_t *hal,
                                        const Ltc6812_CommandSet_t *cmds)
{
    uint8_t txComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    uint8_t rxComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    Ltc6812_Status_t st;
    /* ODP write device address: 0xB0 */
    const uint8_t odp_dev_write = (uint8_t)((LTC6812_EEPROM_ODP_I2C_ADDR_BASE << 1u) & 0xFEu);

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (ctx->icCount == 0u))
    {
        return LTC6812_ERR_PARAM;
    }

    /* M24C08 permanent ODP lock sequence (irreversible hardware OTP):
     * START + 0xB0 + 0x80 (A7=1 = lock address) + 0x02 (lock bit) + STOP
     * The STOP condition triggers the one-time programming. */
    Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, odp_dev_write, LTC6812_COMM_FCOM_ACK,       &txComm[0][0], &txComm[0][1]);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, 0x80u,          LTC6812_COMM_FCOM_ACK,       &txComm[0][2], &txComm[0][3]);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, 0x02u,          LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][4], &txComm[0][5]);

    st = Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);
    if (st != LTC6812_OK)
    {
        return st;
    }

    /* Wait for OTP programming to complete (tW max = 4 ms) */
    if (hal->delayMs != 0)
    {
        hal->delayMs(LTC6812_EEPROM_WRITE_DELAY_MS);
    }

    return LTC6812_OK;
}

bool Ltc6812_EepromIsOdpLocked(Ltc6812_Context_t *ctx,
                                const Ltc6812_Hal_t *hal,
                                const Ltc6812_CommandSet_t *cmds)
{
    uint8_t txComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    uint8_t rxComm[LTC6812_MAX_ICS][LTC6812_COMM_BYTES_PER_IC];
    Ltc6812_Status_t st;
    uint8_t fcom2;
    /* ODP write device address: 0xB0 */
    const uint8_t odp_dev_write = (uint8_t)((LTC6812_EEPROM_ODP_I2C_ADDR_BASE << 1u) & 0xFEu);

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (ctx->icCount == 0u))
    {
        return false;
    }

    /* STCOMM1: probe the lock-address sequence WITHOUT STOP (FCOM_ACK on last slot).
     * The M24C08 only commits the lock on STOP; omitting STOP keeps it safe.
     * ACK from slave (FCOM=0x0) on slot 2 → ODP not yet locked.
     * NACK from slave (FCOM=0x8) on slot 2 → ODP is locked. */
    Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, odp_dev_write, LTC6812_COMM_FCOM_ACK, &txComm[0][0], &txComm[0][1]);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, 0x80u,          LTC6812_COMM_FCOM_ACK, &txComm[0][2], &txComm[0][3]);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_BLANK, 0x02u,          LTC6812_COMM_FCOM_ACK, &txComm[0][4], &txComm[0][5]);

    st = Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);

    /* STCOMM2: abort the hanging transaction with a repeated START + dummy address + STOP.
     * This resets the I2C bus without triggering the ODP lock. */
    Ltc6812_PrepareCommForIc(txComm, ctx->icCount, 0u);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_START, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][0], &txComm[0][1]);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_NO_TX, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][2], &txComm[0][3]);
    Ltc6812_PackCommByte(LTC6812_COMM_ICOM_NO_TX, 0xFFu, LTC6812_COMM_FCOM_NACK_STOP, &txComm[0][4], &txComm[0][5]);
    (void)Ltc6812_RunComm(ctx, hal, cmds, txComm, rxComm);

    if (st != LTC6812_OK)
    {
        return false;  /* Communication error — report unlocked to avoid false lock indication */
    }

    /* FCOM of slot 2 is in rxComm[0][5] & 0x0F.
     * 0x0 = slave ACK (unlocked), 0x8 = slave NACK (locked). */
    fcom2 = rxComm[0][5] & 0x0Fu;
    return (fcom2 != LTC6812_COMM_FCOM_ACK);
}

Ltc6812_Status_t Ltc6812_SetGpioPullDown(Ltc6812_Context_t *ctx,
                                         const Ltc6812_Hal_t *hal,
                                         const Ltc6812_CommandSet_t *cmds,
                                         uint8_t ic,
                                         uint8_t gpio,
                                         bool pullDownOn)
{
    uint8_t mask;

    if ((ctx == 0) || (hal == 0) || (cmds == 0) || (ic >= ctx->icCount) || (gpio == 0u) || (gpio > 9u))
    {
        return LTC6812_ERR_PARAM;
    }

    if (gpio <= 5u)
    {
        mask = (uint8_t)(1u << (uint8_t)(gpio + 2u));
        if (pullDownOn == true)
        {
            ctx->cfga[ic].data[0] &= (uint8_t)(~mask);
        }
        else
        {
            ctx->cfga[ic].data[0] |= mask;
        }
        return Ltc6812_WriteCfga(ctx, hal, cmds);
    }

    mask = (uint8_t)(1u << (uint8_t)(gpio - 6u));
    if (pullDownOn == true)
    {
        ctx->cfgb[ic].data[0] &= (uint8_t)(~mask);
    }
    else
    {
        ctx->cfgb[ic].data[0] |= mask;
    }

    return Ltc6812_WriteCfgb(ctx, hal, cmds);
}

Ltc6812_Status_t Ltc6812_SetBoardLed(Ltc6812_Context_t *ctx,
                                     const Ltc6812_Hal_t *hal,
                                     const Ltc6812_CommandSet_t *cmds,
                                     bool on)
{
    if (ctx == 0)
    {
        return LTC6812_ERR_PARAM;
    }

    if (ctx->icCount <= LTC6812_BOARD_LED_IC)
    {
        return LTC6812_ERR_STATE;
    }

    return Ltc6812_SetGpioPullDown(ctx,
                                   hal,
                                   cmds,
                                   LTC6812_BOARD_LED_IC,
                                   LTC6812_BOARD_LED_GPIO,
                                   on);
}

Ltc6812_Status_t Ltc6812_Task(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds, uint32_t measurementPeriodMs)
{
    uint32_t timeSinceLastComm;
    uint32_t timeSinceLastMeasure;
    uint32_t timeSinceLastAuxMeasure;

    if ((ctx == 0) || (hal == 0) || (cmds == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    timeSinceLastComm = ctx->tickMs - ctx->lastCommMs;
    timeSinceLastMeasure = ctx->tickMs - ctx->lastMeasureMs;
    timeSinceLastAuxMeasure = ctx->tickMs - ctx->lastAuxMeasureMs;

    switch (ctx->svcState)
    {
        case LTC6812_SVC_BOOT:
            ctx->configured = false;
            ctx->linkState = LTC6812_LINK_IDLE;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->svcState = LTC6812_SVC_WAKE_STACK;
            break;

        case LTC6812_SVC_WAKE_STACK:
            if (Ltc6812_WakeUp(ctx, hal) != LTC6812_OK)
            {
                ctx->commErrorCount++;
                ctx->linkState = LTC6812_LINK_ERROR;
                ctx->svcState = LTC6812_SVC_FAULT;
                return LTC6812_ERR_COMM;
            }

            ctx->linkState = LTC6812_LINK_READY;
            ctx->lastCommMs = ctx->tickMs;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->svcState = (ctx->configured == false) ? LTC6812_SVC_CONFIGURE : LTC6812_SVC_START_MEASURE;
            break;

        case LTC6812_SVC_CONFIGURE:
            if (Ltc6812_FullInitialize(ctx, hal, cmds) != LTC6812_OK)
            {
                ctx->configured = false;
                ctx->commErrorCount++;
                ctx->linkState = LTC6812_LINK_ERROR;
                ctx->svcState = LTC6812_SVC_FAULT;
                return LTC6812_ERR_COMM;
            }

            ctx->stateEntryMs = ctx->tickMs;
            ctx->svcState = LTC6812_SVC_START_MEASURE;
            break;

        case LTC6812_SVC_STANDBY:
            if (timeSinceLastMeasure >= measurementPeriodMs)
            {
                if (timeSinceLastComm >= LTC6812_SLEEP_WATCHDOG_TIMEOUT_MS)
                {
                    ctx->configured = false;
                    ctx->svcState = LTC6812_SVC_WAKE_STACK;
                }
                else if (timeSinceLastComm >= LTC6812_LINK_IDLE_TIMEOUT_MS)
                {
                    ctx->svcState = LTC6812_SVC_WAKE_STACK;
                }
                else
                {
                    ctx->svcState = LTC6812_SVC_START_MEASURE;
                }
                ctx->stateEntryMs = ctx->tickMs;
            }
            else if (timeSinceLastAuxMeasure >= LTC6812_AUX_SAMPLE_PERIOD_MS)
            {
                if (timeSinceLastComm >= LTC6812_SLEEP_WATCHDOG_TIMEOUT_MS)
                {
                    ctx->configured = false;
                    ctx->svcState = LTC6812_SVC_WAKE_STACK;
                }
                else if (timeSinceLastComm >= LTC6812_LINK_IDLE_TIMEOUT_MS)
                {
                    if (Ltc6812_WakeUp(ctx, hal) != LTC6812_OK)
                    {
                        ctx->commErrorCount++;
                        ctx->linkState = LTC6812_LINK_ERROR;
                        ctx->svcState = LTC6812_SVC_FAULT;
                        return LTC6812_ERR_COMM;
                    }

                    ctx->lastCommMs = ctx->tickMs;
                    ctx->linkState = LTC6812_LINK_READY;
                    ctx->svcState = LTC6812_SVC_START_AUX_MEASURE;
                }
                else
                {
                    ctx->svcState = LTC6812_SVC_START_AUX_MEASURE;
                }
                ctx->stateEntryMs = ctx->tickMs;
            }
            break;

        case LTC6812_SVC_START_MEASURE:
            if (timeSinceLastComm >= LTC6812_SLEEP_WATCHDOG_TIMEOUT_MS)
            {
                ctx->configured = false;
                ctx->svcState = LTC6812_SVC_WAKE_STACK;
                break;
            }

            if (timeSinceLastComm >= LTC6812_LINK_IDLE_TIMEOUT_MS)
            {
                ctx->svcState = LTC6812_SVC_WAKE_STACK;
                break;
            }

            ctx->lastDiagStep = LTC6812_DIAG_ADCV;
            ctx->lastDiagCmd = cmds->ADCV;
            if (Ltc6812_StartCellConversion(hal, cmds) != LTC6812_OK)
            {
                ctx->commErrorCount++;
                ctx->linkState = LTC6812_LINK_ERROR;
                ctx->svcState = LTC6812_SVC_FAULT;
                return LTC6812_ERR_COMM;
            }

            ctx->linkState = LTC6812_LINK_XFER;
            ctx->lastCommMs = ctx->tickMs;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->measureCount++;
            ctx->svcState = LTC6812_SVC_WAIT_MEASURE;
            break;

        case LTC6812_SVC_WAIT_MEASURE:
            if ((ctx->tickMs - ctx->stateEntryMs) > LTC6812_CELL_MEASUREMENT_TIME_MS)
            {
                if ((ctx->tickMs - ctx->lastCommMs) >= LTC6812_LINK_IDLE_TIMEOUT_MS)
                {
                    if (Ltc6812_WakeUp(ctx, hal) != LTC6812_OK)
                    {
                        ctx->commErrorCount++;
                        ctx->linkState = LTC6812_LINK_ERROR;
                        ctx->svcState = LTC6812_SVC_FAULT;
                        return LTC6812_ERR_COMM;
                    }

                    ctx->lastCommMs = ctx->tickMs;
                    ctx->linkState = LTC6812_LINK_READY;
                }

                ctx->svcState = LTC6812_SVC_READ_RESULTS;
            }
            break;

        case LTC6812_SVC_READ_RESULTS:
            if (Ltc6812_ReadCellVoltagesByGroup(ctx, hal, cmds) != LTC6812_OK)
            {
                ctx->commErrorCount++;
                ctx->linkState = LTC6812_LINK_ERROR;
                ctx->svcState = LTC6812_SVC_FAULT;
                return LTC6812_ERR_COMM;
            }

            ctx->linkState = LTC6812_LINK_READY;
            ctx->lastCommMs = ctx->tickMs;
            ctx->lastMeasureMs = ctx->tickMs;
            ctx->svcState = LTC6812_SVC_PROCESS_RESULTS;
            break;

        case LTC6812_SVC_START_AUX_MEASURE:
            if (timeSinceLastComm >= LTC6812_SLEEP_WATCHDOG_TIMEOUT_MS)
            {
                ctx->configured = false;
                ctx->svcState = LTC6812_SVC_WAKE_STACK;
                break;
            }

            if (timeSinceLastComm >= LTC6812_LINK_IDLE_TIMEOUT_MS)
            {
                ctx->svcState = LTC6812_SVC_WAKE_STACK;
                break;
            }

            ctx->lastDiagStep = LTC6812_DIAG_ADAX;
            ctx->lastDiagCmd = cmds->ADAX;
            if (Ltc6812_StartAuxConversion(hal, cmds) != LTC6812_OK)
            {
                ctx->commErrorCount++;
                ctx->linkState = LTC6812_LINK_ERROR;
                ctx->svcState = LTC6812_SVC_FAULT;
                return LTC6812_ERR_COMM;
            }

            ctx->linkState = LTC6812_LINK_XFER;
            ctx->lastCommMs = ctx->tickMs;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->auxMeasureCount++;
            ctx->svcState = LTC6812_SVC_WAIT_AUX_MEASURE;
            break;

        case LTC6812_SVC_WAIT_AUX_MEASURE:
            if ((ctx->tickMs - ctx->stateEntryMs) > LTC6812_AUX_MEASUREMENT_TIME_MS)
            {
                if ((ctx->tickMs - ctx->lastCommMs) >= LTC6812_LINK_IDLE_TIMEOUT_MS)
                {
                    if (Ltc6812_WakeUp(ctx, hal) != LTC6812_OK)
                    {
                        ctx->commErrorCount++;
                        ctx->linkState = LTC6812_LINK_ERROR;
                        ctx->svcState = LTC6812_SVC_FAULT;
                        return LTC6812_ERR_COMM;
                    }

                    ctx->lastCommMs = ctx->tickMs;
                    ctx->linkState = LTC6812_LINK_READY;
                }

                ctx->svcState = LTC6812_SVC_READ_AUX_RESULTS;
            }
            break;

        case LTC6812_SVC_READ_AUX_RESULTS:
            if (Ltc6812_ReadAuxVoltagesByGroup(ctx, hal, cmds) != LTC6812_OK)
            {
                ctx->commErrorCount++;
                ctx->linkState = LTC6812_LINK_ERROR;
                ctx->svcState = LTC6812_SVC_FAULT;
                return LTC6812_ERR_COMM;
            }

            ctx->linkState = LTC6812_LINK_READY;
            ctx->lastCommMs = ctx->tickMs;
            ctx->lastAuxMeasureMs = ctx->tickMs;
            ctx->svcState = LTC6812_SVC_PROCESS_RESULTS;
            break;

        case LTC6812_SVC_PROCESS_RESULTS:
            ctx->svcState = LTC6812_SVC_STANDBY;
            break;

        case LTC6812_SVC_FAULT:
            ctx->configured = false;
            ctx->linkState = LTC6812_LINK_ERROR;
            ctx->stateEntryMs = ctx->tickMs;
            ctx->svcState = LTC6812_SVC_WAKE_STACK;
            break;

        default:
            ctx->configured = false;
            ctx->svcState = LTC6812_SVC_FAULT;
            return LTC6812_ERR_STATE;
    }

    return LTC6812_OK;
}

void Ltc6812_CfgaPackDcc(uint16_t dccMask, uint8_t cfga[LTC6812_BYTES_PER_CFGR])
{
    if (cfga == 0)
    {
        return;
    }

    cfga[4] = (uint8_t)(dccMask & 0x00FFu);
    cfga[5] = (uint8_t)((cfga[5] & 0xF0u) | ((dccMask >> 8u) & 0x0Fu));
}

void Ltc6812_CfgbPackDcc(uint16_t dccMask, uint8_t cfgb[LTC6812_BYTES_PER_CFGR])
{
    if (cfgb == 0)
    {
        return;
    }

    cfgb[0] = (uint8_t)((cfgb[0] & 0x8Fu) | ((dccMask >> 8u) & 0x70u));
}
