#include "adbms_runtime_detect.h"

#include <string.h>

#include "tools.h"
#include "qspi.h"
#include "qspi0mstr_illd.h"
#include "adbms6830_on_core2/adbms6830_reg.h"
#include "adbms6830_on_core2/adbms6830_shared.h"
#include "adbms6833_on_core2/adbms6833_shared.h"
#include "ltc6812_on_core2/ltc6812_reg.h"
#include "ltc6812_on_core2/ltc6812_pec.h"
#include "ltc6812_on_core2/ltc6812_shared.h"

#define ADBMS_RUNTIME_CMD_FRAME_SIZE        (4u)
#define ADBMS_RUNTIME_REG_GROUP_DATA_LEN    (6u)
#define ADBMS_RUNTIME_DATA_PEC_SIZE         (2u)
#define ADBMS_RUNTIME_PROBE_IC_COUNT        (ADBMS6830_SHARED_AFE_COUNT)
#define ADBMS_RUNTIME_PROBE_RETRY_COUNT     (3u)
#define ADBMS_RUNTIME_WAKE_BYTES            (3u)
#define ADBMS_RUNTIME_WAKE_DELAY_US         (500u)
#define ADBMS_RUNTIME_RETRY_DELAY_MS        (100u)
#define ADBMS_RUNTIME_NO_DEVICE_DELAY_MS    (1000u)

extern void adbms6830_main_on_core2(void);
extern void adbms6833_main_on_core2(void);
extern void ltc6812_main_on_core2(void);
extern ECU8TR_ADBMS6830_State_t adbms6830_getState(void);
extern ECU8TR_ADBMS6830_State_t adbms6833_getState(void);
extern ECU8TR_ADBMS6830_State_t ltc6812_getState(void);

static uint16_t g_adbmsRuntimeDetectedDevice = ADBMS_RUNTIME_DEVICE_UNKNOWN;
static volatile bool g_adbmsRuntimeStartRequested = false;
static volatile bool g_adbmsRuntimeManualBalanceEnabled = false;
static volatile uint32_t g_adbmsRuntimeBalanceMask20 = 0u;
static volatile bool g_adbmsRuntimeEepromRequestPending = false;
static volatile bool g_adbmsRuntimeDynAreaA = true;
static volatile AdbmsRuntime_EepromRequest_t g_adbmsRuntimeEepromRequest;
static volatile bool g_adbmsRuntimeEepromReadResultReady = false;
static volatile AdbmsRuntime_EepromReadResult_t g_adbmsRuntimeEepromReadResult;
static volatile AdbmsRuntime_EepromCache_t g_adbmsRuntimeEepromCache;

static bool AdbmsRuntime_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void AdbmsRuntime_WakeIsoSpi(void);
static void AdbmsRuntime_BuildCmdFrame(uint16_t cmd, uint8_t frame[ADBMS_RUNTIME_CMD_FRAME_SIZE]);
static bool AdbmsRuntime_ReadRegister(uint16_t cmd, uint8_t data[ADBMS_RUNTIME_REG_GROUP_DATA_LEN + ADBMS_RUNTIME_DATA_PEC_SIZE]);
static bool AdbmsRuntime_ProbeAdbms683x(void);
static bool AdbmsRuntime_ProbeLtc6812(void);
static uint16_t AdbmsRuntime_DetectDevice(void);
static uint16_t AdbmsRuntime_Pec15Calc(const uint8_t *data, uint16_t len);
static uint16_t AdbmsRuntime_Pec10Calc(const uint8_t *data, uint8_t cmdCounter, uint16_t len);
static bool AdbmsRuntime_VerifyAdbmsPec10(const uint8_t *payload, uint16_t dataLen);
static bool AdbmsRuntime_VerifyLtcPec15(const uint8_t *payload, uint16_t dataLen);
static void AdbmsRuntime_Copy6830Snapshot(AdbmsRuntime_SharedSnapshot_t *dst, const Adbms6830_SharedSnapshot_t *src);
static void AdbmsRuntime_Copy6833Snapshot(AdbmsRuntime_SharedSnapshot_t *dst, const Adbms6833_SharedSnapshot_t *src);
static void AdbmsRuntime_CopyLtc6812Snapshot(AdbmsRuntime_SharedSnapshot_t *dst, const Ltc6812_SharedSnapshot_t *src);

