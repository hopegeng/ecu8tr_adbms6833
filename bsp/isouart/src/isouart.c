/*
 * uart.c
 *
 *  Created on: Jan 30, 2024
 *      Author: rgeng
 */

#include <string.h>
#include <IfxAsclin_Asc.h>
#include <IfxAsclin_reg.h>
#include <IfxCpu_Irq.h>
#include <IfxPort_reg.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "shell.h"
#include "isouart.h"
#include "eeprom.h"
#include "board.h"
#include "tools.h"
#include "tle9012.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define UART_BAUDRATE           2000000u                                /* UART baud rate in bit/s                  */

#define UART_1_PIN_RX             IfxAsclin1_RXD_P14_8_IN                 /* UART receive port pin                    */
#define UART_1_PIN_TX             IfxAsclin1_TX_P14_10_OUT                /* UART transmit port pin                   */

#define UART_9_PIN_RX             IfxAsclin9_RXD_P14_9_IN                 /* UART receive port pin                    */
#define UART_9_PIN_TX             IfxAsclin9_TX_P14_7_OUT                /* UART transmit port pin                   */

/* Definition of the interrupt priorities */
#define INTPRIO_ASCLIN1_ERR		0x35
#define INTPRIO_ASCLIN1_RX      0x36
#define INTPRIO_ASCLIN1_TX      0x37

#define INTPRIO_ASCLIN9_ERR		0x45
#define INTPRIO_ASCLIN9_RX      0x46
#define INTPRIO_ASCLIN9_TX      0x47

#define UART_RX_BUFFER_SIZE     64                                      /* Definition of the receive buffer size    */
#define UART_TX_BUFFER_SIZE     64                                      /* Definition of the transmit buffer size   */
#define SIZE                    13                                      /* Size of the string                       */

#define	BAUD_STD_PRESCALER		(20u)
#define	BAUD_FAST_PRESCALER		(4u)
#define BAUD_DENOMINATOR		(3125u)
#define BAUD_OVERSAMPLING		(16u)
/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/


static uint8 isouart_buffer[2][32] = { {0}, {0} };
static uint8 isouart_rd_cnt[2] = {0};
static uint8 isouart_return_cnt[2] = {0};
static SemaphoreHandle_t isouart_semaphore[2] = { NULL };
static BaseType_t isouart_higherPriorityTaskWoken[2] = { 0 };
static ISOUART_NetArch_t isouart_network = ISOUART_LowSide;
static Ifx_ASCLIN* ISOUART_MODULE[3] = { &MODULE_ASCLIN1, &MODULE_ASCLIN9, NULL };
static uint32  ISOUART_INT_RX[3] = { INTPRIO_ASCLIN1_RX, INTPRIO_ASCLIN9_RX, 0x00 };
static uint32  ISOUART_INT_ERR[3] = { INTPRIO_ASCLIN1_ERR, INTPRIO_ASCLIN9_ERR, 0x00 };
static volatile Ifx_SRC_SRCR* ISOUART_SRC_RX[3] = { &SRC_ASCLIN_ASCLIN1_RX, &SRC_ASCLIN_ASCLIN9_RX, NULL };
static volatile Ifx_SRC_SRCR* ISOUART_SRC_ERR[3] = { &SRC_ASCLIN_ASCLIN1_ERR, &SRC_ASCLIN_ASCLIN9_ERR, NULL };
static boolean ISOUART_IS_RING = FALSE;


/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/* Adding of the interrupt service routines */


IFX_INTERRUPT( asc1_rx_isr, 0, INTPRIO_ASCLIN1_RX );
void asc1_rx_isr( void )
{
	Ifx_ASCLIN* module = ISOUART_MODULE[ISOUART_LowSide];
	uint8 u8Data;

	if( module->FLAGS.B.RFL == 1 )
	{
		uint8 fifoLen;
		/* Clear the Receive FIFO Level flag */
		module->FLAGSCLEAR.B.RFLC = 1u;

		fifoLen = (uint8)module->RXFIFOCON.B.FILL;

		for( int idx = 0; idx < fifoLen; idx++ )
		{
			u8Data = (uint8)module->RXDATA.U;
			isouart_buffer[ISOUART_LowSide][isouart_rd_cnt[ISOUART_LowSide]] = u8Data;
			isouart_rd_cnt[ISOUART_LowSide]++;
			if( (isouart_rd_cnt[ISOUART_LowSide] == isouart_return_cnt[ISOUART_LowSide]) && (isouart_network == ISOUART_LowSide) )
			{
		        xSemaphoreGiveFromISR( isouart_semaphore[ISOUART_LowSide], &isouart_higherPriorityTaskWoken[ISOUART_LowSide] );
		        portYIELD_FROM_ISR( isouart_semaphore[ISOUART_LowSide] );
			}
		}
	}
}

