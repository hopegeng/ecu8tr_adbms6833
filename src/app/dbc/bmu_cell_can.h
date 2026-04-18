/*
 * bmu_cell_can.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_DBC_BMU_CELL_CAN_H_
#define SRC_APP_DBC_BMU_CELL_CAN_H_



#include <stdint.h>
#include "bmu_types.h"

Bmu_ReturnType Bmu_CellCan_BuildMessage(uint16_t global_cell_0based, void *msg_out);
Bmu_ReturnType Bmu_CellCan_SendOne(uint16_t global_cell_0based);


#endif /* SRC_APP_DBC_BMU_CELL_CAN_H_ */
