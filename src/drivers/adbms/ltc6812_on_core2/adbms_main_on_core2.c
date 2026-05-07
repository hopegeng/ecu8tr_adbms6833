#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tools.h"
#include "qspi.h"
#include "qspi0mstr_illd.h"
#include "shell.h"
#include "ltc6812_balance.h"
#include "ltc6812_driver.h"
#include "ltc6812_shared.h"
#include "ecu8tr_cmd.h"

#define DO_TEST			(0)
#define ISR_PRIORITY_STM2_TICK_LTC6812  19
#define LTC6812_CORE2_DEMO_MODE         (0u)
#define LTC6812_QSPI_WAIT_TIMEOUT_LOOPS (1000000u)

#ifndef LTC6812_BALANCE_FORCE_CHARGING_ACTIVE
/* DC3036A bench test hook. Set to 0 for application-controlled charging state. */
#define LTC6812_BALANCE_FORCE_CHARGING_ACTIVE (1u)
#endif

#ifndef LTC6812_BALANCE_DCTO_MINUTES_CODE
#define LTC6812_BALANCE_DCTO_MINUTES_CODE     (1u)
#endif

#ifndef LTC6812_ACTIVE_IC_COUNT
#define LTC6812_ACTIVE_IC_COUNT         (1u)
#endif

#undef LTC6812_ENABLE_DEBUG_PRINTF
#define LTC6812_ENABLE_DEBUG_PRINTF     (1u)
#if LTC6812_ENABLE_DEBUG_PRINTF
#define LTC6812_DEBUG_PRINTF(...)       PRINTF(__VA_ARGS__)
#define LTC6812_PRINT_DEBUG_STATUS(status) Ltc6812_PrintDebugStatus(status)
#else
#define LTC6812_DEBUG_PRINTF(...)       ((void)0)
#define LTC6812_PRINT_DEBUG_STATUS(status) ((void)(status))
#endif

typedef struct
{
    uint32_t sequence;
    Ltc6812_SharedSnapshot_t snapshot;
} Ltc6812_SharedMemory_t;

static Ltc6812_Context_t g_ltc6812Drv;
static Ltc6812_BalanceContext_t g_ltc6812Bal;
static Ltc6812_CommandSet_t g_ltc6812Cmds;
static Ltc6812_Hal_t g_ltc6812Hal;
static volatile Ltc6812_SharedMemory_t g_ltc6812Shared __attribute__ ((section("NC_cpu0_dlmu"))) = {0};
static volatile uint32_t g_ltc6812SysTickMs = 0u;
static bool g_ltc6812RuntimeInit = false;
static ECU8TR_ADBMS6830_State_t g_ltc6812State = ECU8TR_ADBMS6830_IDLE;
static uint32_t g_ltc6812LastPublishedMeasureMs = 0u;
static uint32_t g_ltc6812SampleCounter = 0u;
#if LTC6812_ENABLE_DEBUG_PRINTF
static uint32_t g_ltc6812LastDebugPrintMs = 0u;
#endif
static uint32_t g_ltc6812LastBalanceDebugMeasureMs = 0u;
static uint32_t g_ltc6812QspiTimeoutCount = 0u;
static uint16_t g_ltc6812LastAppliedDcc[LTC6812_MAX_ICS] = {0u};