IFX_INTERRUPT( asc9_rx_isr, 0, INTPRIO_ASCLIN9_RX );
void asc9_rx_isr(void)
{
	Ifx_ASCLIN* module = ISOUART_MODULE[ISOUART_HighSide];
	uint8 u8Data;

	if( module->FLAGS.B.RFL == 1 )
	{
		uint8 fifoLen;
		/* Clear the Receive FIFO Level flag */
		module->FLAGSCLEAR.B.RFLC = 1u;

		fifoLen = (uint8)module->RXFIFOCON.B.FILL;

		for( int idx = 0; idx < fifoLen; idx++ )
		{
			u8Data = (uint8)module->RXDATA.U;
			isouart_buffer[ISOUART_HighSide][isouart_rd_cnt[ISOUART_HighSide]] = u8Data;
			isouart_rd_cnt[ISOUART_HighSide]++;
			if( (isouart_rd_cnt[ISOUART_HighSide] == isouart_return_cnt[ISOUART_HighSide]) && (isouart_network == ISOUART_HighSide))
			{
		        xSemaphoreGiveFromISR( isouart_semaphore[ISOUART_HighSide], &isouart_higherPriorityTaskWoken[ISOUART_HighSide] );
		        portYIELD_FROM_ISR( isouart_semaphore[ISOUART_HighSide] );
			}
		}
	}
}


static void isouart_pinSet( ISOUART_NetArch_t network )
{
	Ifx_ASCLIN *pModule_ASCLIN;

	pModule_ASCLIN = ISOUART_MODULE[network];

	if( network == ISOUART_LowSide )
	{
		/* TxD = P14.10; RxD = P14.8 */
		/* Route the I/O, we are going to drive the TxD strong as there are level shifting FETs in the signal path */
		pModule_ASCLIN->IOCR.B.ALTI = 0x03u;
		IfxPort_setPinModeOutput( &MODULE_P14, ((uint8)0x0Au), IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_alt4 );
		IfxPort_setPinPadDriver( &MODULE_P14, ((uint8)0x0Au), IfxPort_PadDriver_cmosAutomotiveSpeed1 );
	}
	else if( network == ISOUART_HighSide )
	{
		/* TxD = P14.7; RxD = P14.9 */
		/* Route the I/O, we are going to drive the TxD strong as there are level shifting FETs in the signal path */
		pModule_ASCLIN->IOCR.B.ALTI = 0x03u;
		IfxPort_setPinModeOutput( &MODULE_P14, ((uint8)0x07u), IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_alt4 );
		IfxPort_setPinPadDriver( &MODULE_P14, ((uint8)0x07u), IfxPort_PadDriver_cmosAutomotiveSpeed1 );
	}
}

