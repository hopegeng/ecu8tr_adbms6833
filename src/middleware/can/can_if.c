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

    if( msg == 0 )
    {
        return BMU_E_PARAM;
    }

    low = bytes_to_u32_le( msg->data );
    high = bytes_to_u32_le( msg->data+4 );

    if( can_transmitCanMessage( msg->id, low, high ) != IfxCan_Status_ok )
    {
    	return BMU_E_BUSY;
    }

    if( pdFALSE == xSemaphoreTake( canTxSemaphore, pdMS_TO_TICKS(400) ) )
    {
    	return BMU_E_BUSY;
    }


    return BMU_OK;

}

