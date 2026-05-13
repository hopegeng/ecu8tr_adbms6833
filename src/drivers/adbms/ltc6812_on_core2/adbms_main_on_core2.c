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
#include "../adbms_runtime_detect.h"
#include "ecu8tr_cmd.h"

#define __DO_LTC6812_TEST__				(0)
#define __DO_LTC6812_EEPROM_TEST__		(0)


#define ISR_PRIORITY_STM2_TICK_LTC6812  19
#define LTC6812_CORE2_DEMO_MODE         (0u)
#define LTC6812_QSPI_WAIT_TIMEOUT_LOOPS (1000000u)
#define LTC6812_BW_TEMP_COUNT           (14u)
#define LTC6812_BW_DEW_THRESHOLD_MV     (1500u)
#define LTC6812_EEPROM_TEST_ADDRESS     (0x03F0u)
#define LTC6812_EEPROM_TEST_LEN         (8u)
#define LTC6812_EEPROM_SIZE_BYTES       (1024u)
#define LTC6812_EEPROM_ID_CELL_TYPE_MAJOR_ADDR     (0x0002u)
#define LTC6812_EEPROM_ID_CELL_TYPE_MINOR_ADDR     (0x0003u)
#define LTC6812_EEPROM_ID_MODULE_TYPE_MAJOR_ADDR   (0x0004u)
#define LTC6812_EEPROM_ID_MODULE_TYPE_MINOR_ADDR   (0x0005u)
#define LTC6812_EEPROM_ID_PCB_TYPE_MAJOR_ADDR      (0x000Eu)
#define LTC6812_EEPROM_ID_PCB_TYPE_MINOR_ADDR      (0x000Fu)
#define LTC6812_EEPROM_STATIC1_IC_TYPE_MAJOR_ADDR  (0x001Eu)
#define LTC6812_EEPROM_STATIC1_IC_TYPE_MINOR_ADDR  (0x001Fu)
#define LTC6812_EEPROM_STATIC2_IC_TYPE_MAJOR_ADDR  (0x00BEu)
#define LTC6812_EEPROM_STATIC2_IC_TYPE_MINOR_ADDR  (0x00BFu)
#define LTC6812_EEPROM_ID_MODULE_SERIAL_ADDR        (0x0006u)
#define LTC6812_EEPROM_STATIC1_SERIAL_CELLS_ADDR    (0x0014u)
#define LTC6812_EEPROM_STATIC1_PCB_SERIAL_ADDR      (0x0016u)

#ifndef LTC6812_BALANCE_FORCE_CHARGING_ACTIVE
/* DC3036A bench test hook. Set to 0 for application-controlled charging state. */
#define LTC6812_BALANCE_FORCE_CHARGING_ACTIVE (1u)
#endif

#ifndef LTC6812_BALANCE_DCTO_MINUTES_CODE
#define LTC6812_BALANCE_DCTO_MINUTES_CODE     (1u)
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
static uint8_t g_ltc6812LedOn = 0u;

static Ltc6812_Status_t Ltc6812_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void Ltc6812_DelayUs(uint32_t us);
static void Ltc6812_DelayMs(uint32_t ms);
static void Ltc6812_FillDefaultCfga(Ltc6812_Context_t *ctx);
static void Ltc6812_FillDefaultCfgb(Ltc6812_Context_t *ctx);
static void Ltc6812_InitCore2Runtime(void);
static void Ltc6812_PublishSharedSnapshot(void);
static int16_t Ltc6812_NtcVoltageToTempRaw_0p01C(uint16_t voltage_mV);
static uint16_t Ltc6812_GetExternalTempVoltage_mV(uint8_t tempIndex);
static int16_t Ltc6812_GetExternalTempRaw_0p01C(uint8_t tempIndex);
static uint8_t Ltc6812_GetCellTempIndex(uint8_t afeIdx, uint8_t usedCellIdx);
static uint16_t Ltc6812_GetDewSensor_mV(void);
static Ltc6812_Status_t Ltc6812_SetSystemLed(bool on);
static void Ltc6812_ApplyManualBalanceCommand(void);
static bool Ltc6812_WriteEepromVerified(uint16_t address, const uint8_t *data, uint8_t length);
static void Ltc6812_ProcessEepromRequests(void);
#if LTC6812_CORE2_DEMO_MODE
static void Ltc6812_PublishDemoSnapshot(void);
#endif
#if __DO_LTC6812_EEPROM_TEST__
static void ltc6812_eeprom_test(void);
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

