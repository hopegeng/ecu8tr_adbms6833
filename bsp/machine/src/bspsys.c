/**
 ******************************************************************************
 * @file    sys.c
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


/*----- Includes -------------------------------------------------------------*/
/* C includes */
#include <bspsys.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
#include "Scu/Std/IfxScuCcu.h"
#include "Scu/Std/IfxScuRcu.h"
#include "IfxSrc_reg.h"
#include "Cpu/Std/IfxCpu.h"
/* TASKING PINMAPPER includes */
#include "aurix_pin_mappings.h"
/* Application includes */
#include "din.h"
#include "dout.h"

#undef NOP	/* NOP is defined by Mcal_Compilers.h and iLLD  and */
#include "Port.h"
#include "Mcu.h"
#include "Dio.h"
#include "Spi.h"
#include "Can_17_McmCan.h"
#include "Uart.h"
#include "Irq.h"

/*----- Protected Variables --------------------------------------------------*/


/*----- Prototypes -----------------------------------------------------------*/
void bspsys_Init(void);


/*----- Macros ---------------------------------------------------------------*/
#define BSPSYS_INIT_TC0() bspsys_Init();
#define BSPSYS_INIT_TC1()
#define BSPSYS_INIT_TC2()
#define BSPSYS_INIT_TC3()
#define BSPSYS_INIT_TC4()
#define BSPSYS_INIT_TC5()

#define CAN_NODE1 Can_17_McmCanConf_CanController_CanController_NODE1
#define CAN_NODE2 Can_17_McmCanConf_CanController_CanController_NODE2
#define CAN_SRC_SET_SRE                 (0x00000400U)
/* Set CLRR to clear SRR bit and disable SRE bit */
#define CAN_SRC_SET_CLRR_DISABLE_SRE     (0x52000000U)

/*----- Externals ------------------------------------------------------------*/
//void _call_init(void) {bspsys_Init();}
void _call_init(void) {BSPSYS_INIT_TC0()}
void _call_init_tc1(void) {BSPSYS_INIT_TC1()}
void _call_init_tc2(void) {BSPSYS_INIT_TC2()}
void _call_init_tc3(void) {BSPSYS_INIT_TC3()}
void _call_init_tc4(void) {BSPSYS_INIT_TC4()}
void _call_init_tc5(void) {BSPSYS_INIT_TC5()}
void bspsys_ResetStatus(void);

/*----- Member variables -----------------------------------------------------*/
static SysLastReset_t bspsysLastReset;


/* CAN SRC temporary storage buffers */
static uint32_t SrcCanNode1Int[4] = {0};
static uint32_t SrcCanNode2Int[4] = {0};

/*----- Constant member variables --------------------------------------------*/


/*----- Private Functions ----------------------------------------------------*/

/* MCAL Init function */
/*******************************************************************************
 * @author  BT
 * @date    May2021 (created)
 * @brief   Initialise Port module to enable serial communication
 *
 * @param   NONE.
 * @retval  NONE.
*******************************************************************************/


void App_Port_Init(void)
{

	/* Get pointer to Mcal port configuration in Port_PBcfg.c */
	const Port_ConfigType * ConfigPtr = NULL_PTR;
	ConfigPtr = &Port_Config;
	Port_Init(ConfigPtr);

	/* Enable 6820 on the old BMS board */
	//P13_OUT.B.P2 = 1;
}

