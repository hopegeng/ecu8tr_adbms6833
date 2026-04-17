/*
 * GETH_OS.c
 *
 *  Created on: Dec 2023
 *      Author: rgeng
 */

/* C includes */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* IFX includes */
#include <Cpu/Std/IfxCpu.h>
#include <Cpu/Std/Platform_Types.h>
#include <Cpu/Irq/IfxCpu_Irq.h>
#include <IfxGeth.h>
/* LWIP includes */
#include <FreeRTOS.h>
#include <etharp.h>
#include <netif.h>
#include <tcpip.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

/* Application includes */
#include "Ifx_Console.h"
#include "shell.h"
#include "GETH_Dev.h"
#include "GETH_Debug.h"
#include "GETH_Config.h"
#include "GETH_Phy.h"
#include "if_ether.h"


#define LINK_TMR_INTERVAL							(1000u)		/* 1 Second */
#define ETHIF_RXPIPE_PRIORITY 						(5u)
#define ETHIF_RXPIPE_TASK_STACK_SIZE 				(512u) /* 512*4 = 2048 BYTES */


extern GETH_CONFIG_t xGethConfig[];
extern GETH_STATS_t xGethStatus[];
extern uint8_t network_def_config[5][8];


StaticTask_t xEthifRxPipeTaskTBC __align(4);
StackType_t xEthifRxPipeTaskStack[ETHIF_RXPIPE_TASK_STACK_SIZE] __align(4);
struct netif xGethNetIf[IFXGETH_NUM_MODULES] __align(4);
QueueHandle_t xRxQueueH = NULL_PTR;
xTaskHandle xRxTaskH = NULL_PTR;
boolean bLWIPEnabled = FALSE;
StaticSemaphore_t xTxMutexBuffer;
SemaphoreHandle_t xTxMutexH = NULL_PTR;

static uint8_t u8LinkUp[2] = { 0x00, 0x00 };
static uint8 ray_Debug_TX[64];
static err_t GETH_OS_link_output( struct netif* pNetIf, struct pbuf* pBuff )
{
	uint16 txDescIdx;
	uint8_t	gethIdx;
	uint8 chanIdx;
	GETH_CONFIG_t *pGethConfig;


	memcpy( ray_Debug_TX, pBuff->payload, (pBuff->len>64)? 64: pBuff->len );

	pGethConfig = (GETH_CONFIG_t*)pNetIf->state;
	gethIdx = (uint8_t)IfxGeth_getIndex( pGethConfig->pGeth );

#if ETH_PAD_SIZE
    pbuf_header( pBuff, -ETH_PAD_SIZE ); /* drop the padding word */
#endif

    chanIdx = 0u;
	txDescIdx = GETH_Dev_dma_xmit( gethIdx, chanIdx, pBuff->payload, pBuff->len );

#if ETH_PAD_SIZE
    pbuf_header( pBuff, ETH_PAD_SIZE ); /* drop the padding word */
#endif

	return ERR_OK;
}


