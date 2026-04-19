/*
 * qspimstr.h
 *
 *  Created on: Mar 18, 2022
 *      Author: rlarocque
 */

#ifndef BSP_QSPI_INC_QSPIMSTR_H_
#define BSP_QSPI_INC_QSPIMSTR_H_

#include <stdint.h>
/* IFX includes */
#include "_Impl/IfxSrc_cfg.h"
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "_Reg/IfxSrc_reg.h"
#include "_Reg/IfxPort_reg.h"

#include "qspi.h"
//#include "core0_isr.h"
#include "stdbool.h"
extern int32_t qspimstr_Send(QspiCs_t CS, uint16_t u16Length, uint8_t* pu8SrcBuff,uint8_t* pu8DstBuff);
//extern bool qspimstr_waitForRxDone( void );

#endif /* BSP_QSPI_INC_QSPIMSTR_H_ */
