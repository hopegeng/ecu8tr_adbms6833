/**
 ******************************************************************************
 * @file    dout.h
 * @author	JW
 * @version V0.1.0
 * @date    Sep2020
 * @brief	Supports Global digital OUTPUT macros.
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


#ifndef INC_DOUT_H_
#define INC_DOUT_H_


/*************************** Source File Constants ****************************/
#define DOUT_LOGIC_HI	(1)
#define DOUT_LOGIC_LO	(0)

#define BMS_DOUT_TOGGLE_MASK				(0x00010001u)
#define BMS_DOUT_TOGGLE_PIN(x)				(BMS_DOUT_TOGGLE_MASK << x)

#define BMS_DOUT_PIN_HS_UART_TX				(10u)		/* P14.10 */
#define BMS_DOUT_PIN_LS_UART_TX				(7u)		/* P14.7 */

/* PORT0 */
#define BMS_DOUT_CONTACTOR_IN(x) 	MODULE_P00.OUT.B.P6 = (x ? 1:0)
#define BMS_DOUT_CONTACTOR_DEN(x)	MODULE_P00.OUT.B.P7 = (x ? 1:0)
#define BMS_DOUT_PRECHARGE_IN(x)	MODULE_P00.OUT.B.P9 = (x ? 1:0)

/* PORT2 */
#define BMS_DOUT_CHG_OFF_PKG1(x) 	MODULE_P02.OUT.B.P0 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG2(x) 	MODULE_P02.OUT.B.P1 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG3(x) 	MODULE_P02.OUT.B.P2 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG4(x) 	MODULE_P02.OUT.B.P3 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG5(x) 	MODULE_P02.OUT.B.P4 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG6(x) 	MODULE_P02.OUT.B.P5 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG7(x) 	MODULE_P02.OUT.B.P6 = (x ? 1:0)
#define BMS_DOUT_CHG_OFF_PKG8(x) 	MODULE_P02.OUT.B.P7 = (x ? 1:0)

/* PORT13 */
#define BMS_ETHPYH_RESET(x)			MODULE_P13.OUT.B.P2 = (x ? 1:0)

/* PORT14 */
#define BMS_DOUT_HS_UART_TX(x)		MODULE_P14.OUT.B.P10 = (x ? 1:0)
#define BMS_TOGGLE_HS_DATA_TX()		MODULE_P14.OMR.U |= BMS_DOUT_TOGGLE_PIN(BMS_DOUT_PIN_HS_UART_TX)
#define BMS_DOUT_LS_UART_TX(x)		MODULE_P14.OUT.B.P7 = (x ? 1:0)
#define BMS_TOGGLE_LS_DATA_TX()		MODULE_P14.OMR.U |= BMS_DOUT_TOGGLE_PIN(BMS_DOUT_PIN_LS_UART_TX)

/* PORT20 */
#define BMS_DOUT_ERRQ_EXT(x) 		MODULE_P20.OUT.B.P3 = (x ? 1:0)
#define BMS_DOUT_ERRQ_RES(x) 		MODULE_P20.OUT.B.P6 = (x ? 1:0)
#define BMS_DOUT_NSLEEP(x) 			MODULE_P20.OUT.B.P10 = (x ? 1:0)

#define ISOSPI_DOUT_EN(x)			MODULE_P13.OUT.B.P3 = (x ? 1:0)

/* PORT 15 */
#define QSPI2_DOUT_SLSO0(x)			MODULE_P15.OUT.B.P2 = (x ? 1:0)

/***************************** Source File Types ******************************/


/****************************** Application APIs ******************************/


#endif /* INC_DOUT_H_ */

