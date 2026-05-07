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

#ifndef ADBMS_RUNTIME_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_RUNTIME_SHARED_SAMPLE_PERIOD_MS       (5000u)
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
    int16_t cell_temp_raw_0p01C[ADBMS_RUNTIME_SHARED_AFE_COUNT][ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE];
    uint8_t balancing[ADBMS_RUNTIME_SHARED_AFE_COUNT][ADBMS_RUNTIME_SHARED_USED_CELLS_PER_AFE];
    bool valid;
} AdbmsRuntime_SharedSnapshot_t;

void adbms_runtime_main_on_core2(void);
bool AdbmsRuntime_SharedRead(AdbmsRuntime_SharedSnapshot_t *snapshot);
ECU8TR_ADBMS6830_State_t AdbmsRuntime_GetState(void);
uint16_t AdbmsRuntime_GetDetectedDevice(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ADBMS_ADBMS_RUNTIME_DETECT_H_ */
