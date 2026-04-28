#ifndef SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6833_SHARED_H_
#define SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6833_SHARED_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS6833_SHARED_AFE_COUNT              (2u)
#define ADBMS6833_SHARED_USED_CELLS_PER_AFE     (10u)
#define ADBMS6833_SHARED_FIRST_USED_CELL_0BASED (6u)

#ifndef ADBMS6833_SHARED_SAMPLE_PERIOD_MS
#define ADBMS6833_SHARED_SAMPLE_PERIOD_MS       (5000u)
#endif

#ifndef ADBMS6833_ENABLE_DEBUG_PRINTF
#define ADBMS6833_ENABLE_DEBUG_PRINTF           (0u)
#endif

#ifndef ADBMS6833_ENABLE_MAIN_DEBUG_PRINTF
#define ADBMS6833_ENABLE_MAIN_DEBUG_PRINTF      (0u)
#endif

#ifndef ADBMS6833_ENABLE_AUX_PRINTF
#define ADBMS6833_ENABLE_AUX_PRINTF             (1u)
#endif

typedef struct
{
    uint32_t sample_counter;
    uint32_t sample_timestamp_ms;
    uint16_t cell_voltage_mV[ADBMS6833_SHARED_AFE_COUNT][ADBMS6833_SHARED_USED_CELLS_PER_AFE];
    int16_t cell_temp_raw_0p01C[ADBMS6833_SHARED_AFE_COUNT][ADBMS6833_SHARED_USED_CELLS_PER_AFE];
    uint8_t balancing[ADBMS6833_SHARED_AFE_COUNT][ADBMS6833_SHARED_USED_CELLS_PER_AFE];
    bool valid;
} Adbms6833_SharedSnapshot_t;

void Adbms6833_SharedInit(void);
void Adbms6833_SharedPublish(const Adbms6833_SharedSnapshot_t *snapshot);
bool Adbms6833_SharedRead(Adbms6833_SharedSnapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6833_SHARED_H_ */