static err_t GETH_OS_net_init( struct netif* pNetIf )
{
	uint8_t gethIdx;
	GETH_CONFIG_t *pGethConfig;


	pGethConfig = (GETH_CONFIG_t*)pNetIf->state;
	gethIdx = (uint8_t)IfxGeth_getIndex( pGethConfig->pGeth );

	/* Step 1: Initialize the MAC interface */
	GETH_Dev_enable_geth( TRUE, gethIdx );

	/* Step 2: Initialize the LWIP netIf */
	pNetIf->name[0] = 'N';
	pNetIf->name[1] = 'C';

	pNetIf->output = etharp_output;
	pNetIf->linkoutput = GETH_OS_link_output;


	/* MAC hardware address length */
	pNetIf->hwaddr_len = GETH_MAC_LEN;
	memcpy( (void*)pNetIf->hwaddr, network_def_config[0], GETH_MAC_LEN );

	/* Maximum data payload */
	pNetIf->mtu = ( GETH_TOTAL_LEN );

	/* Accept brodacast address and ARP traffic */
	pNetIf->flags = ((uint8_t)(NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP));

	/* Step 3: Initialize the PHY interface */
	GETH_Phy_init();

	/* Step 4: Start Transmit and Receive */
	GETH_Dev_start_tx( gethIdx );
	GETH_Dev_start_rx( gethIdx );

	/* Step 5: Check the link status */
	Ifx_GETH_MAC_PHYIF_CONTROL_STATUS linkStatus;

	/* The RMII interface supports the register MAC_PHYIF_CONTROL_STATUS */
	linkStatus.U = pGethConfig->pGeth->MAC_PHYIF_CONTROL_STATUS.U;
	if( linkStatus.B.LNKSTS == 1 )
	{
		PRINTF( "The Phy %d: Link is UP.\r\n", gethIdx );

		u8LinkUp[gethIdx] = 1;
		netif_set_link_up( pNetIf );

		if( linkStatus.B.LNKMOD == 1 )
		{
			PRINTF( "Set to Full duplexed\r\n" );
            IfxGeth_mac_setDuplexMode( pGethConfig->pGeth, IfxGeth_DuplexMode_fullDuplex );
		}
		else
		{
			PRINTF( "Set to Half duplexed\r\n" );
            IfxGeth_mac_setDuplexMode( pGethConfig->pGeth, IfxGeth_DuplexMode_halfDuplex );
		}

		if( linkStatus.B.LNKSPEED == 0 )
		{
			PRINTF( "Set to 10Mbps\r\n" );
            IfxGeth_mac_setLineSpeed( pGethConfig->pGeth, IfxGeth_LineSpeed_10Mbps);
		}
		else
		{
            if (linkStatus.B.LNKSPEED == 1)
            {
                // 100MBit speed
				PRINTF( "Set to 100Mbps\r\n" );
                IfxGeth_mac_setLineSpeed( pGethConfig->pGeth, IfxGeth_LineSpeed_100Mbps );
            }
            else
            {
                // 1000MBit speed
				PRINTF( "Set to 1000Mbps\r\n" );
                IfxGeth_mac_setLineSpeed( pGethConfig->pGeth, IfxGeth_LineSpeed_1000Mbps );
            }
		}
	}

	return(ERR_OK);
}/* END ethif_netIfInit() */


