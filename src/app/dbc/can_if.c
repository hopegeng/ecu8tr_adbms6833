/*
 * can_if.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "can_if.h"

/* Replace with real TC399 MCMCAN driver integration */
void CanIf_Init(void)
{
    /* initCAN(); or your own CAN init */
}

Bmu_ReturnType CanIf_Transmit(const CanIf_MsgType *msg)
{
    if (msg == 0)
    {
        return BMU_E_PARAM;
    }

    /* TODO:
     * 1. Map msg->id, msg->dlc, msg->is_extended, msg->data[]
     *    into IfxCan_Message / Tx buffer structure
     * 2. Request transmission
     * 3. Return BMU_OK if queued successfully
     */

    (void)msg;
    return BMU_OK;
}

