/*
 * bmu_cell_mapping.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#include "../bmu/bmu_cell_mapping.h"

#include "can_if.h"
#include "dbc_cell_message.h"

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

bool Bmu_IsValidAfeIndex(uint8_t afe_index)
{
    return (afe_index < BMU_AFE_COUNT);
}

bool Bmu_IsValidCellOnAfe(uint8_t cell_on_afe)
{
    return (cell_on_afe < BMU_CELL_COUNT_PER_AFE);
}

bool Bmu_IsValidGlobalCell0(uint16_t global_cell_0based)
{
    return (global_cell_0based < BMU_TOTAL_CELLS);
}

uint16_t Bmu_MakeGlobalCellIndex0(uint8_t afe_index, uint8_t cell_on_afe)
{
    return (uint16_t)(((uint16_t)afe_index * BMU_CELL_COUNT_PER_AFE) + cell_on_afe);
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
    return (DBC_CELLMESSAGE_FIRST_ID + (uint32_t)(global_cell_1based - 1u));
}

uint32_t Bmu_CellMessageIdFromGlobal0(uint16_t global_cell_0based)
{
    return (DBC_CELLMESSAGE_FIRST_ID + (uint32_t)global_cell_0based);
}