static bool AdbmsRuntime_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    SpiIf_Status retVal;

    if ((tx == 0) || (rx == 0) || (len == 0u))
    {
        return false;
    }

    retVal = qspi0_send_receive_iLLD(eQspiHwCs02, len, (uint8_t *)tx, rx);
    if (retVal != SpiIf_Status_ok)
    {
        return false;
    }

    return qspimstr_waitForRxDone_iLLD();
}

static void AdbmsRuntime_WakeIsoSpi(void)
{
    uint8_t tx[ADBMS_RUNTIME_WAKE_BYTES] = {0u};
    uint8_t rx[ADBMS_RUNTIME_WAKE_BYTES] = {0u};

    (void)AdbmsRuntime_QspiTransfer(tx, rx, ADBMS_RUNTIME_WAKE_BYTES);
    delay_us_stm2(ADBMS_RUNTIME_WAKE_DELAY_US);
}

static void AdbmsRuntime_BuildCmdFrame(uint16_t cmd, uint8_t frame[ADBMS_RUNTIME_CMD_FRAME_SIZE])
{
    uint16_t pec;

    frame[0] = (uint8_t)(cmd >> 8u);
    frame[1] = (uint8_t)(cmd & 0xFFu);
    pec = AdbmsRuntime_Pec15Calc(frame, 2u);
    frame[2] = (uint8_t)(pec >> 8u);
    frame[3] = (uint8_t)(pec & 0xFFu);
}

static bool AdbmsRuntime_ReadRegister(uint16_t cmd, uint8_t data[ADBMS_RUNTIME_REG_GROUP_DATA_LEN + ADBMS_RUNTIME_DATA_PEC_SIZE])
{
    uint8_t tx[ADBMS_RUNTIME_CMD_FRAME_SIZE +
               (ADBMS_RUNTIME_PROBE_IC_COUNT * (ADBMS_RUNTIME_REG_GROUP_DATA_LEN + ADBMS_RUNTIME_DATA_PEC_SIZE))];
    uint8_t rx[sizeof(tx)];
    uint16_t i;

    for (i = 0u; i < sizeof(tx); i++)
    {
        tx[i] = 0u;
        rx[i] = 0u;
    }

    AdbmsRuntime_BuildCmdFrame(cmd, tx);

    if (AdbmsRuntime_QspiTransfer(tx, rx, (uint16_t)sizeof(tx)) == false)
    {
        return false;
    }

    (void)memcpy(data, &rx[ADBMS_RUNTIME_CMD_FRAME_SIZE], ADBMS_RUNTIME_REG_GROUP_DATA_LEN + ADBMS_RUNTIME_DATA_PEC_SIZE);
    return true;
}

static bool AdbmsRuntime_ProbeAdbms683x(void)
{
    uint8_t block[ADBMS_RUNTIME_REG_GROUP_DATA_LEN + ADBMS_RUNTIME_DATA_PEC_SIZE];

    AdbmsRuntime_WakeIsoSpi();

    if (AdbmsRuntime_ReadRegister(ADBMS6830_RDSID_REG, block) == false)
    {
        return false;
    }

    return AdbmsRuntime_VerifyAdbmsPec10(block, ADBMS_RUNTIME_REG_GROUP_DATA_LEN);
}

volatile static uint8_t ray_detect[20] = {0};
static bool AdbmsRuntime_ProbeLtc6812(void)
{
    uint8_t block[ADBMS_RUNTIME_REG_GROUP_DATA_LEN + ADBMS_RUNTIME_DATA_PEC_SIZE];

    AdbmsRuntime_WakeIsoSpi();

    if (AdbmsRuntime_ReadRegister(LTC6812_RDCFGA_REG, block) == false)
    {
        return false;
    }
    memcpy( ray_detect, block, 12 );

    return AdbmsRuntime_VerifyLtcPec15(block, ADBMS_RUNTIME_REG_GROUP_DATA_LEN);
}

