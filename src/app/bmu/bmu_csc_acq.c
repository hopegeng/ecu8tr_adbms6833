/*
 * bmu_csc_acq.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "../bmu/bmu_csc_acq.h"

#include "../bmu/bmu_cell_db.h"
#include "adbms_service.h"
#include "bmu_cfg.h"


//when the hardware CSC boards are not available, we use the demo version
//#define __DEMO__	1
/* Round-robin acquisition example:
 * each 20 ms task handles one CSC board
 * 18 boards -> full sweep in 360 ms
 */

static uint8_t g_nextCscToAcquire = 0u;

#if __DEMO__

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
static int16_t Bmu_ConvertTempRawFromGpio(uint16_t gpio_raw)
{
    (void)gpio_raw;
    return 2500;
}

void Bmu_CscAcq_Init(void)
{
    (void)Adbms_Service_Init();
}

void Bmu_CscAcq_MainTask_20ms(uint32_t now_ms)
{
    static uint8_t next_csc = 0u;
    Adbms_ServiceCscSnapshotType snap;
    uint8_t cell;

    if (Adbms_Service_ReadCscSnapshot(next_csc, &snap) == BMU_OK)
    {
        if (snap.valid)
        {
            for (cell = 0u; cell < BMU_CELLS_PER_CSC; cell++)
            {
                if (snap.cells[cell].valid)
                {
                    (void)Bmu_CellDb_UpdateMeasurement(next_csc,
                                                       cell,
                                                       snap.cells[cell].cell_voltage_raw_0p1mV,
                                                       Bmu_ConvertTempRawFromGpio(
                                                           snap.cells[cell].gpio_voltage_raw_0p1mV),
                                                       snap.cells[cell].gpio_voltage_raw_0p1mV,
                                                       snap.cells[cell].balancing,
                                                       now_ms);
                }
            }
        }
    }

    next_csc++;
    if (next_csc >= BMU_CSC_COUNT)
    {
        next_csc = 0u;
    }
}
#endif
