/*
 * bmu_csc_acq.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "../bmu/bmu_csc_acq.h"

#include "../bmu/bmu_cell_db.h"
#include "../../drivers/adbms/adbms_on_core2/adbms6830_shared.h"
#include "bmu_cfg.h"


//when the hardware CSC boards are not available, we use the demo version
//#define __DEMO__	1
/* Round-robin acquisition example:
 * each 20 ms task handles one CSC board
 * 18 boards -> full sweep in 360 ms
 */

#if __DEMO__
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

#else
static uint32_t g_lastSampleCounter = 0u;

void Bmu_CscAcq_Init(void)
{
    g_lastSampleCounter = 0u;
}

void Bmu_CscAcq_MainTask_20ms(uint32_t now_ms)
{
    Adbms6830_SharedSnapshot_t snapshot;
    uint8_t afeIdx;
    uint8_t cellIdx;

    if (Adbms6830_SharedRead(&snapshot) == false)
    {
        return;
    }

    if ((snapshot.valid == false) || (snapshot.sample_counter == 0u))
    {
        return;
    }

    if (snapshot.sample_counter == g_lastSampleCounter)
    {
        return;
    }

    for (afeIdx = 0u; afeIdx < BMU_AFE_COUNT_PER_CSC; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < BMU_CELL_COUNT_PER_AFE; cellIdx++)
        {
            (void)Bmu_CellDb_UpdateMeasurement(0u,
                                               (uint8_t)((afeIdx * BMU_CELL_COUNT_PER_AFE) + cellIdx),
                                               (uint16_t)(snapshot.cell_voltage_mV[afeIdx][cellIdx] * 10u),
                                               (int16_t)BMU_INVALID_CELL_TEMP_RAW,
                                               BMU_INVALID_GPIO_VOLTAGE_RAW,
                                               (snapshot.balancing[afeIdx][cellIdx] != 0u),
                                               now_ms);
        }
    }

    g_lastSampleCounter = snapshot.sample_counter;
}
#endif