static void asclinuart_init( ISOUART_NetArch_t network )
{
	uint32 u32Prescaler;
	boolean bInterruptState;
	Ifx_ASCLIN *pModule_ASCLIN;
	Ifx_SRC_SRCR isr;
	uint16 u16Psw;

	u32Prescaler = BAUD_FAST_PRESCALER;

	pModule_ASCLIN = ISOUART_MODULE[network];

	bInterruptState = IfxCpu_disableInterrupts();



	u16Psw = IfxScuWdt_getCpuWatchdogPassword();
	IfxScuWdt_clearCpuEndinit(u16Psw);  	/* clears the endinit protection*/
	pModule_ASCLIN->CLC.U = 0u;		 	/* enables the module*/
	(void)pModule_ASCLIN->CLC.U;			/* read back for activating module */
	IfxScuWdt_setCpuEndinit(u16Psw);		/* sets the endinit protection back on*/


	u16Psw = IfxScuWdt_getCpuWatchdogPassword();
		IfxScuWdt_clearCpuEndinit(u16Psw);  		/* clears the endinit protection*/

		pModule_ASCLIN->KRST1.B.RST = 1;
		pModule_ASCLIN->KRST0.B.RST = 1;
		while( pModule_ASCLIN->KRST0.B.RSTSTAT == 0 )
			;

		pModule_ASCLIN->KRSTCLR.B.CLR = 1u;

		IfxScuWdt_setCpuEndinit(u16Psw);			/* sets the endinit protection back on*/


	/* No clock source (some of the ASCLIN registers can not be accessed if the source clock is set) */
	(pModule_ASCLIN)->CSR.U = 0u;

	(pModule_ASCLIN)->FRAMECON.U = 0u;		/* Ensure MODE = 0, Initialization */

	isouart_pinSet( network );

	/* Configure TxD FIFO */
	/* Flush FIFO, Enabled, 1-Byte inlet, interrupt generate when TxD FIFO empty */
	pModule_ASCLIN->TXFIFOCON.U = 0x00000043u;
	/* Configure RxD FIFO */
	/* Flush FIFO, Enabled, 1-Byte outlet, interrupt generate when RxD FIFO empty */
	pModule_ASCLIN->RXFIFOCON.U = 0x00000043u;
	/* Bit configuration */
	/* SM: 3/bit, SP: 7,8,9, OS: 16, PS: 20 */
	//config.pIfxModule->BITCON.U = 0x890F0013u;
	pModule_ASCLIN->BITCON.B.SM = IfxAsclin_SamplesPerBit_three;
	pModule_ASCLIN->BITCON.B.SAMPLEPOINT = IfxAsclin_SamplePointPosition_9;
	pModule_ASCLIN->BITCON.B.OVERSAMPLING = BAUD_OVERSAMPLING - 1;
	pModule_ASCLIN->BITCON.B.PRESCALER = u32Prescaler - 1;
	pModule_ASCLIN->FRAMECON.B.STOP = 1u;	/* Set the number of stop bits */


	/* Not setting parirty */
	pModule_ASCLIN->DATCON.U = 7u;

	/* Set Baud Rate Generation Register */
	pModule_ASCLIN->BRG.U = 0x07D00C35;

	//RX interrupt
	isr.U = 0u;
	isr.B.SRPN = ISOUART_INT_RX[network];		/* Service Request Priority Number */
	isr.B.SRE = 1u;										/* Enable Service Request */
	isr.B.TOS = 0;			/* Service Request Assigned to CPU */
	ISOUART_SRC_RX[network]->U = isr.U;

#if 0
	//TX interrupt
	isr.U = 0u;
	isr.B.SRPN = 0;										/* No need to enable the tx interrupt */
	isr.B.SRE = 0u;										/* Enable Service Request */
	isr.B.TOS = 0;			/* Service Request Assigned to CPU */
	((Ifx_SRC_SRCR*)(&SRC_ASCLIN_ASCLIN1_TX))->U = isr.U;
#endif

	//ASCLIN1 Error interrupt
	isr.U = 0u;
	isr.B.SRPN = ISOUART_INT_ERR[network];		/* Service Request Priority Number */
	isr.B.SRE = 1u;										/* Enable Service Request */
	isr.B.TOS = 0;			/* Service Request Assigned to CPU */
	ISOUART_SRC_ERR[network]->U = isr.U;

	/* Clear ALL Flags */
	pModule_ASCLIN->FLAGSCLEAR.U = 0xFFFFFFFFu;
	/* Disable all interrupts */
	pModule_ASCLIN->FLAGSENABLE.U = 0u;

	/* Set to ASC mode (UART) */
	pModule_ASCLIN->FRAMECON.B.MSB = 1;
	pModule_ASCLIN->FRAMECON.B.MODE = 1u;

	/* Set ASCLIN fast clock (200MHz)
	 * CCUCON0.CLKSEL = 1 (fPPL2 is used as clock source for fSOURCE2)
	 * CSR.CLKSEL = 2 --> fASCLINF = fSOURCE2 */
	pModule_ASCLIN->CSR.U = IfxAsclin_ClockSource_ascFastClock;

	IfxCpu_restoreInterrupts( bInterruptState );

	pModule_ASCLIN->FLAGSENABLE.B.RFLE = 1u;



}

boolean isouart_setNetwork( ISOUART_NetArch_t network )
{
	if( network < ISOUART_NET_NONE )
	{
		if( isouart_network == network )
		{
			PRINTF( "The commnad is ignored, because the ISOUART has already configured to %d\r\n", isouart_network );
			return FALSE;
		}
		tle9012_sleep();
		vTaskDelay( 2 );

		eeprom_set_isouartNetwork( network );
		isouart_network = network;
		PRINTF( "ISO UART switched to network = %d\r\n", isouart_network );

		return TRUE;
	}

	return FALSE;
}

boolean isouart_getRing( void )
{
	return ISOUART_IS_RING;
}

