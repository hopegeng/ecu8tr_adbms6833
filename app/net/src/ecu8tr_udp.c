/*
 * ecu8_udp.c
 *
 *  Created on: Jan 26, 2024
 *      Author: rgeng
 */


/*----- Includes -------------------------------------------------------------*/
/* C includes */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
/* IFX includes */
#include <Ifx_Types.h>
#include <Cpu/Std/Platform_types.h>
/* FreeRTOS includes */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include <tcp.h>
#include <tcpip.h>
#include <udp.h>

#include "tools.h"
#include "bootloader.h"
#include "shell.h"
#include "ecu8tr_cmd.h"

void bootloader_set_upgradeStatus( ECU8TR_UPGRADE_STATUS_e status );

extern uint8_t network_def_config[5][8];


#define ECU8_UDP_SERVER_PORT  		8100

static struct udp_pcb *xpEth0Pcb;
static  __far struct udp_pcb *xpRxPCB0;

static void ecu8tr_ReplyAck( struct udp_pcb *pcb, const char *msg )
{
	struct pbuf  *pbuf;
	uint16_t msgLen;

	msgLen = (uint16_t)strlen( msg );

	pbuf = pbuf_alloc( PBUF_TRANSPORT, msgLen, PBUF_RAM );
	pbuf_take( pbuf, msg, msgLen );
	udp_send( pcb, pbuf );
	pbuf_free( pbuf );

}


static void do_bootloader( void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port  )
{
	uint8 frame[600];
	uint8 *pFrame;
	uint32 block_seq;
	static struct udp_pcb* new_pcb = NULL;

	if( new_pcb == NULL )
	{
		new_pcb = udp_new();
		udp_bind( new_pcb, IP_ADDR_ANY, ECU8_UDP_SERVER_PORT );
		udp_connect( new_pcb, addr, port );
	}

	pFrame = frame;
	for( struct pbuf *pNext = p; pNext != NULL_PTR; pNext = p->next )
	{
		memcpy( pFrame, pNext->payload, pNext->len );
		pFrame += pNext->len;
	}

	if( bootloader_doUpGrade( frame, p->tot_len, &block_seq ) == FALSE )
	{
		ecu8tr_ReplyAck( new_pcb, "Failed" );
		bootloader_set_upgradeStatus( UPGRADE_STATUS_FAILURE );
		PRINTF( "************************ Failed to do upgrade******************************\r\n" );
		while( 1 )
			;
	}
	else
	{
		ecu8tr_ReplyAck( new_pcb, "OK" );
		if( block_seq == 0xFFFFFFFF )
		{
			bootloader_set_upgradeStatus( UPGRADE_STATUS_SUCCESS );
			PRINTF( "Upgrade Successfully, system rebooting..." );
			vTaskDelay( pdMS_TO_TICKS(2000) );
			board_reset( eSysSystem );
		}
	}

	//PRINTF( "Received UDP data = %s\r\n", p->payload );
	//PRINTF( "ip = %d:%d:%d:%d\r\n", (pcb->local_ip.addr>>0)&0xFF, (pcb->local_ip.addr>>8)&0xFF, (pcb->local_ip.addr>>16)&0xFF, (pcb->local_ip.addr>>24)&0xFF  );

	pbuf_free( p );

}


static void ecu8_udpRecvInit( void )
{

	ip_addr_t ip;
	uint8_t* ip_hex;

	ip_hex = network_def_config[1];
	IP4_ADDR( &ip, ip_hex[0], ip_hex[1], ip_hex[2], ip_hex[3] );

	xpRxPCB0 = udp_new();
	udp_bind( xpRxPCB0, &ip, ECU8_UDP_SERVER_PORT );

	udp_recv( xpRxPCB0, do_bootloader, NULL );
}

static void ecu8_udpSendInit( void )
{
	ip_addr_t dstAddr;

	IP4_ADDR( &dstAddr, 192,168, 1, 2 );
	xpEth0Pcb = udp_new();
	udp_bind( xpEth0Pcb, IP_ADDR_ANY, ECU8_UDP_SERVER_PORT );
	udp_connect( xpEth0Pcb, &dstAddr, 8888 );
}

static void nettest_udpSendTask( void )
{
	struct pbuf  *pEth0Buf;
	char data[20] = {0};
	uint16_t dataLen;
	static int idx = 0;


	//sprintf( data, "%d: %s\n", idx++, "Hello, World from ETH0" );
	sprintf( data, "OK" );
	dataLen = (uint16_t)strlen( (const char*)data );

	pEth0Buf = pbuf_alloc( PBUF_TRANSPORT, dataLen, PBUF_RAM );
	pbuf_take( pEth0Buf, data, dataLen );

	udp_send( xpEth0Pcb, pEth0Buf );

	pbuf_free( pEth0Buf );

}


static void ecu8_udpServerTask( void *arg )
{
    vTaskDelay( pdMS_TO_TICKS(3000) ); // Yield to other tasks

	ecu8_udpSendInit();
	ecu8_udpRecvInit();

    while( 1 )
    {
        vTaskDelay( pdMS_TO_TICKS(1000) ); // Yield to other tasks
       //nettest_udpSendTask();
    }
}

void network_initUDPServer( void )
{
    xTaskCreate( ecu8_udpServerTask, "UDPServer", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
}



