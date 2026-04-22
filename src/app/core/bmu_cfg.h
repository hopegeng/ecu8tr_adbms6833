#ifndef BMU_CFG_H
#define BMU_CFG_H

#include <stdint.h>

/* The BMS system structure:
 * A BMU: TC399 + ADBMS6822 -> isoSPI
 * isoSPI -> CSC( ADBMS6833 ) -> isoSPI -> CSC( ADBMS6833 ) -> ...
 *
 * */

/* Borg Warner BMS system
 * 9 modules, each module 600 cells
 * One ECU8TR controls one module
 * One module: 600 cells
 * 		one CSC: 2 AFEs( 2 ADBMS6833 ) to control 20 cells
 * 			one AFE controls 10 cells, the cell 1 to cell 6 are not used. Cell 7 to Cell 16 are connected to the real battery cell
 * total 30 CSC boards, 60 AFEs(ADBMS6833)
 *
 * From the ADBMS6822
 */

/* System topology */

#define BMU_MODULE_COUNT				(1u)
#define BMU_CSC_COUNT					(30u)
#define BMU_AFE_COUNT_PER_CSC			(2u)
#define BMU_AFE_COUNT					((BMU_CSC_COUNT) * (BMU_AFE_COUNT_PER_CSC) )
#define BMU_CELL_COUNT_PER_AFE			(10u)
#define BMU_TOTAL_CELL_COUNT			((BMU_AFE_COUNT) * (BMU_CELL_COUNT_PER_AFE))		//600
#define BMU_NTC_COUNT_PER_AFE			(5u)

#define BMU_CELLS_PER_CSC               ((BMU_AFE_COUNT_PER_CSC) * (BMU_CELL_COUNT_PER_AFE) )	//20
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
#if (BMU_TOTAL_CELLS != 600u)
# error "This framework currently expects 30 CSC x 20 cells = 600 total cells."
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