static Ltc6812_Status_t Ltc6812_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void Ltc6812_DelayUs(uint32_t us);
static void Ltc6812_DelayMs(uint32_t ms);
static void Ltc6812_FillDefaultCfga(Ltc6812_Context_t *ctx);
static void Ltc6812_FillDefaultCfgb(Ltc6812_Context_t *ctx);
static void Ltc6812_InitCore2Runtime(void);
static void Ltc6812_PublishSharedSnapshot(void);
static bool Ltc6812_HasAnyDccSet(const Ltc6812_BalanceContext_t *bal, uint8_t icCount);
static void Ltc6812_UpdateLastAppliedDcc(const Ltc6812_BalanceContext_t *bal, uint8_t icCount);
#if LTC6812_CORE2_DEMO_MODE
static void Ltc6812_PublishDemoSnapshot(void);
#endif
static void Ltc6812_SharedMemoryBarrier(void);
#if LTC6812_ENABLE_DEBUG_PRINTF
static const char *Ltc6812_StatusToString(Ltc6812_Status_t status);
static const char *Ltc6812_SvcStateToString(Ltc6812_ServiceState_t state);
static const char *Ltc6812_DiagStepToString(Ltc6812_DiagStep_t step);
static const char *Ltc6812_BalanceParityToString(Ltc6812_BalanceParity_t parity);
static void Ltc6812_PrintDebugStatus(Ltc6812_Status_t status);
#endif

static void Ltc6812_SharedMemoryBarrier(void)
{
    __dsync();
}

static bool Ltc6812_HasAnyDccSet(const Ltc6812_BalanceContext_t *bal, uint8_t icCount)
{
    uint8_t ic;

    if (bal == 0)
    {
        return false;
    }

    if (icCount > LTC6812_MAX_ICS)
    {
        icCount = LTC6812_MAX_ICS;
    }

    for (ic = 0u; ic < icCount; ic++)
    {
        if (bal->result[ic].dccMask != 0u)
        {
            return true;
        }

        if (g_ltc6812LastAppliedDcc[ic] != 0u)
        {
            return true;
        }
    }

    return false;
}

static void Ltc6812_UpdateLastAppliedDcc(const Ltc6812_BalanceContext_t *bal, uint8_t icCount)
{
    uint8_t ic;

    if (bal == 0)
    {
        return;
    }

    if (icCount > LTC6812_MAX_ICS)
    {
        icCount = LTC6812_MAX_ICS;
    }

    for (ic = 0u; ic < icCount; ic++)
    {
        g_ltc6812LastAppliedDcc[ic] = bal->result[ic].dccMask;
    }
}

#if LTC6812_ENABLE_DEBUG_PRINTF
static const char *Ltc6812_StatusToString(Ltc6812_Status_t status)
{
    switch (status)
    {
        case LTC6812_OK:
            return "OK";
        case LTC6812_ERR_PARAM:
            return "PARAM";
        case LTC6812_ERR_COMM:
            return "COMM";
        case LTC6812_ERR_PEC:
            return "PEC";
        case LTC6812_ERR_TIMEOUT:
            return "TIMEOUT";
        case LTC6812_ERR_STATE:
            return "STATE";
        default:
            return "UNKNOWN";
    }
}

static const char *Ltc6812_SvcStateToString(Ltc6812_ServiceState_t state)
{
    switch (state)
    {
        case LTC6812_SVC_BOOT:
            return "BOOT";
        case LTC6812_SVC_WAKE_STACK:
            return "WAKE";
        case LTC6812_SVC_CONFIGURE:
            return "CONFIG";
        case LTC6812_SVC_STANDBY:
            return "STANDBY";
        case LTC6812_SVC_START_MEASURE:
            return "START_MEAS";
        case LTC6812_SVC_WAIT_MEASURE:
            return "WAIT_MEAS";
        case LTC6812_SVC_READ_RESULTS:
            return "READ";
        case LTC6812_SVC_PROCESS_RESULTS:
            return "PROCESS";
        case LTC6812_SVC_FAULT:
            return "FAULT";
        default:
            return "UNKNOWN";
    }
}