void App_Spi_Init( void )
{
	Spi_Init( &Spi_Config );
}
/*******************************************************************************
 * @author  BT
 * @date    May2021 (created)
 * @brief   Initialize Mcu module to enable clocks
 *
 * @param   NONE.
 * @retval  NONE.
 *
*******************************************************************************/
void App_Mcu_Init(void)
{
	const Mcu_ConfigType * ConfigPtr = NULL_PTR;
	volatile Mcu_ClockType ClockID = 0;
	Std_ReturnType InitClockRetVal;
	Mcu_PllStatusType Mcu_GetPllStatusRetVal = MCU_PLL_STATUS_UNDEFINED;

	/* Get pointer to Mcal Mcu configuration in Mcu_PBcfg.c */
	ConfigPtr = &Mcu_Config;
	Mcu_Init(ConfigPtr);
	InitClockRetVal = Mcu_InitClock(ClockID);

	if(InitClockRetVal == E_OK)
	{
		do
		{
			Mcu_GetPllStatusRetVal = Mcu_GetPllStatus ();
		} while(Mcu_GetPllStatusRetVal != MCU_PLL_LOCKED);

#if (MCU_DISTRIBUTE_PLL_CLOCK_API == STD_ON)
		Mcu_DistributePllClock ();
#endif
	}
}


/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    Mar2020 (created)
 * @brief   Enable Service Requests for a Can_17_McmCan Controller
 *
 * @param   node Controller id as per Can_17_McmCan_Cfg.h
 * @retval  NONE.
 *
 ******************************************************************************/
void Can_EnableServiceRequest(uint8_t node)
{
	/* Enter Critical Section */
	SuspendAllInterrupts();

	if ((uint8_t)CAN_NODE1 == node)
	{
		SRC_CAN_CAN0_INT4.U |= CAN_SRC_SET_SRE;
		SRC_CAN_CAN0_INT5.U |= CAN_SRC_SET_SRE;
		SRC_CAN_CAN0_INT6.U |= CAN_SRC_SET_SRE;
		SRC_CAN_CAN0_INT7.U |= CAN_SRC_SET_SRE;

		SRC_CAN_CAN0_INT4.U |= SrcCanNode1Int[0];
		SRC_CAN_CAN0_INT5.U |= SrcCanNode1Int[1];
		SRC_CAN_CAN0_INT6.U |= SrcCanNode1Int[2];
		SRC_CAN_CAN0_INT7.U |= SrcCanNode1Int[3];
	}

	if ((uint8_t)CAN_NODE2 == node)
	{
		SRC_CAN_CAN0_INT8.U |= CAN_SRC_SET_SRE;
		SRC_CAN_CAN0_INT9.U |= CAN_SRC_SET_SRE;
		SRC_CAN_CAN0_INT10.U |= CAN_SRC_SET_SRE;
		SRC_CAN_CAN0_INT11.U |= CAN_SRC_SET_SRE;

		SRC_CAN_CAN0_INT8.U |= SrcCanNode2Int[0];
		SRC_CAN_CAN0_INT9.U |= SrcCanNode2Int[1];
		SRC_CAN_CAN0_INT10.U |= SrcCanNode2Int[2];
		SRC_CAN_CAN0_INT11.U |= SrcCanNode2Int[3];
	}

	/* Exit Critical Section */
	ResumeAllInterrupts();
}

/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    Mar2020 (created)
 * @brief   Disable Service Requests for a Can_17_McmCan Controller
 *
 * @param   node Controller id as per Can_17_McmCan_Cfg.h
 * @retval  NONE.
 *
 ******************************************************************************/
static void Can_DisableServiceRequest(uint8_t node)
{
	/* Enter Critical Section */
	SuspendAllInterrupts();

	if ((uint8_t)CAN_NODE1 == node)
	{
		SrcCanNode1Int[0] = SRC_CAN_CAN0_INT4.U;
		SrcCanNode1Int[1] = SRC_CAN_CAN0_INT5.U;
		SrcCanNode1Int[2] = SRC_CAN_CAN0_INT6.U;
		SrcCanNode1Int[3] = SRC_CAN_CAN0_INT7.U;

		SRC_CAN_CAN0_INT4.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
		SRC_CAN_CAN0_INT5.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
		SRC_CAN_CAN0_INT6.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
		SRC_CAN_CAN0_INT7.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
	}

	if ((uint8_t)CAN_NODE2 == node)
	{
		SrcCanNode2Int[0] = SRC_CAN_CAN0_INT8.U;
		SrcCanNode2Int[1] = SRC_CAN_CAN0_INT9.U;
		SrcCanNode2Int[2] = SRC_CAN_CAN0_INT10.U;
		SrcCanNode2Int[3] = SRC_CAN_CAN0_INT11.U;

		SRC_CAN_CAN0_INT8.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
		SRC_CAN_CAN0_INT9.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
		SRC_CAN_CAN0_INT10.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
		SRC_CAN_CAN0_INT11.U = CAN_SRC_SET_CLRR_DISABLE_SRE;
	}

	/* Exit Critical Section */
	ResumeAllInterrupts();
}