static void GETH_OS_link_timer(void* pvParam)
{
	(void)pvParam;
	Ifx_GETH_MAC_PHYIF_CONTROL_STATUS linkStatus;


	for( int idx = 0; idx < IFXGETH_NUM_MODULES; idx++ )
	{
		Ifx_GETH *pGeth;


		pGeth = IfxGeth_cfg_indexMap[idx].module;
		linkStatus.U = pGeth->MAC_PHYIF_CONTROL_STATUS.U;


		if( linkStatus.B.LNKSTS == 0u )
		{
			if( u8LinkUp[idx] != 0u )
			{
				u8LinkUp[idx] = 0u;
				PRINTF( "Network connection is DOWN\r\n" );
				netif_set_link_down( &xGethNetIf[idx] );
				netif_set_down( &xGethNetIf[idx] );	//2024-01-25, Ray, Do we need to bring down netif *?
			}
		}
		else
		{
			if( u8LinkUp[idx] == 1u )
			{
				continue;
			}

			u8LinkUp[idx] = 1u;
			PRINTF( "Network connection is UP\r\n", idx );

			if( linkStatus.B.LNKMOD == 1u )
			{
				PRINTF( "Set to Full duplexed\r\n" );
				if( pGeth->MAC_CONFIGURATION.B.DM != IfxGeth_DuplexMode_fullDuplex )
				{
                    IfxGeth_mac_setDuplexMode( pGeth, IfxGeth_DuplexMode_fullDuplex );
				}
			}
			else
			{
				PRINTF( "Set to Half duplexed\r\n" );
				if( pGeth->MAC_CONFIGURATION.B.DM != IfxGeth_DuplexMode_halfDuplex )
				{
					IfxGeth_mac_setDuplexMode( pGeth, IfxGeth_DuplexMode_halfDuplex );
				}
			}


			if( linkStatus.B.LNKSPEED == 0u )
			{
				PRINTF( "Set to 10Mbps\r\n" );
				if( (pGeth->MAC_CONFIGURATION.B.PS != 1u) && (pGeth->MAC_CONFIGURATION.B.FES != 0) )
				{
                    IfxGeth_mac_setLineSpeed( pGeth, IfxGeth_LineSpeed_10Mbps );
				}
			}
			else
			{
				if( linkStatus.B.LNKSPEED == 1u )
				{
					PRINTF( "Set to 100Mbps\r\n" );
                    if( (pGeth->MAC_CONFIGURATION.B.PS != 1u) && (pGeth->MAC_CONFIGURATION.B.FES != 1u) )
                    {
                        IfxGeth_mac_setLineSpeed( pGeth, IfxGeth_LineSpeed_100Mbps);
                    }
				}
				else
				{
					PRINTF( "Set to 1000Mbps\r\n" );
                    if( (pGeth->MAC_CONFIGURATION.B.PS != 0) && (pGeth->MAC_CONFIGURATION.B.FES != 0) )
                    {
                        IfxGeth_mac_setLineSpeed( pGeth, IfxGeth_LineSpeed_1000Mbps );
                    }
				}
			}

            // set the NETIF_UP flag that the interface is can be use for transmit
            xGethNetIf[idx].flags |= NETIF_FLAG_UP;
            netif_set_link_up( &xGethNetIf[idx] );
            netif_set_up( &xGethNetIf[idx] );


		}	/* LNKSTS == 1 */
	} /* For Loop */

	sys_timeout( LINK_TMR_INTERVAL, GETH_OS_link_timer, NULL );

}/* END GETH_OS_link_timer() */

void GETH_OS_get_link_info( char *p_info )
{
	Ifx_GETH *pGeth;
	int pos;
	Ifx_GETH_MAC_PHYIF_CONTROL_STATUS linkStatus;


	pos = 0;

	pGeth = IfxGeth_cfg_indexMap[0].module;
	linkStatus.U = pGeth->MAC_PHYIF_CONTROL_STATUS.U;
	if( linkStatus.B.LNKSTS == 0 )
	{
		sprintf( p_info + pos, "Link is down\r\n" );
		return;
	}

	pos +=sprintf( p_info + pos, "Link is up\r\nspeed and duplex: " );

	if( linkStatus.B.LNKSPEED == 0 )
	{
		pos += sprintf( p_info + pos, "10Mbps/");
	}
	else
	{
		if( linkStatus.B.LNKSPEED == 1 )
		{
			pos += sprintf( p_info + pos, "100Mbps/");
		}
		else
		{
			pos += sprintf( p_info + pos, "1000Mbps/");
		}
	}


	if( linkStatus.B.LNKMOD == 1 )
	{
		sprintf( p_info + pos, "FD");
	}
	else
	{
		sprintf( p_info + pos, "HD");
	}
}

void GETH_OS_notify_rx( uint8 gethIdx, uint8 chanIdx )
{
	uint16 intSrc;
	BaseType_t taskWoken = pdFALSE;

	intSrc = gethIdx << 8 | chanIdx;
	if( xRxQueueH != NULL_PTR )
	{
		if( xQueueSendToBackFromISR( xRxQueueH, &intSrc, &taskWoken ) != pdPASS )
		{
			xGethStatus[0].freertos_queue_error++;
		}
		portYIELD_FROM_ISR( taskWoken );
	}
}

void GETH_OS_lock( void )
{
	if( xTxMutexH != NULL_PTR )
	{
		xSemaphoreTake( xTxMutexH, portMAX_DELAY );
	}
	else
	{
		configASSERT( FALSE );
	}
}

