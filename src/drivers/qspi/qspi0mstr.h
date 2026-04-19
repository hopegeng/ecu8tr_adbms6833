/**
 ******************************************************************************
 * @file    qspi0mstr.h
 * @author	JW
 * @version V0.1.0
 * @date    Jun2018
 * @brief
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


#ifndef QSPI_INC_QSPI0MASTER_H_
#define QSPI_INC_QSPI0MASTER_H_


/* IFX includes */
#include "_Impl/IfxSrc_cfg.h"
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "_Reg/IfxSrc_reg.h"
#include "_Reg/IfxPort_reg.h"

#include "qspi.h"
//#include "core0_isr.h"


/******************************** Configuration *******************************/


/*************************** Source File Constants ****************************/
#define QSPI0_CPU_VEC_TBL			(0u)
#define QSPI0_RxD_ISR_PRIORITY		(0x38u)
#define QSPI0_TxD_ISR_PRIORITY		(0x37u)
#define QSPI0_ERR_ISR_PRIORITY		(0x39u)
#define QSPI0_CPU_TOS				IfxSrc_Tos_cpu0

#define kuQSPI0_RX_EVENT			(0x00000001u)
#define kuQSPI0_ERR_EVENT			(0x00000002u)


/***************************** Source File Types ******************************/
#ifdef __cplusplus
extern "C" {
#endif

extern int32_t qspi0mstr_Send(QspiCs_t CS, uint16_t u16Length, uint8_t* pu8Buff, uint8_t* pu8DstBuff);
extern int32_t qspi0mstr_Enable(boolean bState);
extern boolean qspi0mstr_waitForRxDone( void );

#ifdef __cplusplus
}
#endif


#endif /* QSPI_INC_QSPIMASTER_H_ */

