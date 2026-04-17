/**
 ******************************************************************************
 * @file    qspi3mstr.h
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


#ifndef QSPI_INC_QSPI3MASTER_H_
#define QSPI_INC_QSPI3MASTER_H_


/* IFX includes */
#include "_Impl/IfxSrc_cfg.h"
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "_Reg/IfxSrc_reg.h"
#include "_Reg/IfxPort_reg.h"

#include "qspi.h"
#include "core2_isr.h"


/******************************** Configuration *******************************/


/*************************** Source File Constants ****************************/
#define QSPI3_CPU_VEC_TBL			kuCPU2_VEC_TBL
#define QSPI3_RxD_ISR_PRIORITY		kuCPU2_ISR_PRIORITY_QSPI3_RX
#define QSPI3_TxD_ISR_PRIORITY		kuCPU2_ISR_PRIORITY_QSPI3_TX
#define QSPI3_ERR_ISR_PRIORITY		kuCPU2_ISR_PRIORITY_QSPI3_ER
#define QSPI3_CPU_TOS				IfxSrc_Tos_cpu2

#define kuQSPI3_RX_EVENT			(0x00000001u)
#define kuQSPI3_ERR_EVENT			(0x00000002u)

/* CS for TLF35584 is SLSO10 */
#define TLF35584_SLSO           								10
/*
 * Definitions copied from TLF35584Demo.c of
 * AP32342_Testing_and_servicing_the_TLF35584_v3_0.pdf
 */
#define QSPI3_WRITE(cmd, data)  ((((cmd) | 0x40) << 8) | ((data) & 0xFF))
#define QSPI3_READ(cmd)   ((cmd)<<8)

#define TLF35584_UNLOCK1 QSPI3_WRITE (PROTCFG, 0xAB)  // unlock sequence 1
#define TLF35584_UNLOCK2 QSPI3_WRITE (PROTCFG, 0xEF)  // unlock sequence 2
#define TLF35584_UNLOCK3 QSPI3_WRITE (PROTCFG, 0x56)  // unlock sequence 3
#define TLF35584_UNLOCK4 QSPI3_WRITE (PROTCFG, 0x12)  // unlock sequence 4
#define TLF35584_LOCK1  QSPI3_WRITE (PROTCFG, 0xDF)   // lock sequence 1
#define TLF35584_LOCK2  QSPI3_WRITE (PROTCFG, 0x34)   // lock sequence 2
#define TLF35584_LOCK3  QSPI3_WRITE (PROTCFG, 0xBE)   // lock sequence 3
#define TLF35584_LOCK4  QSPI3_WRITE (PROTCFG, 0xCA)   // lock sequence 4

#define	BACON_DEFAULT (0x7206187 | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=0,IPRE=3,LAST=1 => TIDLEA,B= 0.32us;tInterframe=  0.64us
#define	BACON_40us    (0x72061BB | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=3,IPRE=5,LAST=1 => TIDLEA,B=20.48us;tInterframe= 40.96us
#define	BACON_60us    (0x72061DB | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=5,IPRE=5,LAST=1 => TIDLEA,B=30.72us;tInterframe= 61.44us
#define	BACON_120us   (0x72061AD | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=2,IPRE=6,LAST=1 => TIDLEA,B=61.44us;tInterframe=122.88us
#define	BACON_160us   (0x720618F | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=0,IPRE=7,LAST=1 => TIDLEA,B=81.92us;tInterframe=163.84us
#define	BACON_320us   (0x720619F | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=1,IPRE=7,LAST=1 => TIDLEA,B=163.84us;tInterframe=327.68us
#define	BACON_490us   (0x72061AF | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=2,IPRE=7,LAST=1 => TIDLEA,B=245.76us;tInterframe=491.52us
#define	BACON_810us   (0x72061CF | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=4,IPRE=7,LAST=1 => TIDLEA,B=409.6us;tInterframe=819.2us
#define	BACON_1100us  (0x72061EF | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=6,IPRE=7,LAST=1 => TIDLEA,B=573.44us;tInterframe=1146.88us
#define	BACON_1300us  (0x72061FF | TLF35584_SLSO << 28) // CS=QSPI_SLSO,DL=14,BYTE=0,MSB=1,UINT=0,PARTYP=0,TRAIL=0,TPRE=3,LEAD=0,LPRE=3,IDLE=7,IPRE=7,LAST=1 => TIDLEA,B=655.36us;tInterframe=1310.72us


/***************************** Source File Types ******************************/
#ifdef __cplusplus
extern "C" {
#endif

extern int32_t qspi3mstr_Enable(boolean bState);
extern int32_t qspi3mstr_contSend(QspiCs_t CS, uint16_t u16Length, uint16_t* pu8SrcBuff, uint16_t* pu8DstBuff);
extern boolean qspi3mstr_waitForRxDone( void );
extern boolean qspi3mstr_sshotSend( const uint32_t* pu32TxBuff, uint16_t* pu32RxBuff, uint32_t u32TxLen, uint32_t u32RxLen);

#ifdef __cplusplus
}
#endif

#endif /* QSPI_INC_QSPIMASTER_H_ */

