/*
 * can_test.c
 *
 *  Created on: Jan. 31, 2023
 *      Author: rgeng
 */



/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "can_drv.h"

#include <Ifx_Types.h>
#include <IfxPort.h>
#include <IfxPort_reg.h>
#include <freeRTOS.h>
#include <queue.h>
#include <IfxCan_Can.h>
#include <IfxCan.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "tools.h"
#include "can_if.h"

/*----- Constant members -----------------------------------------------------*/

#define ISR_PRIORITY_CAN_TX         31                       /* Define the CAN TX interrupt priority                 */
#define ISR_PRIORITY_CAN_RX         30                      /* Define the CAN RX interrupt priority                 */

#define MODULE_CAN0_RAM    (uint32)&MODULE_CAN0
#define NODE0_RAM_OFFSET   0x0
#define NODE1_RAM_OFFSET   0x1000


/*********************************************************************************************************************/
/*---------------------------------------------Function Declarations----------------------------------------------*/
/*********************************************************************************************************************/
uint32 can_doCanSend(void* lpParam);


/**< \brief DOCAN transmit (send) function pointer */


/*********************************************************************************************************************/
/*-------------------------------------------------External variables--------------------------------------------------*/
/*********************************************************************************************************************/

extern xQueueHandle xDoCan_queue;

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
McmcanType                  g_mcmcan;                       /* Global MCMCAN configuration and control structure    */
IfxPort_Pin_Config          g_led1;                         /* Global LED1 configuration and control structure      */
IfxPort_Pin_Config          g_led2;                         /* Global LED2 configuration and control structure      */

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/* Macro to define Interrupt Service Routine.
 * This macro:
 * - defines linker section as .intvec_tc<vector number>_<interrupt priority>.
 * - defines compiler specific attribute for the interrupt functions.
 * - defines the Interrupt service routine as ISR function.
 *
 * IFX_INTERRUPT(isr, vectabNum, priority)
 *  - isr: Name of the ISR function.
 *  - vectabNum: Vector table number.
 *  - priority: Interrupt priority. Refer Usage of Interrupt Macro for more details.
 */

uint32 rxBufferID;
uint32 rxMsgID;
volatile boolean cell_data_received = FALSE;
volatile uint32 can_rx_data[2];

volatile boolean is_tx_done = FALSE;

extern SemaphoreHandle_t canTxSemaphore;

IFX_INTERRUPT(canIsrTxHandler, 0, ISR_PRIORITY_CAN_TX);
IFX_INTERRUPT(canIsrRxHandler, 0, ISR_PRIORITY_CAN_RX);

/* Interrupt Service Routine (ISR) called once the TX interrupt has been generated.
 *
 */
