#ifndef BMU_CFG_H
#define BMU_CFG_H

#include <stdint.h>

/* BMU topology for the current ADBMS6830 platform:
 * one BMU controls one battery module
 * one module contains two ADBMS6830 AFEs
 * each AFE reports only cells 7..16, so 10 used cells per AFE
 * total measured cell count is 20
 */

/* System topology */

#define BMU_MODULE_COUNT				(1u)
#define BMU_CSC_COUNT					(1u)
#define BMU_AFE_COUNT_PER_CSC			(2u)
#define BMU_AFE_COUNT					((BMU_CSC_COUNT) * (BMU_AFE_COUNT_PER_CSC) )
#define BMU_CELL_COUNT_PER_AFE			(10u)
#define BMU_TOTAL_CELL_COUNT			((BMU_AFE_COUNT) * (BMU_CELL_COUNT_PER_AFE))
#define BMU_NTC_COUNT_PER_AFE			(5u)

#define BMU_CELLS_PER_CSC               ((BMU_AFE_COUNT_PER_CSC) * (BMU_CELL_COUNT_PER_AFE))
#define BMU_TOTAL_CELLS                 (BMU_TOTAL_CELL_COUNT)

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
#if (BMU_TOTAL_CELLS != 20u)
# error "This BMU build expects 1 CSC x 2 AFEs x 10 used cells = 20 total cells."
#endif


typedef struct
{

	uint16_t cellRaw[BMU_AFE_COUNT][BMU_CELL_COUNT_PER_AFE];
    uint16_t cell_mV[BMU_AFE_COUNT][BMU_CELL_COUNT_PER_AFE];

    uint8_t  cfgA[BMU_AFE_COUNT][6];
    uint8_t  cfgB[BMU_AFE_COUNT][6];

    uint8_t  serialID[BMU_AFE_COUNT][6];
    uint8_t  pecErrorCount;

} BmuContext_t;


#endif /* BMU_CFG_H */