static int16_t Ltc6812_NtcVoltageToTempRaw_0p01C(uint16_t voltage_mV)
{
    typedef struct
    {
        uint16_t voltage_mV;
        int16_t temp_raw_0p01C;
    } Ltc6812_NtcPoint_t;

    static const Ltc6812_NtcPoint_t ntcTable[] =
    {
        {2962u, -4000},
        {2044u,  2500},
        { 529u,  8000},
        { 184u, 12500}
    };
    uint8_t i;

    if (voltage_mV >= ntcTable[0].voltage_mV)
    {
        return ntcTable[0].temp_raw_0p01C;
    }

    for (i = 0u; i < ((uint8_t)(sizeof(ntcTable) / sizeof(ntcTable[0])) - 1u); i++)
    {
        const Ltc6812_NtcPoint_t *hi = &ntcTable[i];
        const Ltc6812_NtcPoint_t *lo = &ntcTable[i + 1u];

        if (voltage_mV >= lo->voltage_mV)
        {
            int32_t voltageSpan_mV = (int32_t)hi->voltage_mV - (int32_t)lo->voltage_mV;
            int32_t tempSpan_raw = (int32_t)lo->temp_raw_0p01C - (int32_t)hi->temp_raw_0p01C;
            int32_t voltageDrop_mV = (int32_t)hi->voltage_mV - (int32_t)voltage_mV;

            return (int16_t)((int32_t)hi->temp_raw_0p01C +
                             ((voltageDrop_mV * tempSpan_raw) / voltageSpan_mV));
        }
    }

    return ntcTable[(sizeof(ntcTable) / sizeof(ntcTable[0])) - 1u].temp_raw_0p01C;
}

static uint16_t Ltc6812_GetExternalTempVoltage_mV(uint8_t tempIndex)
{
    static const uint8_t tempIc[LTC6812_BW_TEMP_COUNT] =
    {
        0u, 0u, 0u, 0u, 0u, 0u, 0u,
        1u, 1u, 1u, 1u, 1u, 1u, 1u
    };
    static const uint8_t tempGpio[LTC6812_BW_TEMP_COUNT] =
    {
        1u, 2u, 3u, 6u, 7u, 8u, 9u,
        1u, 2u, 3u, 6u, 7u, 8u, 9u
    };
    uint8_t ic;
    uint8_t gpio;

    if (tempIndex >= LTC6812_BW_TEMP_COUNT)
    {
        return 0u;
    }

    ic = tempIc[tempIndex];
    gpio = tempGpio[tempIndex];

    if ((ic >= g_ltc6812Drv.icCount) || (gpio == 0u) || (gpio > LTC6812_AUX_CHANNELS_PER_IC))
    {
        return 0u;
    }

    return g_ltc6812Drv.aux[ic].mV[gpio - 1u];
}

static int16_t Ltc6812_GetExternalTempRaw_0p01C(uint8_t tempIndex)
{
    return Ltc6812_NtcVoltageToTempRaw_0p01C(Ltc6812_GetExternalTempVoltage_mV(tempIndex));
}

static uint8_t Ltc6812_GetCellTempIndex(uint8_t afeIdx, uint8_t usedCellIdx)
{
    static const uint8_t sideTempIndexByCellPair[5] =
    {
        2u,  /* GPIO3: SideLeft/SideRight1 -> cell 1, 2 */
        3u,  /* GPIO6: SideLeft/SideRight2 -> cell 3, 4 */
        4u,  /* GPIO7: SideLeft/SideRight3 -> cell 5, 6 */
        5u,  /* GPIO8: SideLeft/SideRight4 -> cell 7, 8 */
        6u   /* GPIO9: SideLeft/SideRight5 -> cell 9, 10 */
    };
    uint8_t pairIdx;

    if ((afeIdx >= LTC6812_SHARED_AFE_COUNT) || (usedCellIdx >= LTC6812_SHARED_USED_CELLS_PER_AFE))
    {
        return 0xFFu;
    }

    pairIdx = (uint8_t)(usedCellIdx / 2u);
    return (uint8_t)(((uint8_t)afeIdx * 7u) + sideTempIndexByCellPair[pairIdx]);
}

static uint16_t Ltc6812_GetDewSensor_mV(void)
{
    if (g_ltc6812Drv.icCount <= 1u)
    {
        return 0u;
    }

    return g_ltc6812Drv.aux[1].mV[3u];
}

static void Ltc6812_ReadIcStatusInputs(void)
{
    Ltc6812_Status_t st;

    st = Ltc6812_StartStatusConversion(&g_ltc6812Hal, &g_ltc6812Cmds);
    if (st != LTC6812_OK)
    {
        LTC6812_PRINT_DEBUG_STATUS(st);
        return;
    }

    if (g_ltc6812Hal.delayMs != 0)
    {
        g_ltc6812Hal.delayMs(10u);
    }

    Ltc6812_WakeUp( &g_ltc6812Drv, &g_ltc6812Hal );

    st = Ltc6812_ReadStatus(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds);
    if (st != LTC6812_OK)
    {
        LTC6812_PRINT_DEBUG_STATUS(st);
    }
}

static Ltc6812_Status_t Ltc6812_SetSystemLed(bool on)
{
    Ltc6812_Status_t st;

    st = Ltc6812_SetBoardLed(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds, on);
    if (st == LTC6812_OK)
    {
        g_ltc6812LedOn = on ? 1u : 0u;
        g_ltc6812Drv.lastCommMs = g_ltc6812Drv.tickMs;
    }

    return st;
}

static uint16_t Ltc6812_ReadAppliedDccMaskFromCfg(uint8_t afeIdx)
{
    if (afeIdx >= g_ltc6812Drv.icCount)
    {
        return 0u;
    }

    return (uint16_t)((uint16_t)g_ltc6812Drv.cfga[afeIdx].data[4] |
                      (((uint16_t)g_ltc6812Drv.cfga[afeIdx].data[5] & 0x0Fu) << 8u) |
                      (((uint16_t)g_ltc6812Drv.cfgb[afeIdx].data[0] & 0x70u) << 8u));
}

