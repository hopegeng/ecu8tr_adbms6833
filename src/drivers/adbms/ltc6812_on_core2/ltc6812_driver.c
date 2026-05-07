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
    cmds->RDCVA = LTC6812_RDCVA_REG;
    cmds->RDCVB = LTC6812_RDCVB_REG;
    cmds->RDCVC = LTC6812_RDCVC_REG;
    cmds->RDCVD = LTC6812_RDCVD_REG;
    cmds->RDCVE = LTC6812_RDCVE_REG;
    cmds->ADCV = LTC6812_ADCV_BASE_REG;
    cmds->ADAX = LTC6812_ADAX_BASE_REG;
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
#endif
		for( int idx = 0; idx < 6; idx++ )
		{
			PRINTF( "The tx cfga[%d] = 0x%x, the rx cfga[%d] = 0x%x\r\n", idx, ctx->cfga[ic].data[idx], idx, readCfga[idx] );
		}
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

Ltc6812_Status_t Ltc6812_Task(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds, uint32_t measurementPeriodMs)
{
    uint32_t timeSinceLastComm;
    uint32_t timeSinceLastMeasure;

    if ((ctx == 0) || (hal == 0) || (cmds == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    timeSinceLastComm = ctx->tickMs - ctx->lastCommMs;
    timeSinceLastMeasure = ctx->tickMs - ctx->lastMeasureMs;

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