/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    Mar2020 (created)
 * @brief   Initialize MCMCAN module
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
void App_CAN_Init(void)
{
	const Can_17_McmCan_ConfigType * ConfigPtr = NULL_PTR;


	/* Set CAN interrupt priority & TOS */
	IrqCan_Init();

	/* Get pointer to Mcal MCMCAN configuration in Can_17_McmCan_PBcfg.c */
	ConfigPtr = &Can_17_McmCan_Config;
	Can_17_McmCan_Init(ConfigPtr);

	/* Mcal requires this to be called before the first call of the
	 * Can_17_McmCan_EnableControllerInterrupts routine */
	Can_17_McmCan_DisableControllerInterrupts(CAN_NODE1);
	Can_17_McmCan_DisableControllerInterrupts(CAN_NODE2);

	Can_EnableServiceRequest(CAN_NODE1);
	Can_EnableServiceRequest(CAN_NODE2);

	Can_17_McmCan_EnableControllerInterrupts(CAN_NODE1);
	Can_17_McmCan_EnableControllerInterrupts(CAN_NODE2);
}

void App_UART_Init( void )
{
	const Uart_ConfigType * ConfigPtr = 0;
	ConfigPtr = &Uart_Config;
	Uart_Init(ConfigPtr);

	IrqAsclin_Init();

	SRC_ASCLIN2TX.B.SRE = 0x01u;
	SRC_ASCLIN2RX.B.SRE = 0x01u;
	SRC_ASCLIN2ERR.B.SRE = (1u);

	SRC_ASCLIN1TX.B.SRE = 0x01u;
	SRC_ASCLIN1RX.B.SRE = 0x01u;
	SRC_ASCLIN1ERR.B.SRE = (1u);


	SRC_ASCLIN9TX.B.SRE = 0x01u;
	SRC_ASCLIN9RX.B.SRE = 0x01u;
	SRC_ASCLIN9ERR.B.SRE = (1u);

}

