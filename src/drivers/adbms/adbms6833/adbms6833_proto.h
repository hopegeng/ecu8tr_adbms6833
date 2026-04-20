/*
 * adbms6833_proto.h
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#ifndef SRC_DRIVERS_ADBMS_ADBMS6833_ADBMS6833_PROTO_H_
#define SRC_DRIVERS_ADBMS_ADBMS6833_ADBMS6833_PROTO_H_


#include <stdint.h>
#include <stdbool.h>
#include "bmu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS6833_MAX_AFES_PER_STACK      (36u)
#define ADBMS6833_CELL_CH_PER_AFE         (16u)
#define ADBMS6833_GPIO_CH_PER_AFE         (10u)
#define ADBMS6833_MAX_FRAME_BYTES         (512u)

/* Example command opcodes for skeleton only.
 * Replace with actual ADBMS6833 values later.
 */
#define ADBMS6833_CMD_ADCV                (0x0260u)
#define ADBMS6833_CMD_ADAX                (0x0460u)
#define ADBMS6833_CMD_RDCVA               (0x0400u)
#define ADBMS6833_CMD_RDAUXA              (0x0C00u)
#define ADBMS6833_CMD_RDBAL               (0x1200u)

typedef struct
{
    uint16_t cell_raw_0p1mV[ADBMS6833_CELL_CH_PER_AFE];
    uint16_t gpio_raw_0p1mV[ADBMS6833_GPIO_CH_PER_AFE];
    bool balancing[ADBMS6833_CELL_CH_PER_AFE];
    bool valid;
} Adbms6833_AfeSnapshotType;

typedef struct
{
    uint8_t afe_count;
    Adbms6833_AfeSnapshotType afe[ADBMS6833_MAX_AFES_PER_STACK];
} Adbms6833_StackSnapshotType;

void Adbms6833Proto_Init(void);

/* High-level operations */
Bmu_ReturnType Adbms6833_StartCellConversion(void);
Bmu_ReturnType Adbms6833_StartAuxConversion(void);
Bmu_ReturnType Adbms6833_ReadSnapshot(Adbms6833_StackSnapshotType *snapshot);

/* Lower-level helpers */
Bmu_ReturnType Adbms6833_SendCommand(uint16_t cmd);
Bmu_ReturnType Adbms6833_ReadRegisterGroup(uint16_t cmd,
                                           uint8_t total_afes,
                                           uint8_t bytes_per_afe,
                                           uint8_t *rx_buf,
                                           uint16_t rx_len);

/* Optional test/demo control */
void Adbms6833Proto_SetConfiguredAfeCount(uint8_t afe_count);

#ifdef __cplusplus
}
#endif


#endif /* SRC_DRIVERS_ADBMS_ADBMS6833_ADBMS6833_PROTO_H_ */
