/*
 * can_if.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_DBC_CAN_IF_H_
#define SRC_APP_DBC_CAN_IF_H_


#include <stdint.h>
#include <stdbool.h>
#include "bmu_types.h"

typedef struct
{
    uint32_t id;
    uint8_t  dlc;
    bool     is_extended;
    uint8_t  data[8];
} CanIf_MsgType;

void CanIf_Init(void);
Bmu_ReturnType CanIf_Transmit(const CanIf_MsgType *msg);


#endif /* SRC_APP_DBC_CAN_IF_H_ */