static void Ltc6812_ApplyManualBalanceCommand(void)
{
    uint32_t mask;
    uint8_t afeIdx;

    if (AdbmsRuntime_IsManualBalanceEnabled() == false)
    {
        return;
    }

    mask = AdbmsRuntime_GetBalanceMask();
    for (afeIdx = 0u; afeIdx < g_ltc6812Drv.icCount; afeIdx++)
    {
        uint8_t usedCellIdx;
        uint16_t dccMask = 0u;

        for (usedCellIdx = 0u; usedCellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; usedCellIdx++)
        {
            uint8_t globalCellIdx = (uint8_t)((afeIdx * LTC6812_SHARED_USED_CELLS_PER_AFE) + usedCellIdx);
            uint8_t driverCellIdx = (uint8_t)(LTC6812_SHARED_FIRST_USED_CELL_0BASED + usedCellIdx);

            if ((mask & (1UL << globalCellIdx)) != 0u)
            {
                dccMask |= (uint16_t)(1u << driverCellIdx);
            }
        }

        g_ltc6812Bal.result[afeIdx].dccMask = dccMask;
        g_ltc6812Bal.result[afeIdx].selectedParity = LTC6812_BALANCE_PARITY_NONE;
    }
}

static bool Ltc6812_WriteEepromVerified(uint16_t address, const uint8_t *data, uint8_t length)
{
    uint8_t verify[4];
    Ltc6812_Status_t st;

    if ((data == 0) || (length == 0u) || (length > sizeof(verify)) ||
        (((uint32_t)address + (uint32_t)length) > LTC6812_EEPROM_SIZE_BYTES))
    {
        return false;
    }

    st = Ltc6812_WakeUp(&g_ltc6812Drv, &g_ltc6812Hal);
    if (st == LTC6812_OK)
    {
        st = Ltc6812_EepromWrite(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds, address, data, length);
    }
    if (st == LTC6812_OK)
    {
        st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds, address, verify, length);
    }

    if ((st != LTC6812_OK) || (memcmp(data, verify, length) != 0))
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        LTC6812_PRINT_DEBUG_STATUS(st);
        return false;
    }

    g_ltc6812Drv.lastCommMs = g_ltc6812Drv.tickMs;
    return true;
}

