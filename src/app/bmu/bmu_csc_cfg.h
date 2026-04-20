/*
 * bmu_csc_cfg.h
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_BMU_BMU_CSC_CFG_H_
#define SRC_APP_BMU_BMU_CSC_CFG_H_


#include <stdint.h>
#include <stdbool.h>
#include "bmu_cfg.h"

#define BMU_AFES_PER_CSC          (2u)
#define BMU_TOTAL_AFES            (BMU_CSC_COUNT * BMU_AFES_PER_CSC)

typedef struct
{
    bool used;
    uint8_t afe_on_csc;          /* 0 or 1 */
    uint8_t afe_cell_channel;    /* 0..15 */
    uint8_t afe_gpio_channel;    /* optional temp source */
    uint8_t balance_channel;     /* 0..15 */
} Bmu_CscCellMapType;

typedef struct
{
    Bmu_CscCellMapType cell_map[BMU_CELLS_PER_CSC];
} Bmu_CscMapType;

extern const Bmu_CscMapType g_bmuCscMap[BMU_CSC_COUNT];

uint8_t Bmu_Csc_GlobalAfeIndex(uint8_t csc_index, uint8_t afe_on_csc);


#endif /* SRC_APP_BMU_BMU_CSC_CFG_H_ */
