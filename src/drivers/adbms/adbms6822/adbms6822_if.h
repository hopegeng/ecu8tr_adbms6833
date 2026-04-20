/*
 * adbms6822_if.h
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#ifndef SRC_DRIVERS_ADBMS_ADBMS6822_ADBMS6822_IF_H_
#define SRC_DRIVERS_ADBMS_ADBMS6822_ADBMS6822_IF_H_



#include <stdint.h>
#include <stdbool.h>
#include "bmu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool initialized;
    bool link_awake;
    bool lpcm_enabled;
    uint32_t last_activity_ms;
    uint32_t tx_count;
    uint32_t rx_count;
} Adbms6822_StateType;

void Adbms6822_Init(void);
const Adbms6822_StateType *Adbms6822_GetState(void);

bool Adbms6822_IsAwake(void);
Bmu_ReturnType Adbms6822_WakeChain(void);
Bmu_ReturnType Adbms6822_SendRawFrame(const uint8_t *tx,
                                      uint8_t *rx,
                                      uint16_t len);

#ifdef __cplusplus
}
#endif


#endif /* SRC_DRIVERS_ADBMS_ADBMS6822_ADBMS6822_IF_H_ */