static void Ltc6812_ProcessEepromRequests(void)
{
    AdbmsRuntime_EepromRequest_t request;
    uint8_t data[2];
    bool ok = false;

    if (AdbmsRuntime_TakeEepromRequest(&request) == false)
    {
        return;
    }

    if (request.kind == ADBMS_RUNTIME_EEPROM_REQ_WRITE)
    {
        ok = Ltc6812_WriteEepromVerified(request.address, request.data, request.length);
    }
    else if (request.kind == ADBMS_RUNTIME_EEPROM_REQ_CONFIG)
    {
        data[0] = request.major;
        data[1] = request.minor;

        switch (request.config_kind)
        {
            case ADBMS_RUNTIME_LTC_CONFIG_CELL_TYPE:
                ok = Ltc6812_WriteEepromVerified(LTC6812_EEPROM_ID_CELL_TYPE_MAJOR_ADDR, data, 2u);
                break;

            case ADBMS_RUNTIME_LTC_CONFIG_MODULE_TYPE:
                ok = Ltc6812_WriteEepromVerified(LTC6812_EEPROM_ID_MODULE_TYPE_MAJOR_ADDR, data, 2u);
                break;

            case ADBMS_RUNTIME_LTC_CONFIG_PCB_TYPE:
                ok = Ltc6812_WriteEepromVerified(LTC6812_EEPROM_ID_PCB_TYPE_MAJOR_ADDR, data, 2u);
                break;

            case ADBMS_RUNTIME_LTC_CONFIG_IC_TYPE:
                ok = Ltc6812_WriteEepromVerified(LTC6812_EEPROM_STATIC1_IC_TYPE_MAJOR_ADDR, data, 2u);
                if (ok == true)
                {
                    ok = Ltc6812_WriteEepromVerified(LTC6812_EEPROM_STATIC2_IC_TYPE_MAJOR_ADDR, data, 2u);
                }
                break;

            default:
                ok = false;
                break;
        }
    }
    else if (request.kind == ADBMS_RUNTIME_EEPROM_REQ_READ)
    {
        uint8_t readData[4] = {0u, 0u, 0u, 0u};
        Ltc6812_Status_t st;

        st = Ltc6812_WakeUp(&g_ltc6812Drv, &g_ltc6812Hal);
        if (st == LTC6812_OK)
        {
            st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                                    request.address, readData, request.length);
        }
        if (st == LTC6812_OK)
        {
            AdbmsRuntime_PublishEepromReadResult(request.address, readData, request.length);
            ok = true;
        }
        else
        {
            g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
            LTC6812_PRINT_DEBUG_STATUS(st);
        }
    }

    if (ok == true)
    {
        LTC6812_DEBUG_PRINTF("LTC6812 EEPROM request OK kind=%u addr=0x%04X len=%u\r\n",
                             (uint32_t)request.kind,
                             (uint32_t)request.address,
                             (uint32_t)request.length);
    }
    else
    {
        LTC6812_DEBUG_PRINTF("LTC6812 EEPROM request FAIL kind=%u addr=0x%04X len=%u\r\n",
                             (uint32_t)request.kind,
                             (uint32_t)request.address,
                             (uint32_t)request.length);
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
        case LTC6812_SVC_START_AUX_MEASURE:
            return "START_AUX";
        case LTC6812_SVC_WAIT_AUX_MEASURE:
            return "WAIT_AUX";
        case LTC6812_SVC_READ_AUX_RESULTS:
            return "READ_AUX";
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
        case LTC6812_DIAG_ADAX:
            return "ADAX";
        case LTC6812_DIAG_ADSTAT:
            return "ADSTAT";
        case LTC6812_DIAG_RDCV:
            return "RDCV";
        case LTC6812_DIAG_RDCV_PEC:
            return "RDCV_PEC";
        case LTC6812_DIAG_RDAUX:
            return "RDAUX";
        case LTC6812_DIAG_RDAUX_PEC:
            return "RDAUX_PEC";
        case LTC6812_DIAG_RDSTAT:
            return "RDSTAT";
        case LTC6812_DIAG_RDSTAT_PEC:
            return "RDSTAT_PEC";
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
    uint8_t icIdx;
    uint8_t tempIdx;

    g_ltc6812Shared.sequence = 0u;
    g_ltc6812Shared.snapshot.sample_counter = 0u;
    g_ltc6812Shared.snapshot.sample_timestamp_ms = 0u;
    g_ltc6812Shared.snapshot.valid = false;

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            g_ltc6812Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx] = 0u;
            g_ltc6812Shared.snapshot.gpio_voltage_mV[afeIdx][cellIdx] = 0u;
            g_ltc6812Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = 0;
            g_ltc6812Shared.snapshot.balancing[afeIdx][cellIdx] = 0u;
        }
    }

    for (tempIdx = 0u; tempIdx < LTC6812_SHARED_EXTERNAL_TEMP_COUNT; tempIdx++)
    {
        g_ltc6812Shared.snapshot.external_temp_raw_0p01C[tempIdx] = 0;
        g_ltc6812Shared.snapshot.external_temp_voltage_mV[tempIdx] = 0u;
    }

    for (icIdx = 0u; icIdx < LTC6812_SHARED_IC_STATUS_COUNT; icIdx++)
    {
        g_ltc6812Shared.snapshot.ic_cell_sum_raw_0p01V[icIdx] = 0u;
        g_ltc6812Shared.snapshot.ic_internal_temp_raw_0p01C[icIdx] = 0;
        g_ltc6812Shared.snapshot.ic_status_valid[icIdx] = 0u;
    }

    g_ltc6812Shared.snapshot.dew_sensor_mV = 0u;
    g_ltc6812Shared.snapshot.dew_sensor_state = 0u;
    g_ltc6812Shared.snapshot.led_on = 0u;
}

void Ltc6812_SharedPublish(const Ltc6812_SharedSnapshot_t *snapshot)
{
    uint8_t afeIdx;
    uint8_t cellIdx;
    uint8_t icIdx;
    uint8_t tempIdx;

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
            g_ltc6812Shared.snapshot.gpio_voltage_mV[afeIdx][cellIdx] = snapshot->gpio_voltage_mV[afeIdx][cellIdx];
            g_ltc6812Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = snapshot->cell_temp_raw_0p01C[afeIdx][cellIdx];
            g_ltc6812Shared.snapshot.balancing[afeIdx][cellIdx] = snapshot->balancing[afeIdx][cellIdx];
        }
    }

    for (tempIdx = 0u; tempIdx < LTC6812_SHARED_EXTERNAL_TEMP_COUNT; tempIdx++)
    {
        g_ltc6812Shared.snapshot.external_temp_raw_0p01C[tempIdx] = snapshot->external_temp_raw_0p01C[tempIdx];
        g_ltc6812Shared.snapshot.external_temp_voltage_mV[tempIdx] = snapshot->external_temp_voltage_mV[tempIdx];
    }

    for (icIdx = 0u; icIdx < LTC6812_SHARED_IC_STATUS_COUNT; icIdx++)
    {
        g_ltc6812Shared.snapshot.ic_cell_sum_raw_0p01V[icIdx] = snapshot->ic_cell_sum_raw_0p01V[icIdx];
        g_ltc6812Shared.snapshot.ic_internal_temp_raw_0p01C[icIdx] = snapshot->ic_internal_temp_raw_0p01C[icIdx];
        g_ltc6812Shared.snapshot.ic_status_valid[icIdx] = snapshot->ic_status_valid[icIdx];
    }

    g_ltc6812Shared.snapshot.dew_sensor_mV = snapshot->dew_sensor_mV;
    g_ltc6812Shared.snapshot.dew_sensor_state = snapshot->dew_sensor_state;
    g_ltc6812Shared.snapshot.led_on = snapshot->led_on;

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
    uint8_t icIdx;
    uint8_t tempIdx;

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
                snapshot->gpio_voltage_mV[afeIdx][cellIdx] = g_ltc6812Shared.snapshot.gpio_voltage_mV[afeIdx][cellIdx];
                snapshot->cell_temp_raw_0p01C[afeIdx][cellIdx] = g_ltc6812Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx];
                snapshot->balancing[afeIdx][cellIdx] = g_ltc6812Shared.snapshot.balancing[afeIdx][cellIdx];
            }
        }

        for (tempIdx = 0u; tempIdx < LTC6812_SHARED_EXTERNAL_TEMP_COUNT; tempIdx++)
        {
            snapshot->external_temp_raw_0p01C[tempIdx] = g_ltc6812Shared.snapshot.external_temp_raw_0p01C[tempIdx];
            snapshot->external_temp_voltage_mV[tempIdx] = g_ltc6812Shared.snapshot.external_temp_voltage_mV[tempIdx];
        }

        for (icIdx = 0u; icIdx < LTC6812_SHARED_IC_STATUS_COUNT; icIdx++)
        {
            snapshot->ic_cell_sum_raw_0p01V[icIdx] = g_ltc6812Shared.snapshot.ic_cell_sum_raw_0p01V[icIdx];
            snapshot->ic_internal_temp_raw_0p01C[icIdx] = g_ltc6812Shared.snapshot.ic_internal_temp_raw_0p01C[icIdx];
            snapshot->ic_status_valid[icIdx] = g_ltc6812Shared.snapshot.ic_status_valid[icIdx];
        }

        snapshot->dew_sensor_mV = g_ltc6812Shared.snapshot.dew_sensor_mV;
        snapshot->dew_sensor_state = g_ltc6812Shared.snapshot.dew_sensor_state;
        snapshot->led_on = g_ltc6812Shared.snapshot.led_on;

        Ltc6812_SharedMemoryBarrier();
        seqEnd = g_ltc6812Shared.sequence;
    } while ((seqStart != seqEnd) || ((seqEnd & 0x1u) != 0u));

    return true;
}

