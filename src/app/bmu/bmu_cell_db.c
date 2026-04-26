/*
 * bmu_cell_db.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "../bmu/bmu_cell_db.h"

#include "../bmu/bmu_cell_mapping.h"

Bmu_CellDbType g_bmuCellDb;

void Bmu_CellDb_Init(void)
{
    uint16_t i;

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        g_bmuCellDb.cells[i].dbc_cell_sig.cell_voltage_raw_0p1mV = 0u;
        g_bmuCellDb.cells[i].dbc_cell_sig.cell_temp_raw_0p01C = 0;
        g_bmuCellDb.cells[i].dbc_cell_sig.gpio_voltage_raw_0p1mV = 0u;
        g_bmuCellDb.cells[i].dbc_cell_sig.balancing = false;
        g_bmuCellDb.cells[i].valid = false;
        g_bmuCellDb.cells[i].stale = true;
        g_bmuCellDb.cells[i].sample_timestamp_ms = 0u;
        g_bmuCellDb.cells[i].csc_index = Bmu_GlobalToCscIndex(i);
        g_bmuCellDb.cells[i].cell_on_csc = Bmu_GlobalToCellOnCsc(i);
    }
}

Bmu_ReturnType Bmu_CellDb_UpdateMeasurement(uint8_t afe_index,
                                            uint8_t cell_on_afe,
                                            uint16_t cell_voltage_raw_0p1mV,
                                            int16_t cell_temp_raw_0p01C,
                                            uint16_t gpio_voltage_raw_0p1mV,
                                            bool balancing,
                                            uint32_t now_ms)
{
    uint16_t idx;

    if ((!Bmu_IsValidAfeIndex(afe_index)) || (!Bmu_IsValidCellOnAfe(cell_on_afe)))
    {
        return BMU_E_RANGE;
    }

    idx = Bmu_MakeGlobalCellIndex0(afe_index, cell_on_afe);

    g_bmuCellDb.cells[idx].dbc_cell_sig.cell_voltage_raw_0p1mV = cell_voltage_raw_0p1mV;
    g_bmuCellDb.cells[idx].dbc_cell_sig.cell_temp_raw_0p01C = cell_temp_raw_0p01C;
    g_bmuCellDb.cells[idx].dbc_cell_sig.gpio_voltage_raw_0p1mV = gpio_voltage_raw_0p1mV;
    g_bmuCellDb.cells[idx].dbc_cell_sig.balancing = balancing;
    g_bmuCellDb.cells[idx].valid = true;
    g_bmuCellDb.cells[idx].stale = false;
    g_bmuCellDb.cells[idx].sample_timestamp_ms = now_ms;

    return BMU_OK;
}

const Bmu_CellRecordType *Bmu_CellDb_GetByGlobal0(uint16_t global_cell_0based)
{
    if (!Bmu_IsValidGlobalCell0(global_cell_0based))
    {
        return 0;
    }

    return &g_bmuCellDb.cells[global_cell_0based];
}

void Bmu_CellDb_StaleMonitor(uint32_t now_ms)
{
    uint16_t i;
    uint32_t age;

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        if (g_bmuCellDb.cells[i].valid == false)
        {
            g_bmuCellDb.cells[i].stale = true;
        }
        else
        {
            age = now_ms - g_bmuCellDb.cells[i].sample_timestamp_ms;
            g_bmuCellDb.cells[i].stale = (age > BMU_CELL_STALE_TIMEOUT_MS) ? true : false;
        }
    }
}