void canIsrTxHandler(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* Clear the "Transmission Completed" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_transmissionCompleted);
    is_tx_done = TRUE;
	// Release the semaphore to unblock the task
    if( canTxSemaphore != NULL )
    {
    	xSemaphoreGiveFromISR(canTxSemaphore, &xHigherPriorityTaskWoken);
    	// Yield if a higher-priority task was woken
    	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* Interrupt Service Routine (ISR) called once the RX interrupt has been generated.
 * Compares the content of the received CAN message with the content of the transmitted CAN message
 * and in case of success, turns on the LED2 to indicate successful CAN message reception.
 */
void canIsrRxHandler(void)
{
	CanPkt_t canPkt;
    CanIf_MsgType canIfMsg;
    Ifx_CAN_RXMSG *rxBufferElement;
    boolean isExtendedFrame;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    /* Clear the "Message stored to Dedicated RX Buffer" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_messageStoredToDedicatedRxBuffer);

    /* Read the received CAN message */
    IfxCan_Can_initMessage(&g_mcmcan.rxMsg);
    rxBufferElement = IfxCan_Node_getRxBufferElementAddress(g_mcmcan.canSrcNode.node,
                                                            g_mcmcan.canSrcNode.messageRAM.baseAddress,
                                                            g_mcmcan.canSrcNode.messageRAM.rxBuffersStartAddress,
                                                            g_mcmcan.rxMsg.bufferNumber);
    isExtendedFrame = (rxBufferElement->R0.B.XTD != 0u) ? TRUE : FALSE;
    IfxCan_Can_readMessage(&g_mcmcan.canSrcNode, &g_mcmcan.rxMsg, g_mcmcan.rxData);
    g_mcmcan.rxMsg.messageIdLength = (isExtendedFrame == TRUE) ?
        IfxCan_MessageIdLength_extended : IfxCan_MessageIdLength_standard;

    rxMsgID =  g_mcmcan.rxMsg.messageId;
    rxBufferID = rxMsgID - 0x700;

    /* Check if the received data matches with the transmitted one */


    can_rx_data[0] = g_mcmcan.rxData[0];
    can_rx_data[1] = g_mcmcan.rxData[1];


	cell_data_received = TRUE;

	//IfxPort_togglePin( &MODULE_P20, 8 );

	canPkt.msg.messageId = rxMsgID;
	canPkt.data = g_mcmcan.rxData;

    canIfMsg.id = (rxMsgID & 0x1FFFFFFFUL);
    canIfMsg.dlc = 8u;
    canIfMsg.is_extended = (isExtendedFrame == TRUE) ? true : false;
    canIfMsg.data[0] = (uint8_t)(g_mcmcan.rxData[0] & 0xFFu);
    canIfMsg.data[1] = (uint8_t)((g_mcmcan.rxData[0] >> 8u) & 0xFFu);
    canIfMsg.data[2] = (uint8_t)((g_mcmcan.rxData[0] >> 16u) & 0xFFu);
    canIfMsg.data[3] = (uint8_t)((g_mcmcan.rxData[0] >> 24u) & 0xFFu);
    canIfMsg.data[4] = (uint8_t)(g_mcmcan.rxData[1] & 0xFFu);
    canIfMsg.data[5] = (uint8_t)((g_mcmcan.rxData[1] >> 8u) & 0xFFu);
    canIfMsg.data[6] = (uint8_t)((g_mcmcan.rxData[1] >> 16u) & 0xFFu);
    canIfMsg.data[7] = (uint8_t)((g_mcmcan.rxData[1] >> 24u) & 0xFFu);

    CanIf_RxIndication(&canIfMsg);

	if( (rxMsgID & 0x1FFFFFFF) == 0x18DA0B0A )
	{

		////2024-01-29: xQueueSendToBackFromISR( xDoCan_queue, &canPkt, &xHigherPriorityTaskWoken );

		/* Now the buffer is empty we can switch context if necessary. */
		if( xHigherPriorityTaskWoken )
		{
			/* Actual macro used here is port specific. */
			//taskYIELD();
		}
	}

}

IFX_CONST IfxCan_Can_Pins Can01_pins =
{
		&IfxCan_TXD01_P14_0_OUT, IfxPort_OutputMode_pushPull,
		&IfxCan_RXD01B_P14_1_IN, IfxPort_InputMode_pullUp,
		IfxPort_PadDriver_cmosAutomotiveSpeed3
};


void CanDrv_initCan(void)
{

#if 0
    IfxPort_setPinModeOutput( &MODULE_P34, 2,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow( &MODULE_P34, 2 );

    IfxPort_setPinModeOutput( &MODULE_P34, 3,  IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow( &MODULE_P34, 3 );
#endif


    /* ==========================================================================================
     * CAN module configuration and initialization:
     * ==========================================================================================
     *  - load default CAN module configuration into configuration structure
     *  - initialize CAN module with the default configuration
     * ==========================================================================================
     */
	 IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);

	IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);

	/* ==========================================================================================
	 * Source CAN node configuration and initialization:
	 * ==========================================================================================
	 *  - load default CAN node configuration into configuration structure
	 *
	 *  - set source CAN node in the "Loop-Back" mode (no external pins are used)
	 *  - assign source CAN node to CAN node 0
	 *
	 *  - define the frame to be the transmitting one
	 *
	 *  - once the transmission is completed, raise the interrupt
	 *  - define the transmission complete interrupt priority
	 *  - assign the interrupt line 0 to the transmission complete interrupt
	 *  - transmission complete interrupt service routine should be serviced by the CPU0
	 *
	 *  - initialize the source CAN node with the modified configuration
	 * ==========================================================================================
	 */
	IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);
	g_mcmcan.canNodeConfig.pins = &Can01_pins;

	g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;
	g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_1;


	g_mcmcan.canNodeConfig.clockSource = IfxCan_ClockSource_both;
	g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
	g_mcmcan.canNodeConfig.frame.mode = IfxCan_FrameMode_standard;

	g_mcmcan.canNodeConfig.txConfig.txMode = IfxCan_TxMode_dedicatedBuffers;
	g_mcmcan.canNodeConfig.txConfig.dedicatedTxBuffersNumber = 30;
	g_mcmcan.canNodeConfig.txConfig.txBufferDataFieldSize = IfxCan_DataFieldSize_8;

	g_mcmcan.canNodeConfig.rxConfig.rxMode = IfxCan_RxMode_dedicatedBuffers;
	g_mcmcan.canNodeConfig.rxConfig.rxBufferDataFieldSize = IfxCan_DataFieldSize_8;


	//Transmit interrupt
	g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
	g_mcmcan.canNodeConfig.interruptConfig.traco.priority = ISR_PRIORITY_CAN_TX;
	g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine = IfxCan_InterruptLine_0;
	g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService = IfxSrc_Tos_cpu0;

	//receive interrupt
	g_mcmcan.canNodeConfig.interruptConfig.messageStoredToDedicatedRxBufferEnabled = TRUE;
	g_mcmcan.canNodeConfig.interruptConfig.reint.priority = ISR_PRIORITY_CAN_RX;
	g_mcmcan.canNodeConfig.interruptConfig.reint.interruptLine = IfxCan_InterruptLine_1;
	g_mcmcan.canNodeConfig.interruptConfig.reint.typeOfService = IfxSrc_Tos_cpu0;

	g_mcmcan.canNodeConfig.filterConfig.standardListSize = 11;
	g_mcmcan.canNodeConfig.filterConfig.messageIdLength = IfxCan_MessageIdLength_both; //IfxCan_MessageIdLength_standard;
	g_mcmcan.canNodeConfig.filterConfig.extendedListSize = 10;

	//g_mcmcan.canNodeConfig.messageRAM.standardFilterListStartAddress = 0x0;
	//g_mcmcan.canNodeConfig.messageRAM.rxBuffersStartAddress          = 0x100;


	IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);

	/* ==========================================================================================
		* CAN filter configuration and initialization:
		* ==========================================================================================
		*  - filter configuration is stored under the filter element number 0
		*  - store received frame in a dedicated RX Buffer
		*  - define the same message ID as defined for the TX message
		*  - assign the filter to the dedicated RX Buffer (RxBuffer0 in this case)
		*
		*  - initialize the standard filter with the modified configuration
		* ==========================================================================================
		*/



	g_mcmcan.canFilter.number = 0;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x700;
	g_mcmcan.canFilter.id2 = 0x00;
	g_mcmcan.canFilter.type = IfxCan_FilterType_classic;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 1;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x701;
	g_mcmcan.canFilter.id2 = 0x00;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	g_mcmcan.canFilter.type = IfxCan_FilterType_classic;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 2;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x702;
	g_mcmcan.canFilter.id2 = 0x00;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	g_mcmcan.canFilter.type = IfxCan_FilterType_classic;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 3;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x703;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 4;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x704;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 5;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x705;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 6;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x706;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 7;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x707;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 8;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x708;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 9;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x709;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);


	g_mcmcan.canFilter.number = 10;
	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.id1 = 0x70a;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;
	IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
	g_mcmcan.canFilter.type = IfxCan_FilterType_classic;
	g_mcmcan.canFilter.id2 = 0x00;
	g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;

	g_mcmcan.canFilter.number = 0;
	g_mcmcan.canFilter.id1 = 0x18DA0B0A;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 1;
	g_mcmcan.canFilter.id1 = 0x0A181400;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 2;
	g_mcmcan.canFilter.id1 = 0x10087801;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 3;
	g_mcmcan.canFilter.id1 = 0x10088801;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 4;
	g_mcmcan.canFilter.id1 = 0x1808D801;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 5;
	g_mcmcan.canFilter.id1 = 0x12182401;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 6;
	g_mcmcan.canFilter.id1 = 0x1E05D401;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 7;
	g_mcmcan.canFilter.id1 = 0x1E093901;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 8;
	g_mcmcan.canFilter.id1 = 0x1E093801;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	g_mcmcan.canFilter.number = 9;
	g_mcmcan.canFilter.id1 = 0x12013401;
	IfxCan_Can_setExtendedFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);

	while (IfxCan_Can_isNodeSynchronized(&g_mcmcan.canSrcNode) != TRUE)
		;

}