boolean isouart_isRing( void )
{
	ISOUART_IS_RING = FALSE;

	if( isouart_rd_cnt[ISOUART_HighSide] == isouart_rd_cnt[ISOUART_LowSide] )
	{
		if( memcmp( (const void*)isouart_buffer[ISOUART_HighSide], (const void*)isouart_buffer[ISOUART_LowSide], isouart_rd_cnt[ISOUART_LowSide] ) == 0 )
		{
			ISOUART_IS_RING = TRUE;
		}
	}

	return (ISOUART_IS_RING);

}

void isouart_getResult( uint8 *pBuffer, uint8 *pLen )
{
    xSemaphoreTake( isouart_semaphore[isouart_network], pdMS_TO_TICKS(5) );
	*pLen = isouart_rd_cnt[isouart_network];
	memcpy( pBuffer, isouart_buffer[isouart_network], *pLen );
}

void isouart_write( uint8* pMsg, uint16 count )
{
	Ifx_ASCLIN *pModule_ASCLIN;

	pModule_ASCLIN = ISOUART_MODULE[isouart_network];

	isouart_rd_cnt[ISOUART_HighSide] = isouart_rd_cnt[ISOUART_LowSide] = 0x0u;
	isouart_return_cnt[0] = isouart_return_cnt[1] = (count==4)? 9 : 7;		//Read return 9, write return 6;

	for( int idx = 0; idx < count; idx++ )
	{
		while( pModule_ASCLIN->TXFIFOCON.B.FILL == 16 )
		{
			__nop();
		}

		pModule_ASCLIN->TXDATA.U = pMsg[idx];
	}

	//tle9012uart1Conf.pIfxModule->TXFIFOCON.B.ENO = (0u);

	return;

}/* END tle9012uart1_Write() */

ISOUART_NetArch_t isouart_get_network( void )
{
	return isouart_network;
}

void isouart_apply_network( ISOUART_NetArch_t network )
{
	if( network >= ISOUART_NET_NONE )
	{
		PRINTF( "isouart_apply_network = %d\r\n", network );
		network = ISOUART_LowSide;
	}

	isouart_network = network;

}

void isouart_init( void )
{
	PRINTF( "Initializing the isouart network on %s\r\n", (isouart_network)? "HighSide" : "LowSide" );

	isouart_semaphore[0] = xSemaphoreCreateBinary();
	isouart_semaphore[1] = xSemaphoreCreateBinary();

	IfxPort_setPinLow( &MODULE_P32, 5 );	//Hold the tle9015 in reset mode, but it may not be necessary, more tests are needed

	delayMillSecond( 10 );

	asclinuart_init( ISOUART_LowSide );
	asclinuart_init( ISOUART_HighSide );

	delayMillSecond( 10 );

	IfxPort_setPinHigh( &MODULE_P32, 5 );
}

/* isouart_deinit is not used anymore */
void isouart_deinit( void )
{

	Ifx_ASCLIN *pModule_ASCLIN;
	Ifx_SRC_SRCR isr;
	uint16 u16Psw;


	pModule_ASCLIN = ISOUART_MODULE[isouart_network];

	u16Psw = IfxScuWdt_getCpuWatchdogPassword();
	IfxScuWdt_clearCpuEndinit(u16Psw);  		/* clears the endinit protection*/

	pModule_ASCLIN->KRST1.B.RST = 1;
	pModule_ASCLIN->KRST0.B.RST = 1;
	while( pModule_ASCLIN->KRST0.B.RSTSTAT == 0 )
		;

	pModule_ASCLIN->KRSTCLR.B.CLR = 1u;

	IfxScuWdt_setCpuEndinit(u16Psw);			/* sets the endinit protection back on*/


	pModule_ASCLIN->CSR.U = 0u;
	pModule_ASCLIN->FLAGSENABLE.U = 0u;
	pModule_ASCLIN->BRG.U = 0u;
	pModule_ASCLIN->DATCON.U = 0u;
	pModule_ASCLIN->FLAGSCLEAR.U = 0xFFFFFFFF;
	pModule_ASCLIN->FLAGSENABLE.B.RFLE = 0u;

	isr.U = 0u;

	ISOUART_SRC_RX[isouart_network]->U = isr.U;
	ISOUART_SRC_ERR[isouart_network]->U = isr.U;

	/* Finally disable the module */
	u16Psw = IfxScuWdt_getCpuWatchdogPassword();
	IfxScuWdt_clearCpuEndinit(u16Psw);  		/* clears the endinit protection*/
	pModule_ASCLIN->CLC.U = 1u;		 		/* enables the module*/
	(void)pModule_ASCLIN->CLC.U;				/* read back for deactivating module */
	IfxScuWdt_setCpuEndinit(u16Psw);			/* sets the endinit protection back on*/
}
