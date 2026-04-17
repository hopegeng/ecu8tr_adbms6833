/**
 ******************************************************************************
 * @file    dout.h
 * @author	JW
 * @version V0.1.0
 * @date    Sep2020
 * @brief	Supports Global digital INPUT macros.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2020 Neutron Controls.</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS for
 * the sole purpose of a reference design, WITHOUT WARRANTIES OR CONDITIONS OF
 * ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
 *
 ******************************************************************************
 */


/* C includes */
#include <stdio.h>
/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Port/Io/IfxPort_Io.h"
#include "_PinMap/IfxPort_PinMap.h"
/* TASKING PINMAPPER includes */
#include "aurix_pin_mappings.h"
/* Application includes */

#ifndef INC_DIN_H_
#define INC_DIN_H_


/*************************** Source File Constants ****************************/
/* PORT20 */
#define BMS_DIN_CONTACTOR_AUX() MODULE_P00.IN.B.P3
/* These 3 are used as validation in state machine of outputs */
#define BMS_DIN_CONTACTOR_IN() 	MODULE_P00.IN.B.P6
#define BMS_DIN_CONTACTOR_DEN()	MODULE_P00.IN.B.P7
#define BMS_DIN_PRECHARGE_IN()	MODULE_P00.IN.B.P9

/* PORT20 */
#define BMS_DIN_ERRQ_EXT() MODULE_P20.IN.B.P3
#define BMS_DIN_ERRQ_RES() MODULE_P20.IN.B.P6
#define BMS_DIN_ERRQ() MODULE_P20.IN.B.P9
#define BMS_DIN_NSLEEP() MODULE_P20.IN.B.P10
#define BMS_DIN_ERRQ_LOC_OUT() MODULE_P20.IN.B.P11
#define BMS_DIN_ERR_EXT_OUT() MODULE_P20.IN.B.P12


/***************************** Source File Types ******************************/


/****************************** Application APIs ******************************/


#endif /* INC_DIN_H_ */