static uint16_t AdbmsRuntime_DetectDevice(void)
{
    uint8_t attempt;

    for (attempt = 0u; attempt < ADBMS_RUNTIME_PROBE_RETRY_COUNT; attempt++)
    {
        if (AdbmsRuntime_ProbeAdbms683x() == true)
        {
#if (ADBMS_RUNTIME_ADBMS683X_TARGET == ADBMS_RUNTIME_DEVICE_ADBMS6833)
            return ADBMS_RUNTIME_DEVICE_ADBMS6833;
#else
            return ADBMS_RUNTIME_DEVICE_ADBMS6830;
#endif
        }


        if (AdbmsRuntime_ProbeLtc6812() == true)
        {
            return ADBMS_RUNTIME_DEVICE_LTC6812;
        }

        delay_ms_stm2(ADBMS_RUNTIME_RETRY_DELAY_MS);
    }

    return ADBMS_RUNTIME_DEVICE_UNKNOWN;
}

static uint16_t AdbmsRuntime_Pec15Calc(const uint8_t *data, uint16_t len)
{
    return Ltc6812_Pec15Calc(data, len);
}

static uint16_t AdbmsRuntime_Pec10Calc(const uint8_t *data, uint8_t cmdCounter, uint16_t len)
{
    uint16_t pecCalc = 16u;
    uint16_t pecTemp = 0u;
    uint16_t in0;
    uint16_t in1;
    uint16_t in2;
    uint16_t in3;
    uint16_t in7;
    int16_t i;
    int16_t j;
    static const uint8_t dataBits[8] =
    {
        0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u
    };
    static const uint16_t pecBits[10] =
    {
        0x001u, 0x002u, 0x004u, 0x008u, 0x010u, 0x020u, 0x040u, 0x080u, 0x100u, 0x200u
    };

    for (i = 0; i < (int16_t)len; i++)
    {
        for (j = 7; j >= 0; j--)
        {
            in0 = (uint16_t)((((data[i] & dataBits[j]) != 0u) ? 1u : 0u) ^
                             (((pecCalc & pecBits[9]) != 0u) ? 1u : 0u));
            in1 = (uint16_t)((in0 ^ (((pecCalc & pecBits[0]) != 0u) ? 1u : 0u)) ? 0x02u : 0u);
            in2 = (uint16_t)((in0 ^ (((pecCalc & pecBits[1]) != 0u) ? 1u : 0u)) ? 0x04u : 0u);
            in3 = (uint16_t)((in0 ^ (((pecCalc & pecBits[2]) != 0u) ? 1u : 0u)) ? 0x08u : 0u);
            in7 = (uint16_t)((in0 ^ (((pecCalc & pecBits[6]) != 0u) ? 1u : 0u)) ? 0x80u : 0u);

            pecTemp = (uint16_t)(((pecCalc & pecBits[8]) != 0u) ? pecBits[9] : 0u);
            pecTemp |= (uint16_t)(((pecCalc & pecBits[7]) != 0u) ? pecBits[8] : 0u);
            pecTemp |= in7;
            pecTemp |= (uint16_t)(((pecCalc & pecBits[5]) != 0u) ? pecBits[6] : 0u);
            pecTemp |= (uint16_t)(((pecCalc & pecBits[4]) != 0u) ? pecBits[5] : 0u);
            pecTemp |= (uint16_t)(((pecCalc & pecBits[3]) != 0u) ? pecBits[4] : 0u);
            pecTemp |= in3;
            pecTemp |= in2;
            pecTemp |= in1;
            pecTemp |= in0;

            pecCalc = pecTemp;
        }
    }

    for (j = 5; j >= 0; j--)
    {
        in0 = (uint16_t)((((cmdCounter & dataBits[j]) != 0u) ? 1u : 0u) ^
                         (((pecCalc & pecBits[9]) != 0u) ? 1u : 0u));
        in1 = (uint16_t)((in0 ^ (((pecCalc & pecBits[0]) != 0u) ? 1u : 0u)) ? 0x02u : 0u);
        in2 = (uint16_t)((in0 ^ (((pecCalc & pecBits[1]) != 0u) ? 1u : 0u)) ? 0x04u : 0u);
        in3 = (uint16_t)((in0 ^ (((pecCalc & pecBits[2]) != 0u) ? 1u : 0u)) ? 0x08u : 0u);
        in7 = (uint16_t)((in0 ^ (((pecCalc & pecBits[6]) != 0u) ? 1u : 0u)) ? 0x80u : 0u);

        pecTemp = (uint16_t)(((pecCalc & pecBits[8]) != 0u) ? pecBits[9] : 0u);
        pecTemp |= (uint16_t)(((pecCalc & pecBits[7]) != 0u) ? pecBits[8] : 0u);
        pecTemp |= in7;
        pecTemp |= (uint16_t)(((pecCalc & pecBits[5]) != 0u) ? pecBits[6] : 0u);
        pecTemp |= (uint16_t)(((pecCalc & pecBits[4]) != 0u) ? pecBits[5] : 0u);
        pecTemp |= (uint16_t)(((pecCalc & pecBits[3]) != 0u) ? pecBits[4] : 0u);
        pecTemp |= in3;
        pecTemp |= in2;
        pecTemp |= in1;
        pecTemp |= in0;

        pecCalc = pecTemp;
    }

    return pecCalc;
}