/**
 *******************************************************************************
 *
 * @author  JW
 * @date    Jul2018 (created)
 * @brief   bspsys_Init()
 * 			TASKING callback, called from cstart.c after core0 h/w and C
 * 			initialization and before main() is called
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/

void bspsys_Init(void)
{

	App_Mcu_Init();

#if 0 //Cause the board resetting irregually
	App_Port_Init();
#else
	/* Status LED */
	MODULE_P20.IOCR4.B.PC7 = 0x10;
	MODULE_P20.IOCR8.B.PC8 = 0x10;
	MODULE_P20.OUT.B.P7 = 1;
	MODULE_P20.OUT.B.P8 = 0;

	/* ASCLIN1 UART TLE9012 HS */
	MODULE_P14.IOCR8.B.PC10 = 0x14;

	/* ASCLIN9 UART TLE9012 LS */
	MODULE_P14.IOCR4.B.PC7 = 0x14;
	/* ASCLIN2 UART CLI OUT */
	MODULE_P02.IOCR8.B.PC9 = 0x12;

	/* CAN NODE 1 */
	MODULE_P14.IOCR0.B.PC0 = 0x15;
	/* CAN NODE 2 */
	MODULE_P15.IOCR0.B.PC0 = 0x15;

	/* QSPI0 for isoSPI */
	MODULE_P20.IOCR8.B.PC11 = 0x13;		/* Clk */
	MODULE_P20.IOCR12.B.PC13 = 0x13;	/* CS2 */
	MODULE_P20.IOCR12.B.PC14 = 0x13;	/* MTSR */

	/* GPIO Out */
	MODULE_P34.IOCR0.B.PC1 = 0x10;		/*CCu60_COUT63*/
	MODULE_P34.OUT.B.P1 = 0x0;

	MODULE_P34.IOCR4.B.PC4 = 0x10;		/*CHARGE_CONTROL*/
	MODULE_P34.OUT.B.P4 = 0x0;
	MODULE_P34.IOCR4.B.PC5 = 0x10;		/* CCU60_COUT61 */
	MODULE_P34.OUT.B.P5 = 0x01;

	MODULE_P33.IOCR0.B.PC2 = 0x10;	/* VOUT_H_DEN_DISABLE */
	MODULE_P33.OUT.B.P2 = 0x0;
	MODULE_P33.IOCR0.B.PC3 = 0x10;	/* VOUT_H_DSEL_DISABLE */
	MODULE_P33.OUT.B.P3 = 0x0;
	MODULE_P33.IOCR8.B.PC9 = 0x10;	/*Load Control */
	MODULE_P33.OUT.B.P9 = 0x0;
	MODULE_P33.IOCR8.B.PC10 = 0x10;
	MODULE_P33.OUT.B.P10 = 0x0;
	MODULE_P33.IOCR8.B.PC11 = 0x10;	/*STATUS_2 */
	MODULE_P33.OUT.B.P11 = 0x1;
	MODULE_P33.IOCR12.B.PC12 = 0x10;	/*LTC2949_CONTROL */
	MODULE_P33.OUT.B.P12 = 1;
	MODULE_P33.IOCR12.B.PC14 = 0x10;	/*VOUT_L_1_CONTROL */
	MODULE_P33.OUT.B.P14 = 1;
	MODULE_P33.IOCR12.B.PC15 = 0x10;	/*VOUT_L_0_CONTROL*/
	MODULE_P33.OUT.B.P15 = 0;
	MODULE_P21.IOCR0.B.PC1 = 0x10;		/* WAIT_BRQA1 */
	MODULE_P21.OUT.B.P1 = 1;

	MODULE_P20.IOCR8.B.PC10 = 0x10;
	MODULE_P20.OUT.B.P10 = 0x0;


#endif


	App_Spi_Init();
	App_CAN_Init();
	App_UART_Init();


	Dio_WriteChannel( DioConf_DioChannel_STATUS_D2, 1 );
	Dio_WriteChannel( DioConf_DioChannel_STATUS_D3, 0 );

	Dio_WriteChannel( DioConf_DioChannel_IsoSpi_Enable, 1 );

}/* END bspsys_Init() */


/**
 *******************************************************************************
 *
 * @author  JW
 * @date    Mar2019 (created)
 * @brief   bspsys_SetUserInfo()
 *
 * @param   .
 * @retval  .
 *
 ******************************************************************************/
uint16_t bspsys_SetUserInfo(uint16_t u16UserKey)
{
	unsigned short u16BldrPw;


	u16BldrPw = IfxScuWdt_getSafetyWatchdogPassword();
	IfxScuWdt_clearSafetyEndinit(u16BldrPw);

	SCU_RSTCON2.B.USRINFO = u16UserKey;

	IfxScuWdt_setSafetyEndinit(u16BldrPw);

	return(SCU_RSTCON2.B.USRINFO);
}/* END bspsys_SetUserInfo() */


/*----- Protected Functions --------------------------------------------------*/


