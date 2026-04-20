/*
 * bmu_cell_scheduler.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_BMU_BMU_CELL_SCHEDULER_H_
#define SRC_APP_BMU_BMU_CELL_SCHEDULER_H_


#include <stdint.h>

typedef struct
{
    uint16_t next_cell_0based;
    uint16_t frac_accum;
} Bmu_CellSchedulerType;

void Bmu_CellScheduler_Init(void);
void Bmu_CellScheduler_10msTask(void);


#endif /* SRC_APP_BMU_BMU_CELL_SCHEDULER_H_ */
