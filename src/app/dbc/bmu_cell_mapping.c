/*
 * bmu_cell_mapping.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "bmu_cell_mapping.h"

void Bmu_CellMapping_InitDefault(void)
{
    /* Default mapping is implicit:
     * CSC1 -> C001..C020
     * CSC2 -> C021..C040
     * ...
     * CSC18 -> C341..C360
     *
     * If later your wiring order differs, replace the implicit mapping
     * with a lookup table here.
     */
}

bool Bmu_IsValidCscIndex(uint8_t csc_index)
{
    return (csc_index < BMU_CSC_COUNT);
}

bool Bmu_IsValidCellOnCsc(uint8_t cell_on_csc)
{
    return (cell_on_csc < BMU_CELLS_PER_CSC);
}

bool Bmu_IsValidGlobalCell0(uint16_t global_cell_0based)
{
    return (global_cell_0based < BMU_TOTAL_CELLS);
}

uint16_t Bmu_MakeGlobalCellIndex0(uint8_t csc_index, uint8_t cell_on_csc)
{
    return (uint16_t)(((uint16_t)csc_index * BMU_CELLS_PER_CSC) + cell_on_csc);
}

uint8_t Bmu_GlobalToCscIndex(uint16_t global_cell_0based)
{
    return (uint8_t)(global_cell_0based / BMU_CELLS_PER_CSC);
}

uint8_t Bmu_GlobalToCellOnCsc(uint16_t global_cell_0based)
{
    return (uint8_t)(global_cell_0based % BMU_CELLS_PER_CSC);
}

uint32_t Bmu_CellMessageIdFromGlobal1(uint16_t global_cell_1based)
{
    return (BMU_CANID_C001_CELLMESSAGE + (uint32_t)(global_cell_1based - 1u));
}

uint32_t Bmu_CellMessageIdFromGlobal0(uint16_t global_cell_0based)
{
    return (BMU_CANID_C001_CELLMESSAGE + (uint32_t)global_cell_0based);
}