static bool AdbmsRuntime_VerifyAdbmsPec10(const uint8_t *payload, uint16_t dataLen)
{
    uint8_t cmdCounter;
    uint16_t rxPec;
    uint16_t calcPec;

    if (payload == 0)
    {
        return false;
    }

    cmdCounter = (uint8_t)(payload[dataLen] >> 2u);
    rxPec = (uint16_t)((((uint16_t)payload[dataLen] & 0x0003u) << 8u) |
                       (uint16_t)payload[dataLen + 1u]);
    calcPec = AdbmsRuntime_Pec10Calc(payload, cmdCounter, dataLen);

    return (rxPec == calcPec);
}

static bool AdbmsRuntime_VerifyLtcPec15(const uint8_t *payload, uint16_t dataLen)
{
    uint16_t rxPec;
    uint16_t calcPec;

    if (payload == 0)
    {
        return false;
    }

    rxPec = (uint16_t)(((uint16_t)payload[dataLen] << 8u) |
                       (uint16_t)payload[dataLen + 1u]);
    calcPec = AdbmsRuntime_Pec15Calc(payload, dataLen);

    return (rxPec == calcPec);
}

static void AdbmsRuntime_Copy6830Snapshot(AdbmsRuntime_SharedSnapshot_t *dst, const Adbms6830_SharedSnapshot_t *src)
{
    uint8_t afeIdx;
    uint8_t cellIdx;
    uint8_t icIdx;

    (void)memset(dst, 0, sizeof(*dst));
    dst->sample_counter = src->sample_counter;
    dst->sample_timestamp_ms = src->sample_timestamp_ms;
    dst->valid = src->valid;

    for (afeIdx = 0u; afeIdx < ADBMS_RUNTIME_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            dst->cell_voltage_mV[afeIdx][cellIdx] = src->cell_voltage_mV[afeIdx][cellIdx];
            dst->gpio_voltage_mV[afeIdx][cellIdx] = src->gpio_voltage_mV[afeIdx][cellIdx];
            dst->cell_temp_raw_0p01C[afeIdx][cellIdx] = src->cell_temp_raw_0p01C[afeIdx][cellIdx];
            dst->balancing[afeIdx][cellIdx] = src->balancing[afeIdx][cellIdx];
        }
    }

    for (icIdx = 0u; icIdx < ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT; icIdx++)
    {
        dst->ic_cell_sum_raw_0p01V[icIdx] = 0u;
        dst->ic_internal_temp_raw_0p01C[icIdx] = 0;
        dst->ic_status_valid[icIdx] = 0u;
    }
}

static void AdbmsRuntime_Copy6833Snapshot(AdbmsRuntime_SharedSnapshot_t *dst, const Adbms6833_SharedSnapshot_t *src)
{
    uint8_t afeIdx;
    uint8_t cellIdx;
    uint8_t icIdx;

    (void)memset(dst, 0, sizeof(*dst));
    dst->sample_counter = src->sample_counter;
    dst->sample_timestamp_ms = src->sample_timestamp_ms;
    dst->valid = src->valid;

    for (afeIdx = 0u; afeIdx < ADBMS_RUNTIME_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            dst->cell_voltage_mV[afeIdx][cellIdx] = src->cell_voltage_mV[afeIdx][cellIdx];
            dst->gpio_voltage_mV[afeIdx][cellIdx] = src->gpio_voltage_mV[afeIdx][cellIdx];
            dst->cell_temp_raw_0p01C[afeIdx][cellIdx] = src->cell_temp_raw_0p01C[afeIdx][cellIdx];
            dst->balancing[afeIdx][cellIdx] = src->balancing[afeIdx][cellIdx];
        }
    }

    for (icIdx = 0u; icIdx < ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT; icIdx++)
    {
        dst->ic_cell_sum_raw_0p01V[icIdx] = 0u;
        dst->ic_internal_temp_raw_0p01C[icIdx] = 0;
        dst->ic_status_valid[icIdx] = 0u;
    }
}