static const char *Ltc6812_DiagStepToString(Ltc6812_DiagStep_t step)
{
    switch (step)
    {
        case LTC6812_DIAG_NONE:
            return "NONE";
        case LTC6812_DIAG_WAKE:
            return "WAKE";
        case LTC6812_DIAG_WRCFGA:
            return "WRCFGA";
        case LTC6812_DIAG_RDCFGA:
            return "RDCFGA";
        case LTC6812_DIAG_CFGA_COMPARE:
            return "CFGA_CMP";
        case LTC6812_DIAG_WRCFGB:
            return "WRCFGB";
        case LTC6812_DIAG_RDCFGB:
            return "RDCFGB";
        case LTC6812_DIAG_CFGB_COMPARE:
            return "CFGB_CMP";
        case LTC6812_DIAG_ADCV:
            return "ADCV";
        case LTC6812_DIAG_RDCV:
            return "RDCV";
        case LTC6812_DIAG_RDCV_PEC:
            return "RDCV_PEC";
        default:
            return "UNKNOWN";
    }
}

static const char *Ltc6812_BalanceParityToString(Ltc6812_BalanceParity_t parity)
{
    switch (parity)
    {
        case LTC6812_BALANCE_PARITY_NONE:
            return "NONE";
        case LTC6812_BALANCE_PARITY_ODD:
            return "ODD";
        case LTC6812_BALANCE_PARITY_EVEN:
            return "EVEN";
        default:
            return "UNKNOWN";
    }
}

static void Ltc6812_PrintDebugStatus(Ltc6812_Status_t status)
{
    if ((status == LTC6812_OK) &&
        ((g_ltc6812SysTickMs - g_ltc6812LastDebugPrintMs) < LTC6812_DEBUG_PRINT_PERIOD_MS))
    {
        return;
    }

    g_ltc6812LastDebugPrintMs = g_ltc6812SysTickMs;

    LTC6812_DEBUG_PRINTF("LTC6812 t=%lu st=%s svc=%s diag=%s cmd=%04X grp=%u ic=%u cfg=%u meas=%lu pec=%lu comm=%lu qto=%lu rx=%04X calc=%04X C1=%u C2=%u C3=%u C10=%u\r\n",
                         (uint32_t)g_ltc6812SysTickMs,
                         Ltc6812_StatusToString(status),
                         Ltc6812_SvcStateToString(g_ltc6812Drv.svcState),
                         Ltc6812_DiagStepToString(g_ltc6812Drv.lastDiagStep),
                         (uint32_t)g_ltc6812Drv.lastDiagCmd,
                         (uint32_t)g_ltc6812Drv.lastDiagGroup,
                         (uint32_t)g_ltc6812Drv.lastDiagIc,
                         g_ltc6812Drv.configured ? 1u : 0u,
                         (uint32_t)g_ltc6812Drv.measureCount,
                         (uint32_t)g_ltc6812Drv.pecErrorCount,
                         (uint32_t)g_ltc6812Drv.commErrorCount,
                         (uint32_t)g_ltc6812QspiTimeoutCount,
                         (uint32_t)g_ltc6812Drv.lastRxPec,
                         (uint32_t)g_ltc6812Drv.lastCalcPec,
                         (uint32_t)g_ltc6812Drv.cell[0].mV[0],
                         (uint32_t)g_ltc6812Drv.cell[0].mV[1],
                         (uint32_t)g_ltc6812Drv.cell[0].mV[2],
                         (uint32_t)g_ltc6812Drv.cell[0].mV[9]);
}
#endif

void Ltc6812_SharedInit(void)
{
    uint8_t afeIdx;
    uint8_t cellIdx;

    g_ltc6812Shared.sequence = 0u;
    g_ltc6812Shared.snapshot.sample_counter = 0u;
    g_ltc6812Shared.snapshot.sample_timestamp_ms = 0u;
    g_ltc6812Shared.snapshot.valid = false;

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            g_ltc6812Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx] = 0u;
            g_ltc6812Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = 0;
            g_ltc6812Shared.snapshot.balancing[afeIdx][cellIdx] = 0u;
        }
    }
}