/* Function to initialize both TX and RX messages with the default data values.
 * After initialization of the messages, the TX message is transmitted.
 */
//FixMe: This function should be protected by a mutex since multiple task calls this function
IfxCan_Status can_transmitCanMessage( uint32 msgId, uint32 low, uint32 high )
{
    /* Initialization of the RX message with the default configuration */
	//IfxCan_Can_initMessage(&g_mcmcan.rxMsg);

    /* Invalidation of the RX message data content */
    //memset((void *)(&g_mcmcan.rxData[0]), INVALID_RX_DATA_VALUE, MAXIMUM_CAN_DATA_PAYLOAD * sizeof(uint32));
    /* Initialization of the TX message with the default configuration */
    IfxCan_Can_initMessage(&g_mcmcan.txMsg);

    /* Define the content of the data to be transmitted */
    g_mcmcan.txData[0] = low;
    g_mcmcan.txData[1] = high;

    /* Set the message ID that is used during the receive acceptance phase */
    g_mcmcan.txMsg.messageId = msgId;
    g_mcmcan.txMsg.messageIdLength = IfxCan_MessageIdLength_extended;
    g_mcmcan.txMsg.dataLengthCode = IfxCan_DataLengthCode_8;
    g_mcmcan.txMsg.frameMode = IfxCan_FrameMode_standard;

    /* Send the CAN message with the previously defined TX message content */
#if 0
    while( IfxCan_Status_notSentBusy ==
           IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode, &g_mcmcan.txMsg, &g_mcmcan.txData[0]) )
    {
    }
#else
    return IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode, &g_mcmcan.txMsg, &g_mcmcan.txData[0]);
#endif
}


uint32 can_doCanSend(void* lpParam)
{
	CanMgrTxNodeQ_t *pCanMsg = (CanMgrTxNodeQ_t*)lpParam;
	uint32 low;
	uint32 high;
	IfxCan_Status status;

	low = *(uint32*)(pCanMsg->tx_data);
	high = *(uint32*)(pCanMsg->tx_data+1);
  
	do
	{
		status = can_transmitCanMessage( pCanMsg->pkt.msg.messageId, low, high );
	} while( status == IfxCan_Status_notSentBusy );

	return 0;


} /* END of can_doCanSend() */

void can_test( void )
{
	while( 1 )
	{
		can_transmitCanMessage( 0x88, 0x02, 0x02 );
		delayMillSecond( 1000 );
	}
}


void start_can( void )
{
	CanDrv_initCan();
	can_test();
}

