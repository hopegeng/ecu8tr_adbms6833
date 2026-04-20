/*
 * bmu_cell_scheduler.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "../bmu/bmu_cell_scheduler.h"

#include "../bmu/bmu_cell_can.h"
#include "bmu_cfg.h"

/*
	This spreads 360 messages evenly over 1 second using a 10 ms task.

	100 scheduler ticks per second
	need 360 messages per second
	average = 3.6 messages per 10 ms
	so alternate between 3 and 4
*/

static Bmu_CellSchedulerType g_bmuCellScheduler;

void Bmu_CellScheduler_Init(void)
{
    g_bmuCellScheduler.next_cell_0based = 0u;
    g_bmuCellScheduler.frac_accum = 0u;
}

static uint8_t Bmu_CellScheduler_CalcFramesPerTick(void)
{
    /* 3.6 messages per 10 ms tick -> use 3 or 4 with long-term average 3.6 */
    g_bmuCellScheduler.frac_accum += 6u;
    if (g_bmuCellScheduler.frac_accum >= 10u)
    {
        g_bmuCellScheduler.frac_accum -= 10u;
        return 4u;
    }

    return 3u;
}

void Bmu_CellScheduler_10msTask(void)
{
    uint8_t frames_to_send;
    uint8_t i;

    frames_to_send = Bmu_CellScheduler_CalcFramesPerTick();

    for (i = 0u; i < frames_to_send; i++)
    {
        (void)Bmu_CellCan_SendOne(g_bmuCellScheduler.next_cell_0based);

        g_bmuCellScheduler.next_cell_0based++;
        if (g_bmuCellScheduler.next_cell_0based >= BMU_TOTAL_CELLS)
        {
            g_bmuCellScheduler.next_cell_0based = 0u;
        }
    }
}

