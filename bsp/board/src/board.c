/*
 * reset.c
 *
 *  Created on: Jan 24, 2024
 *      Author: rgeng
 */

#include <IfxPort.h>
#include <Ifx_Types.h>
#include <IfxCpu.h>
#include <IfxScuRcu.h>
#include <IfxPort.h>
#include <IfxPort_reg.h>
#include "board.h"

void board_reset( SysRstType_t type)
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



void gpio_init( void )
{
	/* What is MODULE_P34, looks like it is for CAN01 PHY */
	IfxPort_setPinModeOutput( &MODULE_P34, 1,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
	IfxPort_setPinLow( &MODULE_P34, 1 );

	IfxPort_setPinModeOutput( &MODULE_P20, 7,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
	IfxPort_setPinModeOutput( &MODULE_P20, 8,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);

	IfxPort_setPinModeOutput( &MODULE_P20, 3,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
	IfxPort_setPinModeOutput( &MODULE_P20, 6,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
	IfxPort_setPinModeOutput( &MODULE_P20, 10,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);	//Tle9015 reset
	IfxPort_setPinModeOutput( &MODULE_P32, 5,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);		//Tle9015 reset for ECU8


	IfxPort_setPinModeOutput( &MODULE_P33, 14,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);	//GREEN LED
	IfxPort_setPinLow( &MODULE_P33, 14 );
	IfxPort_setPinModeOutput( &MODULE_P34, 4,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);		//REG LED
	IfxPort_setPinHigh( &MODULE_P34, 4 );

}
