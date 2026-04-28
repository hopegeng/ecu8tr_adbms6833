/*
 * can_if.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#include <freeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <Ifx_Types.h>
#include <IfxCan_Can.h>
#include "can_drv.h"
#include "can_if.h"
#include "tools.h"

SemaphoreHandle_t canTxSemaphore = NULL;


void CanIf_Init(void)
{
	CanDrv_initCan();

	canTxSemaphore = xSemaphoreCreateBinary();
	if (canTxSemaphore == NULL)
	{
	   // Handle semaphore creation failure
		__debug();
    }
}

Bmu_ReturnType CanIf_Transmit(const CanIf_MsgType *msg)
{
	uint32 low;
	uint32 high;
    TickType_t startTicks;
    BaseType_t semResult;
    IfxCan_Status txStatus;

    if( msg == 0 )
    {
        return BMU_E_PARAM;
    }

    low = bytes_to_u32_le( msg->data );
    high = bytes_to_u32_le( msg->data+4 );

    startTicks = xTaskGetTickCount();

    do
    {
        txStatus = can_transmitCanMessage( msg->id, low, high );

        if( txStatus == IfxCan_Status_ok )
        {
            break;
        }

        if( txStatus != IfxCan_Status_notSentBusy )
        {
            return BMU_E_BUSY;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    } while( (xTaskGetTickCount() - startTicks) < pdMS_TO_TICKS(50) );

    if( txStatus != IfxCan_Status_ok )
    {
        return BMU_E_BUSY;
    }

    semResult = xSemaphoreTake( canTxSemaphore, pdMS_TO_TICKS(400) );
    if( semResult == pdFALSE )
    {
    	return BMU_E_BUSY;
    }


    return BMU_OK;

}

