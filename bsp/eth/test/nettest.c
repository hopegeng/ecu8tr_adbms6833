/*
 * test.c
 *
 *  Created on: Jul 19, 2021
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

#include "shell.h"


#define ku32UDP_TAKS_STACK_SIZE					128 /* 128*4 = 512 bytes */
#define ku32UDP_TASK_PRIORITY					2u
#define UDP_SERVER_PORT							8888

#define QUEUE_LENGTH							10u
#define ITEM_SIZE								sizeof(uint8_t)

static  __far struct udp_pcb *xpRxPCB0;
static  __far struct udp_pcb *xpRxPCB1;

static struct udp_pcb *xpEth0Pcb;
static struct udp_pcb *xpEth1Pcb;
static struct udp_pcb *xpTagPcb;

static  uint8_t pu8IntQeueStorageBuffer[QUEUE_LENGTH];
static  StaticQueue_t xIntQueueBuffer;


static void nettest_udpTask( void* pvParam );
static void nettest_udpRecvCB( void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port );
static void nettest_udpRecvInit( void );
static void nettest_udpSendTask( void );
static void nettest_udpSendInit( void );


void test_udpInit( void )
{
    xTaskCreate( nettest_udpTask, "UDPTest", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
}


static void nettest_udpRecvInit( void )
{
	ip_addr_t ip;


	IP4_ADDR( &ip, 192, 168, 1, 170 );
	xpRxPCB0 = udp_new();
	udp_bind( xpRxPCB0, &ip, UDP_SERVER_PORT+1 );
	udp_recv( xpRxPCB0, nettest_udpRecvCB, NULL );

	IP4_ADDR( &ip, 192, 168, 2, 10 );
	xpRxPCB1 = udp_new();
	udp_bind( xpRxPCB1, &ip, UDP_SERVER_PORT+1 );
	udp_recv( xpRxPCB1, nettest_udpRecvCB, NULL );

}

static void nettest_udpSendInit( void )
{
	ip_addr_t srcAddr;
	ip_addr_t dstAddr;


	IP4_ADDR( &srcAddr, 192, 168, 1, 170 );
	IP4_ADDR( &dstAddr, 192,168, 1, 3 );
	xpEth0Pcb = udp_new();
	udp_bind( xpEth0Pcb, &srcAddr, 8888 );
	udp_connect( xpEth0Pcb, &dstAddr, 8888 );

	IP4_ADDR( &srcAddr, 192, 168, 1, 170 );
	IP4_ADDR( &dstAddr, 0xFF, 0xFF, 0xFF, 0xFF );
	xpEth1Pcb = udp_new();
	udp_bind( xpEth1Pcb, &srcAddr, 8888 );
	udp_connect( xpEth1Pcb, &dstAddr, 8888 );


	IP4_ADDR( &srcAddr, 192, 168, 3, 10 );
	IP4_ADDR( &dstAddr, 192,168, 3, 125 );
	xpTagPcb = udp_new();
	udp_bind( xpTagPcb, &srcAddr, 8888 );
	udp_connect( xpTagPcb, &dstAddr, 8888 );
}

static void nettest_udpRecvCB( void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port )
{

	PRINTF( "Received UDP data = %s\r\n", p->payload );
	PRINTF( "ip = %d:%d:%d:%d\r\n", (pcb->local_ip.addr>>0)&0xFF, (pcb->local_ip.addr>>8)&0xFF, (pcb->local_ip.addr>>16)&0xFF, (pcb->local_ip.addr>>24)&0xFF);

	pbuf_free( p );

}

static void nettest_udpSendTask( void )
{
	struct pbuf  *pTagBuf;
	struct pbuf  *pEth0Buf;
	struct pbuf  *pEth1Buf;
	char data[20] = {0};
	uint16_t dataLen;
	static int idx = 0;


	sprintf( data, "%d: %s\n", idx++, "Hello, World" );
	dataLen = (uint16_t)strlen( (const char*)data );
	pTagBuf = pbuf_alloc( PBUF_TRANSPORT, dataLen, PBUF_RAM );
	pbuf_take( pTagBuf, data, dataLen );

	pEth0Buf = pbuf_alloc( PBUF_TRANSPORT, dataLen, PBUF_RAM );
	pbuf_take( pEth0Buf, data, dataLen );

	pEth1Buf = pbuf_alloc( PBUF_TRANSPORT, dataLen, PBUF_RAM );
	pbuf_take( pEth1Buf, data, dataLen );


#if 1
	udp_send( xpEth0Pcb, pEth0Buf );
	vTaskDelay( 500u );
	udp_send( xpEth1Pcb, pEth1Buf );
	vTaskDelay( 500u );
	udp_send( xpTagPcb, pTagBuf );
#endif


	//udp_send( xpEth0Pcb, pEth0Buf );


	pbuf_free( pTagBuf );
	pbuf_free( pEth0Buf );
	pbuf_free( pEth1Buf );

}

void my_ip_assignment(struct netif *netif)
{
    // Check if the netif is up and has an IP address assigned
    if (netif_is_up(netif) && !ip_addr_isany(&netif->ip_addr)) {
        // IP address has been assigned by DHCP
        char ip_addr_str[16];
        ipaddr_ntoa_r(&netif->ip_addr, ip_addr_str, 16);
        PRINTF("IP Address assigned by DHCP: %s\n\n", ip_addr_str);
    }
    else
    {
    	PRINTF( "No IP available\r\n" );
    }
}

extern struct netif xGethNetIf[];

static void nettest_udpTask( void *pvParam )
{

	vTaskDelay( 10000 );

#if defined(__RAY_DHCP_TEST__ )

	while( 1 )
	{
		//__RAY_DHCP_TEST__
		my_ip_assignment( &xGethNetIf[0] );
		vTaskDelay( 1000 );
	}
#endif

	nettest_udpRecvInit();



	nettest_udpSendInit();

	while( 1 )
	{
		nettest_udpSendTask();
		vTaskDelay( 3000 );
	}
}
