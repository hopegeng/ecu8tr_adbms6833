#ifndef SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6830_SHARED_H_
#define SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6830_SHARED_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS6830_SHARED_AFE_COUNT              (2u)
#define ADBMS6830_SHARED_USED_CELLS_PER_AFE     (10u)
#define ADBMS6830_SHARED_IC_STATUS_COUNT        (4u)
#define ADBMS6830_SHARED_FIRST_USED_CELL_0BASED (6u)

#ifndef ADBMS6830_SHARED_SAMPLE_PERIOD_MS
#define ADBMS6830_SHARED_SAMPLE_PERIOD_MS       (1000u)
#endif

#ifndef ADBMS6830_ENABLE_DEBUG_PRINTF
#define ADBMS6830_ENABLE_DEBUG_PRINTF           (0u)
#endif

#ifndef ADBMS6830_ENABLE_MAIN_DEBUG_PRINTF
#define ADBMS6830_ENABLE_MAIN_DEBUG_PRINTF      (0u)
#endif

#ifndef ADBMS6830_ENABLE_AUX_PRINTF
#define ADBMS6830_ENABLE_AUX_PRINTF             (1u)
#endif

typedef struct
{
    uint32_t sample_counter;
    uint32_t sample_timestamp_ms;
    uint16_t cell_voltage_mV[ADBMS6830_SHARED_AFE_COUNT][ADBMS6830_SHARED_USED_CELLS_PER_AFE];
    uint16_t gpio_voltage_mV[ADBMS6830_SHARED_AFE_COUNT][ADBMS6830_SHARED_USED_CELLS_PER_AFE];
    int16_t cell_temp_raw_0p01C[ADBMS6830_SHARED_AFE_COUNT][ADBMS6830_SHARED_USED_CELLS_PER_AFE];
    uint16_t ic_cell_sum_raw_0p01V[ADBMS6830_SHARED_IC_STATUS_COUNT];
    int16_t ic_internal_temp_raw_0p01C[ADBMS6830_SHARED_IC_STATUS_COUNT];
    uint8_t ic_status_valid[ADBMS6830_SHARED_IC_STATUS_COUNT];
    uint8_t balancing[ADBMS6830_SHARED_AFE_COUNT][ADBMS6830_SHARED_USED_CELLS_PER_AFE];
    bool valid;
} Adbms6830_SharedSnapshot_t;

void Adbms6830_SharedInit(void);
void Adbms6830_SharedPublish(const Adbms6830_SharedSnapshot_t *snapshot);
bool Adbms6830_SharedRead(Adbms6830_SharedSnapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6830_SHARED_H_ */
