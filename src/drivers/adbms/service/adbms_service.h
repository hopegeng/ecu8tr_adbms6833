/*
 * adbms_service.h
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#ifndef SRC_DRIVERS_ADBMS_SERVICE_ADBMS_SERVICE_H_
#define SRC_DRIVERS_ADBMS_SERVICE_ADBMS_SERVICE_H_


#include <stdint.h>
#include <stdbool.h>
#include "bmu_types.h"
#include "bmu_cfg.h"

#define ADBMS_SERVICE_CSC_COUNT        (18u)
#define ADBMS_SERVICE_CELLS_PER_CSC    (20u)

typedef struct
{
    uint16_t cell_voltage_raw_0p1mV;
    uint16_t gpio_voltage_raw_0p1mV;
    bool balancing;
    bool valid;
} Adbms_ServiceCellType;

typedef struct
{
    uint8_t csc_index;
    Adbms_ServiceCellType cells[ADBMS_SERVICE_CELLS_PER_CSC];
    bool valid;
} Adbms_ServiceCscSnapshotType;

Bmu_ReturnType Adbms_Service_Init(void);
Bmu_ReturnType Adbms_Service_Wake(void);
Bmu_ReturnType Adbms_Service_ReadCscSnapshot(uint8_t csc_index,
                                             Adbms_ServiceCscSnapshotType *snapshot);

#endif /* SRC_DRIVERS_ADBMS_SERVICE_ADBMS_SERVICE_H_ */