static void AdbmsRuntime_CopyLtc6812Snapshot(AdbmsRuntime_SharedSnapshot_t *dst, const Ltc6812_SharedSnapshot_t *src)
{
    uint8_t afeIdx;
    uint8_t cellIdx;
    uint8_t icIdx;
    uint8_t tempIdx;

    (void)memset(dst, 0, sizeof(*dst));
    dst->sample_counter = src->sample_counter;
    dst->sample_timestamp_ms = src->sample_timestamp_ms;
    dst->valid = src->valid;
    dst->dew_sensor_mV = src->dew_sensor_mV;
    dst->dew_sensor_state = src->dew_sensor_state;
    dst->led_on = src->led_on;

    for (afeIdx = 0u; afeIdx < ADBMS_RUNTIME_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            dst->cell_voltage_mV[afeIdx][cellIdx] = src->cell_voltage_mV[afeIdx][cellIdx];
            dst->gpio_voltage_mV[afeIdx][cellIdx] = src->gpio_voltage_mV[afeIdx][cellIdx];
            dst->cell_temp_raw_0p01C[afeIdx][cellIdx] = src->cell_temp_raw_0p01C[afeIdx][cellIdx];
            dst->balancing[afeIdx][cellIdx] = src->balancing[afeIdx][cellIdx];
        }
    }

    for (icIdx = 0u; icIdx < ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT; icIdx++)
    {
        dst->ic_cell_sum_raw_0p01V[icIdx] = src->ic_cell_sum_raw_0p01V[icIdx];
        dst->ic_internal_temp_raw_0p01C[icIdx] = src->ic_internal_temp_raw_0p01C[icIdx];
        dst->ic_status_valid[icIdx] = src->ic_status_valid[icIdx];
    }

    for (tempIdx = 0u; tempIdx < ADBMS_RUNTIME_SHARED_EXTERNAL_TEMP_COUNT; tempIdx++)
    {
        dst->external_temp_raw_0p01C[tempIdx] = src->external_temp_raw_0p01C[tempIdx];
        dst->external_temp_voltage_mV[tempIdx] = src->external_temp_voltage_mV[tempIdx];
    }
}

void adbms_runtime_main_on_core2(void)
{
    qspi0mstr_Init_iLLD();

    while (g_adbmsRuntimeStartRequested == false)
    {
        delay_ms_stm2(10u);
    }

    while (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_UNKNOWN)
    {
        g_adbmsRuntimeDetectedDevice = AdbmsRuntime_DetectDevice();

        if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_UNKNOWN)
        {
            PRINTF("ADBMS runtime detect: no supported CSC detected, retrying\r\n");
            delay_ms_stm2(ADBMS_RUNTIME_NO_DEVICE_DELAY_MS);
        }
    }

    PRINTF("ADBMS runtime detect: selected device %u\r\n", (uint32_t)g_adbmsRuntimeDetectedDevice);

    if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_LTC6812)
    {
        ltc6812_main_on_core2();
    }
    else if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_ADBMS6833)
    {
        adbms6833_main_on_core2();
    }
    else
    {
        adbms6830_main_on_core2();
    }

    while (1)
    {
    }
}

void AdbmsRuntime_RequestStart(void)
{
    g_adbmsRuntimeStartRequested = true;
}

bool AdbmsRuntime_IsStartRequested(void)
{
    return g_adbmsRuntimeStartRequested;
}

void AdbmsRuntime_SetBalanceMask(uint32_t balanceMask20)
{
    g_adbmsRuntimeBalanceMask20 = (balanceMask20 & 0x000FFFFFUL);
    g_adbmsRuntimeManualBalanceEnabled = true;
}

uint32_t AdbmsRuntime_GetBalanceMask(void)
{
    return g_adbmsRuntimeBalanceMask20;
}

bool AdbmsRuntime_IsManualBalanceEnabled(void)
{
    return g_adbmsRuntimeManualBalanceEnabled;
}

