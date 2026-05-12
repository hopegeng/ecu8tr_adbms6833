#ifndef LTC6812_SHARED_H
#define LTC6812_SHARED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LTC6812_SHARED_AFE_COUNT              (2u)
#define LTC6812_SHARED_USED_CELLS_PER_AFE     (10u)
#define LTC6812_SHARED_IC_STATUS_COUNT        (4u)
#define LTC6812_SHARED_FIRST_USED_CELL_0BASED (0u)
#define LTC6812_SHARED_EXTERNAL_TEMP_COUNT    (14u)

#ifndef LTC6812_SHARED_SAMPLE_PERIOD_MS
#define LTC6812_SHARED_SAMPLE_PERIOD_MS       (1000u)
#endif

#ifndef LTC6812_ENABLE_DEBUG_PRINTF
#define LTC6812_ENABLE_DEBUG_PRINTF           (0u)
#endif

#ifndef LTC6812_DEBUG_PRINT_PERIOD_MS
#define LTC6812_DEBUG_PRINT_PERIOD_MS         (1000u)
#endif

typedef struct
{
    uint32_t sample_counter;
    uint32_t sample_timestamp_ms;
    uint16_t cell_voltage_mV[LTC6812_SHARED_AFE_COUNT][LTC6812_SHARED_USED_CELLS_PER_AFE];
    uint16_t gpio_voltage_mV[LTC6812_SHARED_AFE_COUNT][LTC6812_SHARED_USED_CELLS_PER_AFE];
    int16_t cell_temp_raw_0p01C[LTC6812_SHARED_AFE_COUNT][LTC6812_SHARED_USED_CELLS_PER_AFE];
    uint16_t ic_cell_sum_raw_0p01V[LTC6812_SHARED_IC_STATUS_COUNT];
    int16_t ic_internal_temp_raw_0p01C[LTC6812_SHARED_IC_STATUS_COUNT];
    uint8_t ic_status_valid[LTC6812_SHARED_IC_STATUS_COUNT];
    int16_t external_temp_raw_0p01C[LTC6812_SHARED_EXTERNAL_TEMP_COUNT];
    uint16_t external_temp_voltage_mV[LTC6812_SHARED_EXTERNAL_TEMP_COUNT];
    uint16_t dew_sensor_mV;
    uint8_t dew_sensor_state;
    uint8_t led_on;
    uint8_t balancing[LTC6812_SHARED_AFE_COUNT][LTC6812_SHARED_USED_CELLS_PER_AFE];
    bool valid;
} Ltc6812_SharedSnapshot_t;

void Ltc6812_SharedInit(void);
void Ltc6812_SharedPublish(const Ltc6812_SharedSnapshot_t *snapshot);
bool Ltc6812_SharedRead(Ltc6812_SharedSnapshot_t *snapshot);
bool Ltc6812_SetLed(bool on);

#ifdef __cplusplus
}
#endif

#endif /* LTC6812_SHARED_H */
