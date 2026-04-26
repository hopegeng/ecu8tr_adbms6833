/*
 * bmu_cell_mapping.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_BMU_BMU_CELL_MAPPING_H_
#define SRC_APP_BMU_BMU_CELL_MAPPING_H_

#ifndef BMU_CELL_MAPPING_H
#define BMU_CELL_MAPPING_H

#include <stdint.h>
#include <stdbool.h>
#include "bmu_types.h"
#include "bmu_cfg.h"

typedef struct
{
    uint8_t csc_index;      /* 0..17 */
    uint8_t cell_on_csc;    /* 0..19 */
} Bmu_CellPhysicalMapType;

void Bmu_CellMapping_InitDefault(void);

bool Bmu_IsValidAfeIndex(uint8_t csc_index);
bool Bmu_IsValidCellOnAfe(uint8_t cell_on_csc);
bool Bmu_IsValidGlobalCell0(uint16_t global_cell_0based);

uint16_t Bmu_MakeGlobalCellIndex0(uint8_t afe_index, uint8_t cell_on_afe);
uint8_t  Bmu_GlobalToCscIndex(uint16_t global_cell_0based);
uint8_t  Bmu_GlobalToCellOnCsc(uint16_t global_cell_0based);

uint32_t Bmu_CellMessageIdFromGlobal1(uint16_t global_cell_1based);
uint32_t Bmu_CellMessageIdFromGlobal0(uint16_t global_cell_0based);

#endif /* BMU_CELL_MAPPING_H */

#endif /* SRC_APP_BMU_BMU_CELL_MAPPING_H_ */