bool AdbmsRuntime_RequestEepromWrite(uint16_t address, const uint8_t *data, uint8_t length)
{
    uint8_t i;

    if ((data == 0) || (length == 0u) || (length > 4u))
    {
        return false;
    }

    if (g_adbmsRuntimeEepromRequestPending == true)
    {
        return false;
    }

    g_adbmsRuntimeEepromRequest.kind = ADBMS_RUNTIME_EEPROM_REQ_WRITE;
    g_adbmsRuntimeEepromRequest.address = address;
    g_adbmsRuntimeEepromRequest.length = length;
    g_adbmsRuntimeEepromRequest.config_kind = ADBMS_RUNTIME_LTC_CONFIG_CELL_TYPE;
    g_adbmsRuntimeEepromRequest.major = 0u;
    g_adbmsRuntimeEepromRequest.minor = 0u;

    for (i = 0u; i < 4u; i++)
    {
        g_adbmsRuntimeEepromRequest.data[i] = (i < length) ? data[i] : 0u;
    }

    __dsync();
    g_adbmsRuntimeEepromRequestPending = true;
    __dsync();

    return true;
}

bool AdbmsRuntime_RequestEepromRead(uint16_t address, uint8_t length)
{
    if ((length == 0u) || (length > 4u))
    {
        return false;
    }

    if (g_adbmsRuntimeEepromRequestPending == true)
    {
        return false;
    }

    g_adbmsRuntimeEepromRequest.kind = ADBMS_RUNTIME_EEPROM_REQ_READ;
    g_adbmsRuntimeEepromRequest.address = address;
    g_adbmsRuntimeEepromRequest.length = length;
    g_adbmsRuntimeEepromRequest.config_kind = ADBMS_RUNTIME_LTC_CONFIG_CELL_TYPE;
    g_adbmsRuntimeEepromRequest.major = 0u;
    g_adbmsRuntimeEepromRequest.minor = 0u;
    g_adbmsRuntimeEepromRequest.data[0] = 0u;
    g_adbmsRuntimeEepromRequest.data[1] = 0u;
    g_adbmsRuntimeEepromRequest.data[2] = 0u;
    g_adbmsRuntimeEepromRequest.data[3] = 0u;

    __dsync();
    g_adbmsRuntimeEepromRequestPending = true;
    __dsync();

    return true;
}

bool AdbmsRuntime_RequestLtcConfigWrite(AdbmsRuntime_LtcConfigKind_t kind, uint8_t major, uint8_t minor)
{
    if (g_adbmsRuntimeEepromRequestPending == true)
    {
        return false;
    }

    if (kind > ADBMS_RUNTIME_LTC_CONFIG_MODULE_SERIAL_HI)
    {
        return false;
    }

    g_adbmsRuntimeEepromRequest.kind = ADBMS_RUNTIME_EEPROM_REQ_CONFIG;
    g_adbmsRuntimeEepromRequest.address = 0u;
    g_adbmsRuntimeEepromRequest.length = 0u;
    g_adbmsRuntimeEepromRequest.data[0] = 0u;
    g_adbmsRuntimeEepromRequest.data[1] = 0u;
    g_adbmsRuntimeEepromRequest.data[2] = 0u;
    g_adbmsRuntimeEepromRequest.data[3] = 0u;
    g_adbmsRuntimeEepromRequest.config_kind = kind;
    g_adbmsRuntimeEepromRequest.major = major;
    g_adbmsRuntimeEepromRequest.minor = minor;

    __dsync();
    g_adbmsRuntimeEepromRequestPending = true;
    __dsync();

    return true;
}

bool AdbmsRuntime_TakeEepromRequest(AdbmsRuntime_EepromRequest_t *request)
{
    if (request == 0)
    {
        return false;
    }

    if (g_adbmsRuntimeEepromRequestPending == false)
    {
        return false;
    }

    __dsync();
    *request = g_adbmsRuntimeEepromRequest;
    __dsync();
    g_adbmsRuntimeEepromRequestPending = false;
    __dsync();

    return true;
}

