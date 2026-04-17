/*
 * GETH_Config.h
 *
 *  Created on: Dec 2023
 *      Author: rgeng
 */

#ifndef BSP_ETH_INC_GETH_CONFIG_H_
#define BSP_ETH_INC_GETH_CONFIG_H_


#define GETH_TX_BUFFER_SIZE 						(1536u)
#define GETH_RX_BUFFER_SIZE 						(2048u)

#define GETH_MAC_LEN			(6u)
#define GETH_TOTAL_LEN			(1518u)			/* ETH_FRAME_LEN + ETH_FRER_LEN, not including 802.1Q header, because it is added by hardware */


#define GMAC_TDESC_NUM	(8u)
#define GMAC_MAC_NUM	(2u)
#define GMAC_DMA_CHANNEL_NUM	(4u)

/*
 * 		The block is for VLAN control
 */
#define GMAC_VLAN_ENABLE_DVLAN	(0u)				/* Disable Double VLAN */
#define GMAC_VLAN_PACKET_BASED	(0x0u)				/* Enable tagging selected packets: TXDECS2.B.VTIR = 2; and VLP = 1, VLTI = 0, VLC = 2, VLT = tag+priority in the MAC_VLAN_INCL */
#define GMAC_VLAN_ID_FROM_REG	(0x1u)				/* when this bit is set enable the vlan ID from VLT of MAC_VLAN_Inc register */

/* Ray: we use MACROs definition to specify the IOCTRL functions used by Linux system */
#define GMAC_VLAN_BY_REG		(0x00u)
#define GMAC_VLAN_BY_DESC		(0x00u)
#define GMAC_VLAN_TAG_ID		(0x08)				/* Here we need to OR priority */


#define GMAX_VLAN_TX_CHANNEL	(0x03u)
#define GMAX_ETH0_TX_CHANNEL	(0x00u)

static const uint8 NET_MASK[] = { 255, 255, 255, 0 };
static const uint8 GATE_WAY[] = { 0, 0, 0, 0 };

static const uint8 GETH_MAC_ADDRESS[IFXGETH_NUM_MODULES][GETH_MAC_LEN] =
{
		{0x0a, 0x0c, 0x00, 0x00, 0x02, 0x01},
};

static const uint8 IP_ADDRESS_GETH[IFXGETH_NUM_MODULES][4] =
{
		{ 192, 168, 1, 170 },
};


typedef struct GETH_RX_CHANNEL_s
{
	volatile IfxGeth_RxDescr xRxDescList[IFXGETH_MAX_RX_DESCRIPTORS] __align(4);
	uint8 rxBuffer[IFXGETH_MAX_RX_DESCRIPTORS][GETH_RX_BUFFER_SIZE] __align(4); 	/* Here we have to take FRER header into account */
	uint16 u16CurDescIdx;
	uint16 u16RxBufferSize;
	boolean bVlanEnable;
} GETH_RX_CHANNEL_t;

typedef struct GETH_TX_CHANNEL_s
{
	volatile IfxGeth_TxDescr xTxDescList[IFXGETH_MAX_TX_DESCRIPTORS] __align(4);
	uint8 txBuffer[IFXGETH_MAX_TX_DESCRIPTORS][GETH_TX_BUFFER_SIZE] __align(4); 	/* Here we have to take FRER header into account */
	uint16 u16CurDescIdx;
	uint16 u16TxBufferSize;
	boolean bVlanEnable;
} GETH_TX_CHANNEL_t;


typedef struct GETH_CONFIG_s
{
	Ifx_GETH *pGeth;
	//IfxGeth_Eth ethConfig;

	GETH_TX_CHANNEL_t *pTxChannel;
	GETH_RX_CHANNEL_t *pRxChannel;

	const uint8 *pMacAddr;
	uint16 maxPacketSize;

	uint32 phyAddr;
	IfxGeth_PhyInterfaceMode phyIfMode;

	/* MTL configuration */
	uint8	txqCnt;
	uint16 txqSize[IFXGETH_NUM_TX_QUEUES];
	IfxGeth_TxSchedulingAlgorithm txqScheduleAlgo;		/**< \brief Scheduling Algorithm for Tx queues when no of queues are more than 1 */
	boolean txqStoreAndForwarded[IFXGETH_NUM_TX_QUEUES];
	boolean txqUnderFlowIntEnabled[IFXGETH_NUM_TX_QUEUES];

	uint8 rxqCnt;
	IfxGeth_RxArbitrationAlgorithm ;
	IfxGeth_RxArbitrationAlgorithm rxqArbitrationAlgo;
	uint16 rxqSize[IFXGETH_NUM_RX_QUEUES];
	boolean rxqStoreAndForwarded[IFXGETH_NUM_RX_QUEUES];
	boolean rxqForwardErrorPacket[IFXGETH_NUM_RX_QUEUES];
	boolean rxqForwardUnderSizedGoodPacket[IFXGETH_NUM_RX_QUEUES];
	boolean rxqOverFlowIntEnabled[IFXGETH_NUM_RX_QUEUES];
	boolean rxqDaBasedMap[IFXGETH_NUM_RX_QUEUES];
	uint8 rxqMapping[IFXGETH_NUM_RX_QUEUES];

	/* DMA configuration */
	boolean dmaFixedBurstEnable;
	boolean dmaAddrAlignedBeatsEnable;
	uint8 dmaTxRxArbitScheme;
	uint8 dmaMaxBurstLen;			//This burst length for both TX and RX, and for all the channels

	uint8 dmaTxChanCnt;

	uint8 dmaRxChanCnt;


} GETH_CONFIG_t;

typedef struct GETH_STATS_s
{
	uint32_t xmit;
	uint32_t recv[IFXGETH_NUM_RX_CHANNELS];
	uint32_t recv_errors[IFXGETH_NUM_RX_CHANNELS];
	uint32_t prp_managment;
	uint32_t broadcast;
	uint32_t ping;
	uint32_t prp_dropped;
	uint32_t freertos_queue_error;
} GETH_STATS_t;

#endif /* BSP_ETH_INC_GETH_CONFIG_H_ */
