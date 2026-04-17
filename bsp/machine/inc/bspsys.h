/**
 ******************************************************************************
 * @file    sys.h
 * @author	JW
 * @version V0.1.0
 * @date    Nov2015
 * @brief
 ******************************************************************************
 * @attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2020 Neutron Controls</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
 *
 ******************************************************************************
 */


/* C includes */
#include <stdint.h>
/* IFX includes */
#include "Scu/Std/IfxScuRcu.h"


#ifndef BSPSYS_H_
#define BSPSYS_H_


/*************************** Source File Constants ****************************/
#define SYS_RSTSTAT_LBTERM								((unsigned long)0x40000000)	/* BIT30 */
#define SYS_RSTSTAT_LBPORST								((unsigned long)0x20000000)	/* BIT29 */
#define SYS_RSTSTAT_STBYR 								((unsigned long)0x10000000)	/* BIT28 */
#define SYS_RSTSTAT_HSMA            					((unsigned long)0x08000000)	/* BIT27 */
#define SYS_RSTSTAT_HSMS            					((unsigned long)0x04000000)	/* BIT26 */
#define SYS_RSTSTAT_SWD 								((unsigned long)0x02000000)	/* BIT25 */
#define SYS_RSTSTAT_EVR33 								((unsigned long)0x01000000)	/* BIT24 */
#define SYS_RSTSTAT_EVRC 								((unsigned long)0x00800000)	/* BIT23 */
#define SYS_RSTSTAT_CB3 								((unsigned long)0x00100000)	/* BIT20 */
#define SYS_RSTSTAT_CB1 								((unsigned long)0x00080000)	/* BIT19 */
#define SYS_RSTSTAT_CB0 								((unsigned long)0x00040000)	/* BIT18 */
#define SYS_RSTSTAT_PORST 								((unsigned long)0x00010000)	/* BIT16 */
#define SYS_RSTSTAT_STM5 								((unsigned long)0x00000400)	/* BIT10 */
#define SYS_RSTSTAT_STM4 								((unsigned long)0x00000200)	/* BIT09 */
#define SYS_RSTSTAT_STM3 								((unsigned long)0x00000100)	/* BIT08 */
#define SYS_RSTSTAT_STM2 								((unsigned long)0x00000080)	/* BIT07 */
#define SYS_RSTSTAT_STM1 								((unsigned long)0x00000040)	/* BIT06 */
#define SYS_RSTSTAT_STM0 								((unsigned long)0x00000020)	/* BIT05 */
#define SYS_RSTSTAT_SW 									((unsigned long)0x00000010)	/* BIT04 */
#define SYS_RSTSTAT_SMU 								((unsigned long)0x00000008)	/* BIT03 */
#define SYS_RSTSTAT_ESR1 								((unsigned long)0x00000002)	/* BIT01 */
#define SYS_RSTSTAT_ESR0 								((unsigned long)0x00000001)	/* BIT00 */


/***************************** Source File Types ******************************/
typedef enum SysRstType_e
{
	eSysSystem,
	eSysApplication,

	eSysRstUnknown
} SysRstType_t;

typedef enum SysResetClass_e
{
	eSysResetWarm,
	eSysResetCold,
	eSysResetUnknown
} SysResetClass_t;

typedef struct SysLastReset_s
{
	SysResetClass_t type;
	unsigned long u32Cause;
} SysLastReset_t;


/****************************** Application APIs ******************************/
#ifdef __cplusplus
extern "C" {
uint32_t bspsys_CRC32(uint32_t u32BASE, uint32_t u32WORDS);
void bspsys_GetLastReset(SysLastReset_t* pBuff);
void bspsys_ResetStatus(void);
void bspsys_Reset(SysRstType_t type);
int bspsys(void);
}
#else
extern uint32_t bspsys_CRC32(uint32_t u32BASE, uint32_t u32WORDS);
extern void bspsys_GetLastReset(SysLastReset_t* pBuff);
extern void bspsys_ResetStatus(void);
extern void bspsys_Reset(SysRstType_t type);
extern int bspsys(void);
#endif


#endif /* BSPSYS_H_ */

