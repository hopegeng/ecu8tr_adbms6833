#ifndef SRC_DRIVERS_ADBMS_ADBMS_RUNTIME_DETECT_H_
#define SRC_DRIVERS_ADBMS_ADBMS_RUNTIME_DETECT_H_

#include <stdbool.h>
#include <stdint.h>

#include "ecu8tr_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS_RUNTIME_DEVICE_UNKNOWN        (0u)
#define ADBMS_RUNTIME_DEVICE_ADBMS6830      (6830u)
#define ADBMS_RUNTIME_DEVICE_ADBMS6833      (6833u)
#define ADBMS_RUNTIME_DEVICE_LTC6812        (6812u)

#define ADBMS_RUNTIME_SHARED_AFE_COUNT              (2u)
#define ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE     (10u)
#define ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT        (4u)
#define ADBMS_RUNTIME_SHARED_EXTERNAL_TEMP_COUNT    (14u)

#ifndef ADBMS_RUNTIME_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_RUNTIME_SHARED_SAMPLE_PERIOD_MS       (1000u)
#endif

#define ADBMS_BALANCE_CONTROL_M001                  (1u)
#define ADBMS_BALANCE_CONTROL_CELL_VOLTAGE          (2u)

#ifndef ADBMS_BALANCE_CONTROL_MODE
#define ADBMS_BALANCE_CONTROL_MODE                  ADBMS_BALANCE_CONTROL_M001
#endif

#ifndef ADBMS_RUNTIME_ADBMS683X_TARGET
/* Use 6830 for the current bench setup. Change to 6833 when the Gen3.3 CSC is available. */
#define ADBMS_RUNTIME_ADBMS683X_TARGET              ADBMS_RUNTIME_DEVICE_ADBMS6830
#endif

typedef struct
{
    uint32_t sample_counter;
    uint32_t sample_timestamp_ms;
    uint16_t cell_voltage_mV[ADBMS_RUNTIME_SHARED_AFE_COUNT][ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE];
    uint16_t gpio_voltage_mV[ADBMS_RUNTIME_SHARED_AFE_COUNT][ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE];
    int16_t cell_temp_raw_0p01C[ADBMS_RUNTIME_SHARED_AFE_COUNT][ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE];
    uint16_t ic_cell_sum_raw_0p01V[ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT];
    int16_t ic_internal_temp_raw_0p01C[ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT];
    uint8_t ic_status_valid[ADBMS_RUNTIME_SHARED_IC_STATUS_COUNT];
    int16_t external_temp_raw_0p01C[ADBMS_RUNTIME_SHARED_EXTERNAL_TEMP_COUNT];
    uint16_t external_temp_voltage_mV[ADBMS_RUNTIME_SHARED_EXTERNAL_TEMP_COUNT];
    uint16_t dew_sensor_mV;
    uint8_t dew_sensor_state;
    uint8_t led_on;
    uint8_t balancing[ADBMS_RUNTIME_SHARED_AFE_COUNT][ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE];
    bool valid;
} AdbmsRuntime_SharedSnapshot_t;

typedef enum
{
    ADBMS_RUNTIME_EEPROM_REQ_NONE = 0,
    ADBMS_RUNTIME_EEPROM_REQ_WRITE,
    ADBMS_RUNTIME_EEPROM_REQ_CONFIG,
    ADBMS_RUNTIME_EEPROM_REQ_READ
} AdbmsRuntime_EepromRequestKind_t;

typedef struct
{
    bool valid;
    uint16_t address;
    uint8_t length;
    uint8_t data[4];
} AdbmsRuntime_EepromReadResult_t;

typedef enum
{
    ADBMS_RUNTIME_LTC_CONFIG_CELL_TYPE = 0,
    ADBMS_RUNTIME_LTC_CONFIG_IC_TYPE,
    ADBMS_RUNTIME_LTC_CONFIG_MODULE_TYPE,
    ADBMS_RUNTIME_LTC_CONFIG_PCB_TYPE
} AdbmsRuntime_LtcConfigKind_t;

typedef struct
{
    AdbmsRuntime_EepromRequestKind_t kind;
    uint16_t address;
    uint8_t length;
    uint8_t data[4];
    AdbmsRuntime_LtcConfigKind_t config_kind;
    uint8_t major;
    uint8_t minor;
} AdbmsRuntime_EepromRequest_t;

void adbms_runtime_main_on_core2(void);
void AdbmsRuntime_RequestStart(void);
bool AdbmsRuntime_IsStartRequested(void);
void AdbmsRuntime_SetBalanceMask(uint32_t balanceMask20);
uint32_t AdbmsRuntime_GetBalanceMask(void);
bool AdbmsRuntime_IsManualBalanceEnabled(void);
bool AdbmsRuntime_RequestEepromWrite(uint16_t address, const uint8_t *data, uint8_t length);
bool AdbmsRuntime_RequestEepromRead(uint16_t address, uint8_t length);
bool AdbmsRuntime_RequestLtcConfigWrite(AdbmsRuntime_LtcConfigKind_t kind, uint8_t major, uint8_t minor);
bool AdbmsRuntime_TakeEepromRequest(AdbmsRuntime_EepromRequest_t *request);
void AdbmsRuntime_PublishEepromReadResult(uint16_t address, const uint8_t *data, uint8_t length);
bool AdbmsRuntime_TakeEepromReadResult(AdbmsRuntime_EepromReadResult_t *result);
bool AdbmsRuntime_SharedRead(AdbmsRuntime_SharedSnapshot_t *snapshot);
ECU8TR_ADBMS6830_State_t AdbmsRuntime_GetState(void);
uint16_t AdbmsRuntime_GetDetectedDevice(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ADBMS_ADBMS_RUNTIME_DETECT_H_ */