void GETH_OS_unlock( void )
{
	if( xTxMutexH != NULL_PTR )
	{
		xSemaphoreGive( xTxMutexH );
	}
	else
	{
		configASSERT( FALSE );
	}
}


static uint8 ray_Debug_RX[30];
boolean GETH_OS_rx_process( uint8 gethIdx, uint8 *pFrame, uint16 u16FrameLen  )
{
	struct pbuf *pEthBuf;
	uint32 frameLen = u16FrameLen;

	memmove( ray_Debug_RX, pFrame, (u16FrameLen>30)? 30: u16FrameLen );

	if( 1 )
	{

		//GETH_Debug_print_ptp_packet( gethIdx, pFrame, u16FrameLen );

#if ETH_PAD_SIZE
		frameLen += ETH_PAD_SIZE;
#endif	/* ETH_PAD_SIZE */

		pEthBuf = pbuf_alloc(PBUF_RAW, (uint16_t)frameLen, PBUF_POOL);

		if( pEthBuf == NULL_PTR )
		{
			PRINTF( "The PBUF buffer allocation failed.\r\n" );
			configASSERT( FALSE );
		}


		for( struct pbuf* pIndex = pEthBuf; pIndex != NULL; pIndex = pIndex->next )
		{
			memcpy( pIndex->payload, pFrame, pIndex->len );
			pFrame = &pFrame[pIndex->len];
		}

#if ETH_PAD_SIZE
		pbuf_header( pEthBuf, ETH_PAD_SIZE );
#endif	/* ETH_PAD_SIZE */


		if( xGethNetIf[gethIdx].input( pEthBuf, &xGethNetIf[gethIdx] ) != ERR_OK )
		{/* LWIP rejected packet */
			pbuf_free( pEthBuf );
		}

	}
	else
	{
	}

	return TRUE;
}


static void GETH_OS_rx_task(void* vpParam)
{
	uint16_t u16Length;
	uint8* pDmaBuff;
	uint16 frameSrc;
	uint8 gethIdx;
	uint8 chanIdx;
	uint8 dirtyRx;
	uint16 originIdx;

	while( 1 )
	{
		if( xQueueReceive( xRxQueueH, &frameSrc, portMAX_DELAY ) == pdPASS )
		{
			gethIdx = (frameSrc>>8) & 0xFF;
			chanIdx = frameSrc & 0xFF;
			dirtyRx = 0u;

			if( gethIdx !=0 )
			{
				PRINTF( "******************** Received the wrong gethIdx from Receive interrupt, Receving stops.\r\n" );
				configASSERT( FALSE );
			}

			originIdx = GETH_Dev_get_rx_index( gethIdx, chanIdx );
			while( GETH_Dev_rx_packet_avail(gethIdx, chanIdx) )
			{
				//GETH_Debug_dump_rx_desc( gethIdx, chanIdx, 0u );

				pDmaBuff = GETH_Dev_recv_packet( gethIdx, chanIdx, &u16Length );

				if( pDmaBuff == NULL_PTR )
				{
					PRINTF( "Received a NULL PTR DMA buffer\r\n" );
					GETH_Dev_forward_rx_desc_index( gethIdx, chanIdx );
					dirtyRx++;
					continue;
				}

				dirtyRx++;

				GETH_OS_rx_process( gethIdx, pDmaBuff, u16Length );

				//GETH_Debug_print_upd_packet( gethIdx, pDmaBuff, u16Length );

				GETH_Dev_forward_rx_desc_index( gethIdx, chanIdx );
			};

			GETH_Dev_release_rx_desc( gethIdx, chanIdx, originIdx, dirtyRx );
			GETH_Dev_wakeup_rx( gethIdx, chanIdx );

		}
	}

}/* END ethif_RxdPipeTask() */

static boolean GETH_OS_dhcp_enabled( void )
{
	return (network_def_config[1][4] == 1);
}


