/**
 ******************************************************************************
 * @file    machine.h
 * @author	JW
 * @version V0.1.0
 * @date    Jan 16, 2017
 * @brief
 ***
 * @author	JW
 * @version V0.2.0
 * @date    May2018
 * @brief	Fixed sem_WaitSetLock()
 ***
 * @author	JW
 * @version V0.2.0
 * @date    Jun2018
 * @brief	Added sem_WaitSetLockFor()
 * @version	V0.0.3
 * @date	Jan2020
 * @brief	Added function to convert coreId to TOS based of TX3xx variant
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
#include <stdint.h>
/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
/* Application includes */


#ifndef MACHINE_INC_MACHINE_H_
#define MACHINE_INC_MACHINE_H_


/*************************** Source File Constants ****************************/
#define kuEVENT_MCU0_INIT_DONE 			(0x00000001u)
#define kuEVENT_MCU1_INIT_DONE 			(0x00000002u)
#define kuEVENT_MCU2_INIT_DONE 			(0x00000004u)
#define kuEVENT_MCU3_INIT_DONE 			(0x00000008u)
#define kuEVENT_MCU4_INIT_DONE 			(0x00000010u)
#define kuEVENT_MCU5_INIT_DONE 			(0x00000020u)
#define kuEVENT_MCU_STATUS_SYNC			(0x80000000u)

/** \brief Identifier of interrupt service provider, which handles the interrupt service request. */
#if (__CPU_TC39XB__ == 1)
#define kuTC39XBCPU0						(0u)	/**< \brief CPU0 */
#define kuTC39XBCPU1						(1u)	/**< \brief CPU1 */
#define kuTC39XBCPU2						(2u)	/**< \brief CPU2 */
#define kuTOS_CORE0						(0u)	/**< \brief CPU0 interrupt service provider, which handles the interrupt service request. */
#define kuTOS_DMA						(1u)	/**< \brief DMA interrupt service provider, which handles the interrupt service request. */
#define kuTOS_CORE1						(2u)	/**< \brief CPU1 interrupt service provider, which handles the interrupt service request. */
#define kuTOS_CORE2						(3u)	/**< \brief CPU2 interrupt service provider, which handles the interrupt service request. */
#endif /* (__CPU_TC37X__ == 1) */
#define kuTOS_NA						(7u)	/**< \brief Reserved (no action) interrupt service provider, which handles the interrupt service request. */

/***************************** Source File Types ******************************/


/****************************** Application APIs ******************************/
inline void sem_Set(volatile uint32_t* pu32Signal, unsigned uMask)
{	/* Will set only the bits in the flag buffer according to uMask */
	/* Make the previous instruction explicitly write back all data before the
	following instruction can fetch any new data */
	__dsync();
	/* Atomic */
	__swapmskw(((uint32_t*)pu32Signal), 0xFFFFFFFFu, uMask);
}


inline void sem_Release(volatile uint32_t* pu32Signal, unsigned uMask)
{/* Will clear only the bits in the flag buffer according to uMask */
	/* Make the previous instruction explicitly write back all data before the
	following instruction can fetch any new data */
	__dsync();
	/* Atomic */
	__swapmskw(((uint32_t*)pu32Signal), 0, uMask);
}


inline void sem_WaitSetLock(volatile uint32_t* pu32Signal, unsigned u32Mask)
{
	uint32_t compare = *pu32Signal & ~u32Mask;
	uint32_t value = compare | u32Mask;


	__dsync();
	/* Spin until compare == the value pu32Signal is pointing at then update
	 * with value
	 */
	while(__cmpswapw(((uint32_t*)pu32Signal), value, compare));
}


inline boolean sem_WaitSetLockFor(volatile uint32_t* pu32Signal, unsigned u32Mask, uint32_t u32Delay)
{
	uint32_t compare = *pu32Signal & ~u32Mask;
	uint32_t value = compare | u32Mask;


	__dsync();
	/* Spin until compare == the value pu32Signal is pointing at then update
	 * with value
	 */
	do{
		if(u32Delay == 0)
		{
			return(FALSE);
		}
		else
		{
			u32Delay--;
		}
	} while(__cmpswapw(((uint32_t*)pu32Signal), value, compare));

	return(TRUE);
}


inline void sem_WaitForEvent(volatile uint32_t* pu32Signal, uint32_t u32Mask)
{
	while((*pu32Signal & u32Mask) != u32Mask);
}


inline boolean sem_TestSetLock(volatile uint32_t* pu32Signal, uint32_t u32Mask)
{
	uint32_t compare = *pu32Signal & ~u32Mask;
	uint32_t value = compare | u32Mask;
	boolean bRtn = FALSE;


	__dsync();
	if(__cmpswapw(((uint32_t*)pu32Signal), value, compare) == 0)
	{/* LOCK acquired */
		bRtn = TRUE;
	}

	return(bRtn);
}


inline boolean sem_TestForEvent(volatile uint32_t* pu32Signal, uint32_t u32Mask)
{/* When memory content (*pu32Signal) == compare value (Arg3) --> value (Arg2) is transfered to memory */
	/* NOTE: Returns the value stored in memory content (*pu32Signal) */
	if((*pu32Signal & u32Mask) == u32Mask)
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}

inline boolean sem_TestForMultipleEvent(volatile uint32_t* pu32Signal, uint32_t u32Mask)
{/* When memory content (*pu32Signal) == compare value (Arg3) --> value (Arg2) is transfered to memory */
	/* NOTE: Returns the value stored in memory content (*pu32Signal) */
	if((*pu32Signal & u32Mask) != 0u)
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}

inline uint16_t tos_CpuToTos(int16_t u16CPU)
{
	uint16_t u16Rtn = kuTOS_NA;


	switch(u16CPU)
	{
	case kuTC39XBCPU0:
		u16Rtn = kuTOS_CORE0;
		break;

	case kuTC39XBCPU1:
		u16Rtn = kuTOS_CORE1;
		break;

	case kuTC39XBCPU2:
		u16Rtn = kuTOS_CORE2;
		break;

	default:
		break;
	}

	return(u16Rtn);
}


#endif /* MACHINE_INC_MACHINE_H_ */