bool Ltc6812_SetLed(bool on)
{
    return (Ltc6812_SetSystemLed(on) == LTC6812_OK) ? true : false;
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
        ctx->cfga[ic].data[0] = 0xFE;
        ctx->cfga[ic].data[1] = 0x00u;
        ctx->cfga[ic].data[2] = 0x00u;
        ctx->cfga[ic].data[3] = 0x00u;
        ctx->cfga[ic].data[4] = 0x00u;
        ctx->cfga[ic].data[5] = 0x00u;	//(uint8_t)((LTC6812_BALANCE_DCTO_MINUTES_CODE & 0x0Fu) << 4u);
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
    uint8_t icIdx;
    uint8_t tempIdx;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.sample_counter = ++g_ltc6812SampleCounter;
    snapshot.sample_timestamp_ms = g_ltc6812Drv.lastMeasureMs;
    snapshot.valid = (g_ltc6812State == ECU8TR_ADBMS6830_OK);

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (usedCellIdx = 0u; usedCellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; usedCellIdx++)
        {
            uint8_t driverCellIdx = (uint8_t)(LTC6812_SHARED_FIRST_USED_CELL_0BASED + usedCellIdx);
            uint8_t cellTempIdx = Ltc6812_GetCellTempIndex(afeIdx, usedCellIdx);

            if (cellTempIdx != 0xFFu)
            {
                snapshot.cell_temp_raw_0p01C[afeIdx][usedCellIdx] = Ltc6812_GetExternalTempRaw_0p01C(cellTempIdx);
            }
            else
            {
                snapshot.cell_temp_raw_0p01C[afeIdx][usedCellIdx] = 0;
            }
            if (afeIdx < g_ltc6812Drv.icCount)
            {
                uint16_t dccMask = Ltc6812_ReadAppliedDccMaskFromCfg(afeIdx);

                snapshot.cell_voltage_mV[afeIdx][usedCellIdx] = g_ltc6812Drv.cell[afeIdx].mV[driverCellIdx];
                snapshot.gpio_voltage_mV[afeIdx][usedCellIdx] =
                    (cellTempIdx != 0xFFu) ? Ltc6812_GetExternalTempVoltage_mV(cellTempIdx) : 0u;
                snapshot.balancing[afeIdx][usedCellIdx] = ((dccMask & ((uint16_t)1u << driverCellIdx)) != 0u) ? 1u : 0u;
            }
            else
            {
                snapshot.cell_voltage_mV[afeIdx][usedCellIdx] = 0u;
                snapshot.gpio_voltage_mV[afeIdx][usedCellIdx] = 0u;
                snapshot.balancing[afeIdx][usedCellIdx] = 0u;
            }
        }
    }

    for (tempIdx = 0u; tempIdx < LTC6812_SHARED_EXTERNAL_TEMP_COUNT; tempIdx++)
    {
        snapshot.external_temp_voltage_mV[tempIdx] = Ltc6812_GetExternalTempVoltage_mV(tempIdx);
        snapshot.external_temp_raw_0p01C[tempIdx] = Ltc6812_GetExternalTempRaw_0p01C(tempIdx);
    }

    for (icIdx = 0u; icIdx < LTC6812_SHARED_IC_STATUS_COUNT; icIdx++)
    {
        if ((icIdx < g_ltc6812Drv.icCount) && (g_ltc6812Drv.status[icIdx].valid == true))
        {
            snapshot.ic_cell_sum_raw_0p01V[icIdx] = g_ltc6812Drv.status[icIdx].sumCellRaw_0p01V;
            snapshot.ic_internal_temp_raw_0p01C[icIdx] = g_ltc6812Drv.status[icIdx].internalTempRaw_0p01C;
            snapshot.ic_status_valid[icIdx] = 1u;
        }
        else
        {
            snapshot.ic_cell_sum_raw_0p01V[icIdx] = 0u;
            snapshot.ic_internal_temp_raw_0p01C[icIdx] = 0;
            snapshot.ic_status_valid[icIdx] = 0u;
        }
    }

    snapshot.dew_sensor_mV = Ltc6812_GetDewSensor_mV();
    snapshot.dew_sensor_state = (snapshot.dew_sensor_mV >= LTC6812_BW_DEW_THRESHOLD_MV) ? 1u : 0u;
    snapshot.led_on = g_ltc6812LedOn;

    Ltc6812_SharedPublish(&snapshot);
}

