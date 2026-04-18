/*
 * bmu_csc_acq.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "bmu_csc_acq.h"
#include "bmu_cell_db.h"
#include "bmu_cfg.h"

/* Round-robin acquisition example:
 * each 20 ms task handles one CSC board
 * 18 boards -> full sweep in 360 ms
 */

static uint8_t g_nextCscToAcquire = 0u;

void Bmu_CscAcq_Init(void)
{
    g_nextCscToAcquire = 0u;
}

/* Stub conversion helpers for demo only */
static uint16_t Demo_CellVoltageRaw(uint8_t csc, uint8_t cell)
{
    /* Example: around 3.700 V = 37000 * 0.1 mV */
    return (uint16_t)(37000u + (uint16_t)(csc * 10u) + cell);
}

static int16_t Demo_CellTempRaw(uint8_t csc, uint8_t cell)
{
    /* Example: 25.00 C = 2500 * 0.01 C */
    (void)cell;
    return (int16_t)(2500 + (int16_t)csc);
}

static uint16_t Demo_GpioVoltageRaw(uint8_t csc, uint8_t cell)
{
    (void)cell;
    return (uint16_t)(50000u + csc); /* 5000.0 mV if used that way in DBC */
}

static bool Demo_Balancing(uint8_t csc, uint8_t cell)
{
    return (((csc + cell) & 0x01u) != 0u) ? true : false;
}

void Bmu_CscAcq_MainTask_20ms(uint32_t now_ms)
{
    uint8_t cell;

    /* TODO:
     * 1. wake/check ADBMS chain
     * 2. trigger snapshot/read-all as needed
     * 3. read voltages, temperatures, GPIO, balancing status for g_nextCscToAcquire
     * 4. update DB
     */

    for (cell = 0u; cell < BMU_CELLS_PER_CSC; cell++)
    {
        (void)Bmu_CellDb_UpdateMeasurement(g_nextCscToAcquire,
                                           cell,
                                           Demo_CellVoltageRaw(g_nextCscToAcquire, cell),
                                           Demo_CellTempRaw(g_nextCscToAcquire, cell),
                                           Demo_GpioVoltageRaw(g_nextCscToAcquire, cell),
                                           Demo_Balancing(g_nextCscToAcquire, cell),
                                           now_ms);
    }

    g_nextCscToAcquire++;
    if (g_nextCscToAcquire >= BMU_CSC_COUNT)
    {
        g_nextCscToAcquire = 0u;
    }
}

