/*
 * can_test.h
 *
 *  Created on: Jan. 31, 2023
 *      Author: rgeng
 */

#ifndef CAN_TEST_H_
#define CAN_TEST_H_

#include <IfxCan.h>
#include <IfxCan_Can.h>

#define MAXIMUM_CAN_DATA_PAYLOAD    2                       /* Define maximum classical CAN payload in 4-byte words */


/*--------------------------------------------------Data Structures--------------------------------------------------*/

typedef struct
{
    IfxCan_Can_Config canConfig;                            /* CAN module configuration structure                   */
    IfxCan_Can canModule;                                   /* CAN module handle                                    */
    IfxCan_Can_Node canSrcNode;                             /* CAN source node handle data structure                */
    IfxCan_Can_Node canDstNode;                             /* CAN destination node handle data structure           */
    IfxCan_Can_NodeConfig canNodeConfig;                    /* CAN node configuration structure                     */
    IfxCan_Filter canFilter;                                /* CAN filter configuration structure                   */
    IfxCan_Message txMsg;                                   /* Transmitted CAN message structure                    */
    IfxCan_Message rxMsg;                                   /* Received CAN message structure                       */
    uint32 txData[MAXIMUM_CAN_DATA_PAYLOAD];                /* Transmitted CAN data array                           */
    uint32 rxData[MAXIMUM_CAN_DATA_PAYLOAD];                /* Received CAN data array                              */
} McmcanType;


typedef struct CanPkt_s
{
	IfxCan_Message msg;		/**< \brief iLLD structure for CAN Message configuration (transmit/receive) */
	uint32      *data;	/**< \brief Pointer to word array for CAN frame data storage */
} CanPkt_t;



typedef struct CanMgrTxNodeQ_s
{
	uint16 u16State;
	CanPkt_t pkt;
	uint32 tx_data[16];
	boolean status;
} CanMgrTxNodeQ_t;


extern void start_can( void );

#endif /* CAN_TEST_H_ */