#if LTC6812_CORE2_DEMO_MODE
static void Ltc6812_PublishDemoSnapshot(void)
{
    Ltc6812_SharedSnapshot_t snapshot;
    uint8_t afeIdx;
    uint8_t cellIdx;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.sample_counter = ++g_ltc6812SampleCounter;
    snapshot.sample_timestamp_ms = g_ltc6812SysTickMs;
    snapshot.valid = true;

    for (afeIdx = 0u; afeIdx < LTC6812_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < LTC6812_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            snapshot.cell_voltage_mV[afeIdx][cellIdx] = (uint16_t)(3650u + ((uint16_t)afeIdx * 20u) + cellIdx);
            snapshot.gpio_voltage_mV[afeIdx][cellIdx] = (uint16_t)(900u + ((uint16_t)afeIdx * 100u) + cellIdx);
            snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = 0;
            snapshot.balancing[afeIdx][cellIdx] = 0u;
        }

        snapshot.ic_cell_sum_raw_0p01V[afeIdx] = (uint16_t)(3650u + ((uint16_t)afeIdx * 20u));
        snapshot.ic_internal_temp_raw_0p01C[afeIdx] = (int16_t)(2500 + ((int16_t)afeIdx * 100));
        snapshot.ic_status_valid[afeIdx] = 1u;
    }

    g_ltc6812State = ECU8TR_ADBMS6830_OK;
    g_ltc6812LastPublishedMeasureMs = snapshot.sample_timestamp_ms;
    Ltc6812_SharedPublish(&snapshot);
}
#endif





static void Ltc6812_ReadAndPublishEepromCache(void)
{
    AdbmsRuntime_EepromCache_t cache;
    Ltc6812_Status_t st;
    uint8_t buf[8];

    (void)memset(&cache, 0, sizeof(cache));

    st = Ltc6812_WakeUp(&g_ltc6812Drv, &g_ltc6812Hal);
    if (st != LTC6812_OK)
    {
        LTC6812_DEBUG_PRINTF("LTC6812 EEPROM cache: wake failed\r\n");
        return;
    }

    /* cell_type_major, cell_type_minor (2 bytes at 0x0002) */
    st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                             LTC6812_EEPROM_ID_CELL_TYPE_MAJOR_ADDR, buf, 2u);
    if (st == LTC6812_OK)
    {
        cache.cell_type_major = buf[0];
        cache.cell_type_minor = buf[1];
    }

    /* module_type_major, module_type_minor (2 bytes at 0x0004) */
    st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                             LTC6812_EEPROM_ID_MODULE_TYPE_MAJOR_ADDR, buf, 2u);
    if (st == LTC6812_OK)
    {
        cache.module_type_major = buf[0];
        cache.module_type_minor = buf[1];
    }

    /* module_serial (8 bytes at 0x0006) */
    (void)Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                              LTC6812_EEPROM_ID_MODULE_SERIAL_ADDR, cache.module_serial, 8u);

    /* pcb_type_major, pcb_type_minor (2 bytes at 0x000E) */
    st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                             LTC6812_EEPROM_ID_PCB_TYPE_MAJOR_ADDR, buf, 2u);
    if (st == LTC6812_OK)
    {
        cache.pcb_type_major = buf[0];
        cache.pcb_type_minor = buf[1];
    }

    /* serial_cells, parallel_cells (2 bytes at 0x0014) */
    st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                             LTC6812_EEPROM_STATIC1_SERIAL_CELLS_ADDR, buf, 2u);
    if (st == LTC6812_OK)
    {
        cache.serial_cells   = buf[0];
        cache.parallel_cells = buf[1];
    }

    /* pcb_serial (8 bytes at 0x0016) */
    (void)Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                              LTC6812_EEPROM_STATIC1_PCB_SERIAL_ADDR, cache.pcb_serial, 8u);

    /* ic_type_major, ic_type_minor (2 bytes at 0x001E) */
    st = Ltc6812_EepromRead(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds,
                             LTC6812_EEPROM_STATIC1_IC_TYPE_MAJOR_ADDR, buf, 2u);
    if (st == LTC6812_OK)
    {
        cache.ic_type_major = buf[0];
        cache.ic_type_minor = buf[1];
    }

    cache.valid = true;
    AdbmsRuntime_PublishEepromCache(&cache);

    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM cache: cell=%u.%u module=%u.%u pcb=%u.%u ic=%u.%u cells=%u par=%u\r\n",
                          (uint32_t)cache.cell_type_major, (uint32_t)cache.cell_type_minor,
                          (uint32_t)cache.module_type_major, (uint32_t)cache.module_type_minor,
                          (uint32_t)cache.pcb_type_major, (uint32_t)cache.pcb_type_minor,
                          (uint32_t)cache.ic_type_major, (uint32_t)cache.ic_type_minor,
                          (uint32_t)cache.serial_cells, (uint32_t)cache.parallel_cells);
}

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
    //qspi0mstr_Init_iLLD();

    g_ltc6812Hal.spiTransfer = Ltc6812_QspiTransfer;
    g_ltc6812Hal.delayUs = Ltc6812_DelayUs;
    g_ltc6812Hal.delayMs = Ltc6812_DelayMs;

    Ltc6812_Init(&g_ltc6812Drv, LTC6812_SHARED_AFE_COUNT);
    Ltc6812_SetDefaultCommands(&g_ltc6812Cmds);
    Ltc6812_BalanceInit(&g_ltc6812Bal);
    Ltc6812_FillDefaultCfga(&g_ltc6812Drv);
    Ltc6812_FillDefaultCfgb(&g_ltc6812Drv);

    g_ltc6812Bal.cfg.chargingActive = (LTC6812_BALANCE_FORCE_CHARGING_ACTIVE != 0u);
    g_ltc6812Bal.cfg.faultActive = false;
    g_ltc6812Drv.svcState = LTC6812_SVC_BOOT;

    Ltc6812_ReadAndPublishEepromCache();