void Ltc6812_SharedPublish(const Ltc6812_SharedSnapshot_t *snapshot)
{
    uint8_t afeIdx;
    uint8_t cellIdx;

    if (snapshot == 0)
    {
        return;
    }

    g_ltc6812Shared.sequence++;
    Ltc6812_SharedMemoryBarrier();

    g_ltc6812Shared.snapshot.sample_counter = snapshot->sample_counter;
    g_ltc6812Shared.snapshot.sample_timestamp_ms = snapshot->sample_timestamp_ms;
    g_ltc6812Shared.snapshot.valid = snapshot->valid;

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            g_ltc6812Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx] = snapshot->cell_voltage_mV[afeIdx][cellIdx];
            g_ltc6812Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = snapshot->cell_temp_raw_0p01C[afeIdx][cellIdx];
            g_ltc6812Shared.snapshot.balancing[afeIdx][cellIdx] = snapshot->balancing[afeIdx][cellIdx];
        }
    }

    Ltc6812_SharedMemoryBarrier();
    g_ltc6812Shared.sequence++;
    Ltc6812_SharedMemoryBarrier();
}

bool Ltc6812_SharedRead(Ltc6812_SharedSnapshot_t *snapshot)
{
    uint32_t seqStart;
    uint32_t seqEnd;
    uint8_t afeIdx;
    uint8_t cellIdx;

    if (snapshot == 0)
    {
        return false;
    }

    do
    {
        seqStart = g_ltc6812Shared.sequence;
        Ltc6812_SharedMemoryBarrier();

        if ((seqStart & 0x1u) != 0u)
        {
            seqEnd = seqStart;
            continue;
        }

        snapshot->sample_counter = g_ltc6812Shared.snapshot.sample_counter;
        snapshot->sample_timestamp_ms = g_ltc6812Shared.snapshot.sample_timestamp_ms;
        snapshot->valid = g_ltc6812Shared.snapshot.valid;

        for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
        {
            for (cellIdx = 0u; cellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; cellIdx++)
            {
                snapshot->cell_voltage_mV[afeIdx][cellIdx] = g_ltc6812Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx];
                snapshot->cell_temp_raw_0p01C[afeIdx][cellIdx] = g_ltc6812Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx];
                snapshot->balancing[afeIdx][cellIdx] = g_ltc6812Shared.snapshot.balancing[afeIdx][cellIdx];
            }
        }

        Ltc6812_SharedMemoryBarrier();
        seqEnd = g_ltc6812Shared.sequence;
    } while ((seqStart != seqEnd) || ((seqEnd & 0x1u) != 0u));

    return true;
}

IFX_INTERRUPT(stm2_isr_handler_ltc6812, 2, ISR_PRIORITY_STM2_TICK_LTC6812);
void stm2_isr_handler_ltc6812(void)
{
    uint32_t nextCompare;

    IfxCpu_enableInterrupts();
    nextCompare = IfxStm_getTicksFromMilliseconds(&MODULE_STM2, 1u);
    IfxStm_increaseCompare(&MODULE_STM2, IfxStm_Comparator_0, nextCompare);
    g_ltc6812SysTickMs++;
}

static Ltc6812_Status_t Ltc6812_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    SpiIf_Status retVal;

    if ((tx == 0) || (rx == 0) || (len == 0u))
    {
        return LTC6812_ERR_PARAM;
    }

    retVal = qspi0_send_receive_iLLD(eQspiHwCs02, len, (uint8_t *)tx, rx);
    if (retVal == SpiIf_Status_ok)
    {
        //if (qspimstr_waitForRxDoneTimeout_iLLD(LTC6812_QSPI_WAIT_TIMEOUT_LOOPS) == true)
		if (qspimstr_waitForRxDone_iLLD() == true)
        {
            return LTC6812_OK;
        }

        g_ltc6812QspiTimeoutCount++;
        return LTC6812_ERR_TIMEOUT;
    }

    return LTC6812_ERR_COMM;
}

static void Ltc6812_DelayUs(uint32_t us)
{
    delay_us_stm2(us);
}

static void Ltc6812_DelayMs(uint32_t ms)
{
    delay_ms_stm2(ms);
}