bool AdbmsRuntime_SharedRead(AdbmsRuntime_SharedSnapshot_t *snapshot)
{
    bool ok = false;

    if (snapshot == 0)
    {
        return false;
    }

    if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_LTC6812)
    {
        Ltc6812_SharedSnapshot_t src;

        ok = Ltc6812_SharedRead(&src);
        if (ok == true)
        {
            AdbmsRuntime_CopyLtc6812Snapshot(snapshot, &src);
        }
    }
    else if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_ADBMS6833)
    {
        Adbms6833_SharedSnapshot_t src;

        ok = Adbms6833_SharedRead(&src);
        if (ok == true)
        {
            AdbmsRuntime_Copy6833Snapshot(snapshot, &src);
        }
    }
    else if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_ADBMS6830)
    {
        Adbms6830_SharedSnapshot_t src;

        ok = Adbms6830_SharedRead(&src);
        if (ok == true)
        {
            AdbmsRuntime_Copy6830Snapshot(snapshot, &src);
        }
    }

    return ok;
}

ECU8TR_ADBMS6830_State_t AdbmsRuntime_GetState(void)
{
    if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_LTC6812)
    {
        return ltc6812_getState();
    }
    else if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_ADBMS6833)
    {
        return adbms6833_getState();
    }
    else if (g_adbmsRuntimeDetectedDevice == ADBMS_RUNTIME_DEVICE_ADBMS6830)
    {
        return adbms6830_getState();
    }

    return ECU8TR_ADBMS6830_IDLE;
}

uint16_t AdbmsRuntime_GetDetectedDevice(void)
{
    return g_adbmsRuntimeDetectedDevice;
}

void AdbmsRuntime_PublishEepromReadResult(uint16_t address, const uint8_t *data, uint8_t length)
{
    uint8_t i;

    if ((data == 0) || (length == 0u) || (length > 4u))
    {
        return;
    }

    g_adbmsRuntimeEepromReadResult.address = address;
    g_adbmsRuntimeEepromReadResult.length = length;
    for (i = 0u; i < 4u; i++)
    {
        g_adbmsRuntimeEepromReadResult.data[i] = (i < length) ? data[i] : 0u;
    }
    __dsync();
    g_adbmsRuntimeEepromReadResult.valid = true;
    __dsync();
    g_adbmsRuntimeEepromReadResultReady = true;
    __dsync();
}

bool AdbmsRuntime_TakeEepromReadResult(AdbmsRuntime_EepromReadResult_t *result)
{
    uint8_t i;

    if (result == 0)
    {
        return false;
    }

    if (g_adbmsRuntimeEepromReadResultReady == false)
    {
        return false;
    }

    __dsync();
    result->valid   = g_adbmsRuntimeEepromReadResult.valid;
    result->address = g_adbmsRuntimeEepromReadResult.address;
    result->length  = g_adbmsRuntimeEepromReadResult.length;
    for (i = 0u; i < 4u; i++)
    {
        result->data[i] = g_adbmsRuntimeEepromReadResult.data[i];
    }
    __dsync();
    g_adbmsRuntimeEepromReadResultReady = false;
    g_adbmsRuntimeEepromReadResult.valid = false;
    __dsync();

    return true;
}

void AdbmsRuntime_PublishEepromCache(const AdbmsRuntime_EepromCache_t *cache)
{
    uint8_t i;

    if (cache == 0)
    {
        return;
    }

    g_adbmsRuntimeEepromCache.valid            = false;
    __dsync();
    g_adbmsRuntimeEepromCache.locking_locked    = cache->locking_locked;
    g_adbmsRuntimeEepromCache.cell_type_major   = cache->cell_type_major;
    g_adbmsRuntimeEepromCache.cell_type_minor   = cache->cell_type_minor;
    g_adbmsRuntimeEepromCache.module_type_major = cache->module_type_major;
    g_adbmsRuntimeEepromCache.module_type_minor = cache->module_type_minor;
    g_adbmsRuntimeEepromCache.pcb_type_major    = cache->pcb_type_major;
    g_adbmsRuntimeEepromCache.pcb_type_minor    = cache->pcb_type_minor;
    g_adbmsRuntimeEepromCache.ic_type_major     = cache->ic_type_major;
    g_adbmsRuntimeEepromCache.ic_type_minor     = cache->ic_type_minor;
    g_adbmsRuntimeEepromCache.serial_cells      = cache->serial_cells;
    g_adbmsRuntimeEepromCache.parallel_cells    = cache->parallel_cells;
    for (i = 0u; i < 8u; i++)
    {
        g_adbmsRuntimeEepromCache.module_serial[i] = cache->module_serial[i];
        g_adbmsRuntimeEepromCache.pcb_serial[i]    = cache->pcb_serial[i];
    }
    __dsync();
    g_adbmsRuntimeEepromCache.valid = true;
    __dsync();
}

