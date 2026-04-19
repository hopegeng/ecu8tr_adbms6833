/*
 * GETH_Debug.c
 *
 *  Created on: Dec 2023
 *      Author: rgeng
 */


/* IFX includes */
#include <stdint.h>
#include "Ifx_reg.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Cpu/Std/IfxCpu.h"
#include "Port/Std/IfxPort.h"
#include "Cpu/Std/Platform_Types.h"
#include "Geth/Std/IfxGeth.h"
#include "Geth/Eth/IfxGeth_Eth.h"
/* Application includes */
#include "shell.h"
#include "if_ether.h"
#include "GETH_Dev.h"
#include "GETH_Config.h"

extern GETH_CONFIG_t xGethConfig[];

void GETH_Debug_dump_rx_desc( uint8 gethIdx, uint8 chanIdx, uint8 offset )
{
	Ifx_GETH *pGeth;
	uint16 descIdx;
    volatile IfxGeth_RxDescr *pDesc;


    if( gethIdx == 1u )		/* Only Debug port 0 */
    {
    	return;
    }

	pGeth = xGethConfig[gethIdx].pGeth;
	descIdx = xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx;
	descIdx = ( descIdx + offset ) % IFXGETH_MAX_RX_DESCRIPTORS;
	pDesc = &xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx];

	PRINTF( "\r\nPort: %d, Channel: %d\r\n", gethIdx, chanIdx );
	PRINTF( "OWN = %d, CTEXT = %d, FD = %d, LD =%d, RS1V= %d, TSA = %d\r\n\r\n",
			pDesc->RDES3.W.OWN, pDesc->RDES3.W.CTXT, pDesc->RDES3.W.FD, pDesc->RDES3.W.LD, pDesc->RDES3.W.RS1V, pDesc->RDES1.W.TSA );

}


void GETH_Debug_print_upd_packet( uint8 gethIdx, uint8 *pFrame, uint16 frameLen )
{

	if( frameLen >= 60 && pFrame[36] == 0x22 && pFrame[37] == 0xb9 )		/* 0x22b9 = port 8889 */
	{
		PRINTF( "Received a UDP on port: %d, %s\r\n", gethIdx, pFrame+42 );

		PRINTF( "The current pointer = %d\r\n", 		xGethConfig[gethIdx].pRxChannel[0].u16CurDescIdx );
		for( int idx = 0; idx < 8; idx++ )
		{
			PRINTF( "Port %d, RxDesc %d,  Own = %d\r\n", gethIdx, idx, xGethConfig[gethIdx].pRxChannel[0].xRxDescList[idx].RDES3.W.OWN );
		}

	}
}