static void Ltc6812_FillDefaultCfga(Ltc6812_Context_t *ctx)
{
    uint8_t ic;

    if (ctx == 0)
    {
        return;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        (void)memset(ctx->cfga[ic].data, 0, LTC6812_BYTES_PER_CFGR);
        ctx->cfga[ic].data[0] = 0xFCu; /* REFON=1, ADCOPT=0, GPIO1..GPIO5 pull-downs off. */
        ctx->cfga[ic].data[1] = 0x00u;
        ctx->cfga[ic].data[2] = 0x00u;
        ctx->cfga[ic].data[3] = 0x00u;
        ctx->cfga[ic].data[4] = 0x00u;
        ctx->cfga[ic].data[5] = (uint8_t)((LTC6812_BALANCE_DCTO_MINUTES_CODE & 0x0Fu) << 4u);
    }
}

static void Ltc6812_FillDefaultCfgb(Ltc6812_Context_t *ctx)
{
    uint8_t ic;

    if (ctx == 0)
    {
        return;
    }

    for (ic = 0u; ic < ctx->icCount; ic++)
    {
        (void)memset(ctx->cfgb[ic].data, 0, LTC6812_BYTES_PER_CFGR);
        ctx->cfgb[ic].data[0] = 0x0Fu; /* GPIO6..GPIO9 pull-downs off, DCC13..DCC15 off. */
    }
}

static void Ltc6812_InitCore2Runtime(void)
{
    IfxStm_CompareConfig stmCompareConfig;

    if (g_ltc6812RuntimeInit == true)
    {
        return;
    }

    IfxStm_initCompareConfig(&stmCompareConfig);
    stmCompareConfig.comparator = IfxStm_Comparator_0;
    stmCompareConfig.compareOffset = IfxStm_ComparatorOffset_0;
    stmCompareConfig.compareSize = IfxStm_ComparatorSize_32Bits;
    stmCompareConfig.ticks = IfxStm_getTicksFromMilliseconds(&MODULE_STM2, 1u);
    stmCompareConfig.triggerPriority = ISR_PRIORITY_STM2_TICK_LTC6812;
    stmCompareConfig.typeOfService = IfxSrc_Tos_cpu2;

    (void)IfxStm_initCompare(&MODULE_STM2, &stmCompareConfig);
    g_ltc6812SysTickMs = 0u;
    g_ltc6812RuntimeInit = true;
    Ltc6812_SharedInit();
    IfxCpu_enableInterrupts();
}

static void Ltc6812_PublishSharedSnapshot(void)
{
    Ltc6812_SharedSnapshot_t snapshot;
    uint8_t afeIdx;
    uint8_t usedCellIdx;

    snapshot.sample_counter = ++g_ltc6812SampleCounter;
    snapshot.sample_timestamp_ms = g_ltc6812Drv.lastMeasureMs;
    snapshot.valid = (g_ltc6812State == ECU8TR_ADBMS6830_OK);

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (usedCellIdx = 0u; usedCellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; usedCellIdx++)
        {
            uint8_t driverCellIdx = (uint8_t)(LTC6812_SHARED_FIRST_USED_CELL_0BASED + usedCellIdx);

            snapshot.cell_temp_raw_0p01C[afeIdx][usedCellIdx] = 0;
            if (afeIdx < g_ltc6812Drv.icCount)
            {
                uint16_t dccMask = g_ltc6812Bal.result[afeIdx].dccMask;

                snapshot.cell_voltage_mV[afeIdx][usedCellIdx] = g_ltc6812Drv.cell[afeIdx].mV[driverCellIdx];
                snapshot.balancing[afeIdx][usedCellIdx] = ((dccMask & ((uint16_t)1u << driverCellIdx)) != 0u) ? 1u : 0u;
            }
            else
            {
                snapshot.cell_voltage_mV[afeIdx][usedCellIdx] = 0u;
                snapshot.balancing[afeIdx][usedCellIdx] = 0u;
            }
        }
    }

    Ltc6812_SharedPublish(&snapshot);
}