void GETH_OS_start_rx_task(void)
{

	xRxQueueH = xQueueCreate( 100, sizeof(uint16) );
	configASSERT( xRxQueueH );

    xTaskCreate( GETH_OS_rx_task, ((const char* const)"RxdPipe"), ETHIF_RXPIPE_TASK_STACK_SIZE, NULL_PTR, ETHIF_RXPIPE_PRIORITY, NULL);

}/* END ethif_StartRxPipeTask() */




sint32 GETH_OS_init_LWIP( boolean bEnable )
{
	sint32 i32Rtn = -1;


	if( (bLWIPEnabled == FALSE) && (bEnable == TRUE) )
	{
		ip_addr_t ipAddr;
		ip_addr_t netMask;
		ip_addr_t gateWay;


		xTxMutexH = xSemaphoreCreateMutexStatic( &xTxMutexBuffer );

		GETH_OS_start_rx_task();

		bLWIPEnabled = TRUE;
		tcpip_init( NULL, NULL );


		for( int gethIdx = 0; gethIdx < IFXGETH_NUM_MODULES; gethIdx++ )
		{
#if 0
			IP4_ADDR( &ipAddr, IP_ADDRESS_GETH[gethIdx][0], IP_ADDRESS_GETH[gethIdx][1], IP_ADDRESS_GETH[gethIdx][2], IP_ADDRESS_GETH[gethIdx][3] );
			IP4_ADDR( &netMask, NET_MASK[0], NET_MASK[1], NET_MASK[2], NET_MASK[3] );
			IP4_ADDR( &gateWay, GATE_WAY[0], GATE_WAY[1], GATE_WAY[2], GATE_WAY[3] );
#else
			if( GETH_OS_dhcp_enabled() == TRUE )
			{
				PRINTF( "DHCP is enabled\r\n" );
				//Here we don't use the DHCP, if the DHCP is enabled, we use the default IP address
				IP4_ADDR( &ipAddr, 192, 168, 1, 200 );
				IP4_ADDR( &netMask, 255, 255, 255, 0x00 );
				IP4_ADDR( &gateWay, 192, 168, 1, 1 );
			}
			else
			{
				IP4_ADDR( &ipAddr, network_def_config[1][0], network_def_config[1][1], network_def_config[1][2], network_def_config[1][3] );
				IP4_ADDR( &netMask, network_def_config[2][0], network_def_config[2][1], network_def_config[2][2], network_def_config[2][3] );
				IP4_ADDR( &gateWay, network_def_config[3][0], network_def_config[3][1], network_def_config[3][2], network_def_config[3][3] );
			}

#endif
			if( netif_add(&xGethNetIf[gethIdx], &ipAddr, &netMask, &gateWay, &xGethConfig[gethIdx], &GETH_OS_net_init, tcpip_input) == NULL )
			{
				PRINTF( "*************************The xGeth0NetIf init Failed.\r\n\r\n" );
				return( i32Rtn );
			}
			netif_set_default( &xGethNetIf[gethIdx] );
			netif_set_up( &xGethNetIf[gethIdx] );

			if( GETH_OS_dhcp_enabled() == TRUE )
			{
				dhcp_start( &xGethNetIf[gethIdx] );
			}
			else
			{
				PRINTF( "DHCP is disabled, using the statically configured IP address\r\n" );
			}

		}

		IfxCpu_enableInterrupts();
		sys_timeout( LINK_TMR_INTERVAL, GETH_OS_link_timer, NULL );

	}

	return(i32Rtn);
}/* END ethif_InitLwip() */




boolean ethif_LwipGetNetIfStatus(void)
{
	boolean bRtn = FALSE;


	if(netif_is_up(&xGethNetIf[0]) == 1u)
	{
		bRtn = TRUE;
	}

	return(bRtn);
}/* END ethif_LwipGetNetIfStatus() */


void start_network( void )
{
	GETH_OS_init_LWIP( TRUE );
}