/*----- Public Functions -----------------------------------------------------*/
/**
 *******************************************************************************
 *
 * @author  JW
 * @date
 * @brief   Function: bspsys_CRC32()
 * 			Compute CRC32 using TC1.6.1 CRC32 instruction
 *
 * @param	u32BASE of type uint32_t.  Start address
 * @param	u32WORDS of type uint32_t. Number of 32-BIT WORDs.
 * @retval 	uint32_t, CRC32 of the range
 *
 ******************************************************************************/
inline uint32_t bspsys_CRC32(uint32_t u32BASE, uint32_t u32WORDS)
{
	uint32_t u32Crc = 0u;
	volatile uint32_t* pu32Add;


	/* Ensure the address is 32-BIT aligned */
	if((u32BASE & 0x00000003u) == 0)
	{
		pu32Add = ((uint32_t*)u32BASE);

		/* Seed initial CRC value, which is inverted by CRC32 instruction */
		for(uint32_t u32Index = 0; u32Index < u32WORDS; u32Index++)
		{
			u32Crc = __crc32(u32Crc, *pu32Add++);
		}
	}

	return(u32Crc);
}/* bspsys_CRC32() */


/**
 *******************************************************************************
 *
 * @author  JW
 * @date    Feb2017 (created)
 * @brief   sys_GetLastReset()
 * 			Return the cause of the last reset (Numerical data).
 *
 * @param   pBuff of type IfxScuRcu_ResetCode*.  Input buffer to receive structure
 * 			cause of last reset.
 * @retval  NONE.
 *
 ******************************************************************************/
void bspsys_GetLastReset(SysLastReset_t* pBuff)
{
	if(pBuff != ((void*)0))
	{
		pBuff->type = bspsysLastReset.type;
		pBuff->u32Cause = bspsysLastReset.u32Cause;
	}
}/* END sys_GetLastReset() */

/**
 *******************************************************************************
 *
 * @author  JW
 * @date    Feb2017 (created)
 * @brief   sys_ResetStatus()
 *
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
void bspsys_ResetStatus(void)
{
	uint16_t u16Pw;
	IfxScuRcu_ResetCode reset =	IfxScuRcu_evaluateReset();


	bspsysLastReset.type = eSysResetWarm;
	bspsysLastReset.u32Cause = SCU_RSTSTAT.U;

	if(reset.resetType == IfxScuRcu_ResetType_coldpoweron)
	{/* Clear Cold Reset Status */
		u16Pw = IfxScuWdt_getCpuWatchdogPasswordInline(&MODULE_SCU.WDTCPU[IfxCpu_getCoreIndex()]);
		IfxScuWdt_clearCpuEndinitInline(&MODULE_SCU.WDTCPU[IfxCpu_getCoreIndex()], u16Pw);
		MODULE_SCU.RSTCON2.B.CLRC = 1u;
		MODULE_SCU.RSTCON2.B.USRINFO = 0xDEADu;
		IfxScuWdt_setCpuEndinitInline(&MODULE_SCU.WDTCPU[IfxCpu_getCoreIndex()], u16Pw);
		bspsysLastReset.type = eSysResetCold;
	}
}/* END sys_ResetStatus() */


/**
 *******************************************************************************
 *
 * @author  JW
 * @date    Feb2017 (created)
 * @brief   sys_Reset()
 *			Called to issues either a system of application MCU reset.
 *
 * @param   type of type SysRstType_t.
 * 			eSysSystem: A System Reset is generated for a trigger of
 *			Software reset
 *			eSysApplication: An Application Reset is generated for a trigger
 *			of Software reset
 *
 * @retval  NONE.
 *
 ******************************************************************************/
void bspsys_Reset(SysRstType_t type)
{
	if(type == eSysSystem)
	{
		IfxScuRcu_performReset(IfxScuRcu_ResetType_system, 0xBEEFu);
	}
	else
	{
		IfxScuRcu_performReset(IfxScuRcu_ResetType_application, 0xBEEFu);
	}
}/* sys_ResetType() */