#if LTC6812_CORE2_DEMO_MODE
static void Ltc6812_PublishDemoSnapshot(void)
{
    Ltc6812_SharedSnapshot_t snapshot;
    uint8_t afeIdx;
    uint8_t cellIdx;

    snapshot.sample_counter = ++g_ltc6812SampleCounter;
    snapshot.sample_timestamp_ms = g_ltc6812SysTickMs;
    snapshot.valid = true;

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            snapshot.cell_voltage_mV[afeIdx][cellIdx] = (uint16_t)(3650u + ((uint16_t)afeIdx * 20u) + cellIdx);
            snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = 0;
            snapshot.balancing[afeIdx][cellIdx] = 0u;
        }
    }

    g_ltc6812State = ECU8TR_ADBMS6830_OK;
    g_ltc6812LastPublishedMeasureMs = snapshot.sample_timestamp_ms;
    Ltc6812_SharedPublish(&snapshot);
}
#endif





void ltc6812_main_on_core2(void)
{
    uint32_t lastDemoPublishMs = 0u;

    Ltc6812_InitCore2Runtime();

#if (LTC6812_CORE2_DEMO_MODE == 1u)
    while (1)
    {
        if ((g_ltc6812SysTickMs - lastDemoPublishMs) >= LTC6812_SHARED_SAMPLE_PERIOD_MS)
        {
            Ltc6812_PublishDemoSnapshot();
            lastDemoPublishMs = g_ltc6812SysTickMs;
        }
    }
#else
    (void)lastDemoPublishMs;
    qspi0mstr_Init_iLLD();

    g_ltc6812Hal.spiTransfer = Ltc6812_QspiTransfer;
    g_ltc6812Hal.delayUs = Ltc6812_DelayUs;
    g_ltc6812Hal.delayMs = Ltc6812_DelayMs;

    Ltc6812_Init(&g_ltc6812Drv, LTC6812_ACTIVE_IC_COUNT);
    Ltc6812_SetDefaultCommands(&g_ltc6812Cmds);
    Ltc6812_BalanceInit(&g_ltc6812Bal);
    Ltc6812_FillDefaultCfga(&g_ltc6812Drv);
    Ltc6812_FillDefaultCfgb(&g_ltc6812Drv);

    g_ltc6812Bal.cfg.chargingActive = (LTC6812_BALANCE_FORCE_CHARGING_ACTIVE != 0u);
    g_ltc6812Bal.cfg.faultActive = false;
    g_ltc6812Drv.svcState = LTC6812_SVC_BOOT;

#if DO_TEST
    ltc6812_test();
#endif


    while (1)
    {
        Ltc6812_Status_t status;

        g_ltc6812Drv.tickMs = g_ltc6812SysTickMs;
        status = Ltc6812_Task(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds, LTC6812_SHARED_SAMPLE_PERIOD_MS);
        //LTC6812_PRINT_DEBUG_STATUS(status);

        if (status != LTC6812_OK)
        {
            g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        }
        else
        {
            g_ltc6812State = ECU8TR_ADBMS6830_OK;
        }

        if (g_ltc6812Drv.svcState == LTC6812_SVC_STANDBY)
        {
            bool balanceDecisionFresh = (g_ltc6812Drv.lastMeasureMs != g_ltc6812LastBalanceDebugMeasureMs);

            Ltc6812_BalanceEvaluate(&g_ltc6812Bal, &g_ltc6812Drv);

            if (balanceDecisionFresh == true)
            {
#if 0
                for (uint8_t afeIdx = 0u; afeIdx < g_ltc6812Drv.icCount; afeIdx++)
                {
                    LTC6812_DEBUG_PRINTF("LTC6812 BAL AFE%u chg=%u en=%u fault=%u min=%u max=%u d=%u parity=%s dcc=0x%04X\r\n",
                                         (uint32_t)afeIdx,
                                         g_ltc6812Bal.cfg.chargingActive ? 1u : 0u,
                                         g_ltc6812Bal.cfg.balancingEnabled ? 1u : 0u,
                                         g_ltc6812Bal.cfg.faultActive ? 1u : 0u,
                                         (uint32_t)g_ltc6812Bal.result[afeIdx].cellMin_mV,
                                         (uint32_t)g_ltc6812Bal.result[afeIdx].cellMax_mV,
                                         (uint32_t)g_ltc6812Bal.result[afeIdx].delta_mV,
                                         Ltc6812_BalanceParityToString(g_ltc6812Bal.result[afeIdx].selectedParity),
                                         (uint32_t)g_ltc6812Bal.result[afeIdx].dccMask);
                }
#endif

                g_ltc6812LastBalanceDebugMeasureMs = g_ltc6812Drv.lastMeasureMs;
            }

            if ((balanceDecisionFresh == true) &&
                (g_ltc6812Bal.cfg.chargingActive == true) &&
                (Ltc6812_HasAnyDccSet(&g_ltc6812Bal, g_ltc6812Drv.icCount) == true))
            {
                status = Ltc6812_SendMute(&g_ltc6812Hal, &g_ltc6812Cmds);
                if (status == LTC6812_OK)
                {
                    status = Ltc6812_BalanceApplyDcc(&g_ltc6812Bal, &g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds);
                }
                if (status == LTC6812_OK)
                {
                    status = Ltc6812_SendUnmute(&g_ltc6812Hal, &g_ltc6812Cmds);
                }
                if (status != LTC6812_OK)
                {
                    g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
                    LTC6812_PRINT_DEBUG_STATUS(status);
                }
                else if (g_ltc6812Drv.icCount > 0u)
                {
                    Ltc6812_UpdateLastAppliedDcc(&g_ltc6812Bal, g_ltc6812Drv.icCount);
                    LTC6812_DEBUG_PRINTF("LTC6812 BAL WRITE st=OK dcc=0x%04X cfga4=%02X cfga5=%02X cfgb0=%02X\r\n",
                                         (uint32_t)g_ltc6812Bal.result[0].dccMask,
                                         (uint32_t)g_ltc6812Drv.cfga[0].data[4],
                                         (uint32_t)g_ltc6812Drv.cfga[0].data[5],
                                         (uint32_t)g_ltc6812Drv.cfgb[0].data[0]);
                    PRINTF( "\r\n" );
                }
            }

            if ((g_ltc6812Drv.lastMeasureMs != 0u) &&
                (g_ltc6812Drv.lastMeasureMs != g_ltc6812LastPublishedMeasureMs))
            {
                Ltc6812_PublishSharedSnapshot();
                g_ltc6812LastPublishedMeasureMs = g_ltc6812Drv.lastMeasureMs;
            }
        }
    }
#endif
}

ECU8TR_ADBMS6830_State_t ltc6812_getState(void)
{
    return g_ltc6812State;
}

/************ For test *****************/
#if DO_TEST
volatile static uint8_t ray_6812[13] ={0};
static void ltc6812_test( void )
{
	uint8_t tx[4];
	uint8_t rx[12];

	while( 1 )
	{
		tx[0] = 0x00;
		tx[1] = 0x00;
		tx[2] = 0x00;
		tx[3] = 0x00;

		//Wake up
		g_ltc6812Hal.spiTransfer( tx, rx, 3 );
		//g_ltc6812Hal.delayUs( 1 );
		//g_ltc6812Hal.spiTransfer( tx, rx, 2 );
		g_ltc6812Hal.delayUs( 500 );

		//Read Status Register Group B(RDSTATB)
		tx[0] = 0x00;
		tx[1] = 0x12;
		tx[2] = 0x70;
		tx[3] = 0x24;

		g_ltc6812Hal.spiTransfer( tx, ray_6812, 12 );
		g_ltc6812Hal.delayMs( 3000 );
	}
}
#endif
