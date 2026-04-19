/*
 * bmu_cell_db.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_DBC_BMU_CELL_DB_H_
#define SRC_APP_DBC_BMU_CELL_DB_H_

#ifndef BMU_CELL_DB_H
#define BMU_CELL_DB_H

#include <stdint.h>
#include <stdbool.h>
#include "bmu_cfg.h"
#include "bmu_types.h"
#include "can_if.h"
#include "dbc_cell_message.h"

typedef struct
{
    Dbc_CellMessageSignalsType dbc_cell_sig;

    bool     valid;
    bool     stale;
    uint32_t sample_timestamp_ms;

    uint8_t  csc_index;                /* 0..17 */
    uint8_t  cell_on_csc;              /* 0..19 */
} Bmu_CellRecordType;

typedef struct
{
    Bmu_CellRecordType cells[BMU_TOTAL_CELLS];
} Bmu_CellDbType;

extern Bmu_CellDbType g_bmuCellDb;

void Bmu_CellDb_Init(void);

Bmu_ReturnType Bmu_CellDb_UpdateMeasurement(uint8_t csc_index,
                                            uint8_t cell_on_csc,
                                            uint16_t cell_voltage_raw_0p1mV,
                                            int16_t cell_temp_raw_0p01C,
                                            uint16_t gpio_voltage_raw_0p1mV,
                                            bool balancing,
                                            uint32_t now_ms);

const Bmu_CellRecordType *Bmu_CellDb_GetByGlobal0(uint16_t global_cell_0based);

void Bmu_CellDb_StaleMonitor(uint32_t now_ms);

#endif /* BMU_CELL_DB_H */

#endif /* SRC_APP_DBC_BMU_CELL_DB_H_ */