#if __DO_LTC6812_TEST__
    ltc6812_test();
#endif

#if __DO_LTC6812_EEPROM_TEST__
    while( 1 )
    {
    	ltc6812_eeprom_test();
    	Ltc6812_DelayMs( 2000 );
    }

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

            Ltc6812_ProcessEepromRequests();

#if (ADBMS_BALANCE_CONTROL_MODE == ADBMS_BALANCE_CONTROL_CELL_VOLTAGE)
            Ltc6812_BalanceEvaluate(&g_ltc6812Bal, &g_ltc6812Drv);
#else
            Ltc6812_ApplyManualBalanceCommand();
#endif

            if (balanceDecisionFresh == true)
            {
                Ltc6812_ReadIcStatusInputs();
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
                (
#if (ADBMS_BALANCE_CONTROL_MODE == ADBMS_BALANCE_CONTROL_CELL_VOLTAGE)
                 (g_ltc6812Bal.cfg.chargingActive == true)
#else
                 (AdbmsRuntime_IsManualBalanceEnabled() == true)
#endif
                ) &&
                (Ltc6812_BalanceHasAnyDccSet(&g_ltc6812Bal, g_ltc6812Drv.icCount) == true))
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
                    Ltc6812_BalanceUpdateLastAppliedDcc(&g_ltc6812Bal, g_ltc6812Drv.icCount);
                    (void)Ltc6812_ReadCfga(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds);
                    (void)Ltc6812_ReadCfgb(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds);
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
#if __DO_LTC6812_TEST__
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

