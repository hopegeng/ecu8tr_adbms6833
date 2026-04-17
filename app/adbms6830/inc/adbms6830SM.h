/**
 * \file 	adbms6830SM.h
 * \brief 	ADBMS6830 register header file
 * \author 	R. Larocque
 *
 *
 */
#ifndef BMS_DRV_INC_ADBMS6830SM_H_
#define BMS_DRV_INC_ADBMS6830SM_H_

#include "stdint.h"
#include <stdbool.h>
//#include "bmsControl.h"

/******************************************************************************/
/*-------------------------Function Prototypes--------------------------------*/
/******************************************************************************/
extern bool adbms6830SM_CellMeasurements(void);
extern bool adbms6830SM_Freeze_Seq(void);
extern bool adbms6830SM_TrigAuxMeasurements(void);
extern bool adbms6830SM_TrigADSV(void);
extern bool adbms6830SM_Cell_OWD_Even(void);
extern bool adbms6830SM_Cell_OWD_Odd(void);
extern bool adbms6830SM_Read_Cell_OWD(void);
#endif /* BMS_DRV_INC_ADBMS6830SM_H_ */
