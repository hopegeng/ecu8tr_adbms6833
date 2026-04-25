#ifndef SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6830_SHARED_H_
#define SRC_DRIVERS_ADBMS_ADBMS_ON_CORE2_ADBMS6830_SHARED_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS6830_SHARED_AFE_COUNT              (2u)
#define ADBMS6830_SHARED_USED_CELLS_PER_AFE     (10u)
#define ADBMS6830_SHARED_FIRST_USED_CELL_0BASED (6u)

typedef struct
{
    uint32_t sample_counter;
    uint32_t sample_timestamp_ms;
    uint16_t cell_voltage_mV[ADBMS6830_SHARED_AFE_COUNT][ADBMS6830_SHARED_USED_CELLS_PER_AFE];
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
