/*
 * led.c
 *
 *  Created on: Jan 30, 2024
 *      Author: rgeng
 */

#include <IfxPort.h>
#include <IfxPort_reg.h>
#include <freeRTOS.h>
#include <queue.h>
#include "shell.h"
#include "ecu8tr_cmd.h"
#include "../../../drivers/adbms/adbms_family_select.h"

ECU8TR_UPGRADE_STATUS_e bootloader_get_upgradeStatus( void );


/* Task which runs the LED1 app */
static void task_app_led1(void *arg)
{
	IfxPort_setPinHigh( &MODULE_P20, 7 );
	vTaskDelay(pdMS_TO_TICKS(500));

    while (1)
    {
        /* Toggle LED1 state */
    	IfxPort_togglePin( &MODULE_P20, 7 );
        /* Delay 250ms */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* Task which runs the LED1 app */
static void task_app_led2(void *arg)
{
	IfxPort_setPinLow( &MODULE_P20, 8 );
	vTaskDelay(pdMS_TO_TICKS(500));

	while (1)
	{
		/* Toggle LED1 state */
		IfxPort_togglePin( &MODULE_P20, 8 );
		/* Delay 250ms */
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}


static void task_r_g_led(void *arg)
{
	while (1)
	{
		if( bootloader_get_upgradeStatus() == UPGRADE_STATUS_IN_PROGRESS )
		{
			IfxPort_setPinLow( &MODULE_P33, 14 );
			IfxPort_togglePin( &MODULE_P34, 4 );
			vTaskDelay(pdMS_TO_TICKS(100));
			continue;
		}
		else if( bootloader_get_upgradeStatus() == UPGRADE_STATUS_FAILURE )
		{
			PRINTF( "The upgrade failed\r\n" );
			IfxPort_setPinLow( &MODULE_P33, 14 );
			IfxPort_togglePin( &MODULE_P34, 4 );
			vTaskDelay(pdMS_TO_TICKS(100));
			continue;
		}
		else if( bootloader_get_upgradeStatus() == UPGRADE_STATUS_SUCCESS )
		{
			IfxPort_setPinHigh( &MODULE_P33, 14 );	//Green
			IfxPort_setPinLow( &MODULE_P34, 4 );	//RED
			vTaskDelay(pdMS_TO_TICKS(100));
			continue;
		}

#if 1
		if( ADBMS_GET_STATE_FN() == ECU8TR_ADBMS6830_ERROR  )
		{
			IfxPort_setPinLow( &MODULE_P33, 14 );
			IfxPort_togglePin( &MODULE_P34, 4 );
		}
		else if( ADBMS_GET_STATE_FN() == ECU8TR_ADBMS6830_OK  )
		{
			IfxPort_togglePin( &MODULE_P33, 14 );	//Green
			IfxPort_setPinLow( &MODULE_P34, 4 );	//RED OFF
		}
		else
		{
			IfxPort_setPinHigh( &MODULE_P33, 14 );	//Green ON
			IfxPort_setPinLow( &MODULE_P34, 4 );	//RED OFF
		}
#else
		if( ecu8tr_getTLE9012State() == ECU8TR_TLE9012_WAKEUP_DONE  )
		{
			IfxPort_setPinHigh( &MODULE_P33, 14 );	//Green ON
			IfxPort_setPinLow( &MODULE_P34, 4 );	//RED OFF
		}
		else if(ecu8tr_getTLE9012State() == ECU8TR_TLE9012_DATA_STREAMING )
		{
			IfxPort_togglePin( &MODULE_P33, 14 );	//Green
			IfxPort_setPinLow( &MODULE_P34, 4 );	//RED OFF
		}
		else if( (ecu8tr_getTLE9012State()== ECU8TR_TLE9012_IDLE) || (ecu8tr_getTLE9012State() == ECU8TR_TLE9012_SLEEP) )
		{
			IfxPort_setPinLow( &MODULE_P33, 14 );
			IfxPort_togglePin( &MODULE_P34, 4 );
		}
		else
		{
			IfxPort_setPinHigh( &MODULE_P33, 14 );
			IfxPort_togglePin( &MODULE_P34, 4 );
		}

#endif
		vTaskDelay(pdMS_TO_TICKS(500));

	}

}


void led_startTasks( void )
{

    /* Create LED1 app task */
   xTaskCreate( task_app_led1, "LED_STATUS1", configMINIMAL_STACK_SIZE, NULL, 0, NULL );
   xTaskCreate( task_app_led2, "APP LED_STATUS2", configMINIMAL_STACK_SIZE, NULL, 0, NULL );
   xTaskCreate( task_r_g_led, "APP G_R", configMINIMAL_STACK_SIZE, NULL, 10, NULL );			//The tcp/ip task prio = 4, here we use 10, to make sure the led flashing corrctly.


	//Shell doesn't work well in the freeRTOS environment, because of STM used by both shell and freeRTOS
	//start_shell();
}