bool AdbmsRuntime_GetEepromCache(AdbmsRuntime_EepromCache_t *out)
{
    uint8_t i;

    if (out == 0)
    {
        return false;
    }

    __dsync();
    if (g_adbmsRuntimeEepromCache.valid == false)
    {
        return false;
    }

    out->valid            = true;
    out->locking_locked    = g_adbmsRuntimeEepromCache.locking_locked;
    out->cell_type_major   = g_adbmsRuntimeEepromCache.cell_type_major;
    out->cell_type_minor   = g_adbmsRuntimeEepromCache.cell_type_minor;
    out->module_type_major = g_adbmsRuntimeEepromCache.module_type_major;
    out->module_type_minor = g_adbmsRuntimeEepromCache.module_type_minor;
    out->pcb_type_major    = g_adbmsRuntimeEepromCache.pcb_type_major;
    out->pcb_type_minor    = g_adbmsRuntimeEepromCache.pcb_type_minor;
    out->ic_type_major     = g_adbmsRuntimeEepromCache.ic_type_major;
    out->ic_type_minor     = g_adbmsRuntimeEepromCache.ic_type_minor;
    out->serial_cells      = g_adbmsRuntimeEepromCache.serial_cells;
    out->parallel_cells    = g_adbmsRuntimeEepromCache.parallel_cells;
    for (i = 0u; i < 8u; i++)
    {
        out->module_serial[i] = g_adbmsRuntimeEepromCache.module_serial[i];
        out->pcb_serial[i]    = g_adbmsRuntimeEepromCache.pcb_serial[i];
    }
    __dsync();

    return true;
}

bool AdbmsRuntime_RequestLtcModuleSerialWrite(bool isHi, const uint8_t *bytes4)
{
    uint8_t i;

    if (bytes4 == 0)
    {
        return false;
    }

    if (g_adbmsRuntimeEepromRequestPending == true)
    {
        return false;
    }

    g_adbmsRuntimeEepromRequest.kind = ADBMS_RUNTIME_EEPROM_REQ_CONFIG;
    g_adbmsRuntimeEepromRequest.address = 0u;
    g_adbmsRuntimeEepromRequest.length = 4u;
    g_adbmsRuntimeEepromRequest.config_kind = isHi ?
        ADBMS_RUNTIME_LTC_CONFIG_MODULE_SERIAL_HI :
        ADBMS_RUNTIME_LTC_CONFIG_MODULE_SERIAL_LO;
    g_adbmsRuntimeEepromRequest.major = 0u;
    g_adbmsRuntimeEepromRequest.minor = 0u;
    for (i = 0u; i < 4u; i++)
    {
        g_adbmsRuntimeEepromRequest.data[i] = bytes4[i];
    }

    __dsync();
    g_adbmsRuntimeEepromRequestPending = true;
    __dsync();

    return true;
}

bool AdbmsRuntime_RequestIdPageLock(void)
{
    if (g_adbmsRuntimeEepromRequestPending == true)
    {
        return false;
    }

    g_adbmsRuntimeEepromRequest.kind = ADBMS_RUNTIME_EEPROM_REQ_LOCK;
    g_adbmsRuntimeEepromRequest.address = 0u;
    g_adbmsRuntimeEepromRequest.length = 0u;
    g_adbmsRuntimeEepromRequest.config_kind = ADBMS_RUNTIME_LTC_CONFIG_CELL_TYPE;
    g_adbmsRuntimeEepromRequest.major = 0u;
    g_adbmsRuntimeEepromRequest.minor = 0u;
    g_adbmsRuntimeEepromRequest.data[0] = 0u;
    g_adbmsRuntimeEepromRequest.data[1] = 0u;
    g_adbmsRuntimeEepromRequest.data[2] = 0u;
    g_adbmsRuntimeEepromRequest.data[3] = 0u;

    __dsync();
    g_adbmsRuntimeEepromRequestPending = true;
    __dsync();

    return true;
}

void AdbmsRuntime_SetDynAreaA(bool useAreaA)
{
    g_adbmsRuntimeDynAreaA = useAreaA;
}

bool AdbmsRuntime_GetDynAreaA(void)
{
    return g_adbmsRuntimeDynAreaA;
}