#if __DO_LTC6812_EEPROM_TEST__
static void ltc6812_eeprom_test(void)
{
    static const uint8_t patternA[LTC6812_EEPROM_TEST_LEN] =
    {
        0xA5u, 0x5Au, 0x12u, 0x34u, 0x56u, 0x78u, 0xC3u, 0x3Cu
    };
    static const uint8_t patternB[LTC6812_EEPROM_TEST_LEN] =
    {
        0x00u, 0xFFu, 0x55u, 0xAAu, 0x33u, 0xCCu, 0x0Fu, 0xF0u
    };
    uint8_t original[LTC6812_EEPROM_TEST_LEN];
    uint8_t verify[LTC6812_EEPROM_TEST_LEN];
    Ltc6812_Status_t status;
    uint8_t i;
    bool match;

    (void)memset(original, 0, sizeof(original));
    (void)memset(verify, 0, sizeof(verify));

    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST start addr=0x%04X len=%u\r\n",
                         (uint32_t)LTC6812_EEPROM_TEST_ADDRESS,
                         (uint32_t)LTC6812_EEPROM_TEST_LEN);

    status = Ltc6812_FullInitialize(&g_ltc6812Drv, &g_ltc6812Hal, &g_ltc6812Cmds);
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST init=%s\r\n", Ltc6812_StatusToString(status));
    if (status != LTC6812_OK)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        LTC6812_PRINT_DEBUG_STATUS(status);
        return;
    }

    status = Ltc6812_EepromRead(&g_ltc6812Drv,
                                &g_ltc6812Hal,
                                &g_ltc6812Cmds,
                                LTC6812_EEPROM_TEST_ADDRESS,
                                original,
                                LTC6812_EEPROM_TEST_LEN);
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST read-original=%s data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                         Ltc6812_StatusToString(status),
                         (uint32_t)original[0],
                         (uint32_t)original[1],
                         (uint32_t)original[2],
                         (uint32_t)original[3],
                         (uint32_t)original[4],
                         (uint32_t)original[5],
                         (uint32_t)original[6],
                         (uint32_t)original[7]);
    if (status != LTC6812_OK)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        LTC6812_PRINT_DEBUG_STATUS(status);
        return;
    }

    status = Ltc6812_EepromWrite(&g_ltc6812Drv,
                                 &g_ltc6812Hal,
                                 &g_ltc6812Cmds,
                                 LTC6812_EEPROM_TEST_ADDRESS,
                                 patternA,
                                 LTC6812_EEPROM_TEST_LEN);
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST write-patternA=%s\r\n", Ltc6812_StatusToString(status));
    if (status != LTC6812_OK)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        LTC6812_PRINT_DEBUG_STATUS(status);
        return;
    }

    status = Ltc6812_EepromRead(&g_ltc6812Drv,
                                &g_ltc6812Hal,
                                &g_ltc6812Cmds,
                                LTC6812_EEPROM_TEST_ADDRESS,
                                verify,
                                LTC6812_EEPROM_TEST_LEN);
    match = (status == LTC6812_OK);
    for (i = 0u; i < LTC6812_EEPROM_TEST_LEN; i++)
    {
        if (verify[i] != patternA[i])
        {
            match = false;
        }
    }
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST verify-patternA=%s match=%u data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                         Ltc6812_StatusToString(status),
                         match ? 1u : 0u,
                         (uint32_t)verify[0],
                         (uint32_t)verify[1],
                         (uint32_t)verify[2],
                         (uint32_t)verify[3],
                         (uint32_t)verify[4],
                         (uint32_t)verify[5],
                         (uint32_t)verify[6],
                         (uint32_t)verify[7]);
    if (match == false)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        return;
    }

    status = Ltc6812_EepromWrite(&g_ltc6812Drv,
                                 &g_ltc6812Hal,
                                 &g_ltc6812Cmds,
                                 LTC6812_EEPROM_TEST_ADDRESS,
                                 patternB,
                                 LTC6812_EEPROM_TEST_LEN);
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST write-patternB=%s\r\n", Ltc6812_StatusToString(status));
    if (status != LTC6812_OK)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        LTC6812_PRINT_DEBUG_STATUS(status);
        return;
    }

    status = Ltc6812_EepromRead(&g_ltc6812Drv,
                                &g_ltc6812Hal,
                                &g_ltc6812Cmds,
                                LTC6812_EEPROM_TEST_ADDRESS,
                                verify,
                                LTC6812_EEPROM_TEST_LEN);
    match = (status == LTC6812_OK);
    for (i = 0u; i < LTC6812_EEPROM_TEST_LEN; i++)
    {
        if (verify[i] != patternB[i])
        {
            match = false;
        }
    }
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST verify-patternB=%s match=%u data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                         Ltc6812_StatusToString(status),
                         match ? 1u : 0u,
                         (uint32_t)verify[0],
                         (uint32_t)verify[1],
                         (uint32_t)verify[2],
                         (uint32_t)verify[3],
                         (uint32_t)verify[4],
                         (uint32_t)verify[5],
                         (uint32_t)verify[6],
                         (uint32_t)verify[7]);
    if (match == false)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        return;
    }

    status = Ltc6812_EepromWrite(&g_ltc6812Drv,
                                 &g_ltc6812Hal,
                                 &g_ltc6812Cmds,
                                 LTC6812_EEPROM_TEST_ADDRESS,
                                 original,
                                 LTC6812_EEPROM_TEST_LEN);
    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST restore-original=%s\r\n", Ltc6812_StatusToString(status));
    if (status != LTC6812_OK)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
        LTC6812_PRINT_DEBUG_STATUS(status);
        return;
    }

    status = Ltc6812_EepromRead(&g_ltc6812Drv,
                                &g_ltc6812Hal,
                                &g_ltc6812Cmds,
                                LTC6812_EEPROM_TEST_ADDRESS,
                                verify,
                                LTC6812_EEPROM_TEST_LEN);
    match = (status == LTC6812_OK);
    for (i = 0u; i < LTC6812_EEPROM_TEST_LEN; i++)
    {
        if (verify[i] != original[i])
        {
            match = false;
        }
    }

    LTC6812_DEBUG_PRINTF("LTC6812 EEPROM TEST final=%s restored=%u data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                         Ltc6812_StatusToString(status),
                         match ? 1u : 0u,
                         (uint32_t)verify[0],
                         (uint32_t)verify[1],
                         (uint32_t)verify[2],
                         (uint32_t)verify[3],
                         (uint32_t)verify[4],
                         (uint32_t)verify[5],
                         (uint32_t)verify[6],
                         (uint32_t)verify[7]);

    if (match == false)
    {
        g_ltc6812State = ECU8TR_ADBMS6830_ERROR;
    }
}
#endif
