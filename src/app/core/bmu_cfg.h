#ifndef BMU_CFG_H
#define BMU_CFG_H

#include <stdint.h>

/* System topology */
#define BMU_CSC_COUNT                   (18u)
#define BMU_CELLS_PER_CSC               (20u)
#define BMU_TOTAL_CELLS                 (BMU_CSC_COUNT * BMU_CELLS_PER_CSC)

/* DBC timing */

#define BMU_CELLMESSAGE_CYCLE_MS        (1000u)

/* Scheduler tick used by CellMessage publisher */
#define BMU_CELL_TX_TASK_PERIOD_MS      (10u)

/* Stale policy */
#define BMU_CELL_STALE_TIMEOUT_MS       (1500u)

/* DBC invalid fallback values if measurement is stale/invalid */
#define BMU_INVALID_CELL_VOLTAGE_RAW    (0u)     /* 0.1 mV/bit */
#define BMU_INVALID_CELL_TEMP_RAW       (0)      /* 0.01 C/bit */
#define BMU_INVALID_GPIO_VOLTAGE_RAW    (0u)     /* 0.1 mV/bit */
#define BMU_INVALID_BALANCING           (0u)

/* Compile-time checks */
#if (BMU_TOTAL_CELLS != 360u)
# error "This framework currently expects 18 CSC x 20 cells = 360 total cells."
#endif

#endif /* BMU_CFG_H */
