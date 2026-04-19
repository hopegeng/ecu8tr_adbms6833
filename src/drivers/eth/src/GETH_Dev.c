/*
 * GETH_Dev.c
 *
 *  Created on: Jan 2024
 *      Author: rgeng
 */

/* C includes */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* IFX includes */
#include "Ifx_reg.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Cpu/Std/IfxCpu.h"
#include "Port/Std/IfxPort.h"
#include "Cpu/Std/Platform_Types.h"
#include "Geth/Std/IfxGeth.h"
#include "Geth/Eth/IfxGeth_Eth.h"
#include <IfxGeth_reg.h>
#include "Cpu/Irq/IfxCpu_Irq.h"
/* Application */
#include "shell.h"
#include "if_ether.h"
#include "GETH_Config.h"
#include "GETH_Phy.h"
#include "GETH_OS.h"
#include "GETH_Help.h"
#include "version.h"
#include "tools.h"

#define GETH_DRV_MAJOR  												(0x01u)
#define GETH_DRV_MINOR  												(0x00u)
#define GETH_DRV_INC  													(0x4u)

#define GETH_PHY_READ_TO                     ((uint32)0x0004FFFF)

#define CPU0_ISR_PRIORITY_ETH0_DMACH0						(0x10u)
#define CPU0_ISR_PRIORITY_ETH1_DMACH0						(0x14u)


extern uint8_t network_def_config[5][8];

/*----- Constant member variables --------------------------------------------*/
const VERSION_t gethDrvVersion __align(4) =
{
	.ID = (0u),
	.MAJOR = GETH_DRV_MAJOR,
	.MINOR = GETH_DRV_MINOR,
	.INC = GETH_DRV_INC,
	.pStr = "gethDrv"
};

GETH_CONFIG_t xGethConfig[IFXGETH_NUM_MODULES] __align( 128 ) =
{
	{
		.pGeth = &MODULE_GETH,

		.pMacAddr = network_def_config[0],
		.maxPacketSize = 1518,	/* iLLD current dosen't support 1522 packet size */

		.phyAddr = 0x07,
		.phyIfMode = IfxGeth_PhyInterfaceMode_rgmii,

		/* MTL Configuration */
		.txqCnt = IFXGETH_NUM_TX_QUEUES,
		.txqScheduleAlgo = IfxGeth_TxSchedulingAlgorithm_sp,		/* Strict priority */
		.txqStoreAndForwarded = { TRUE, TRUE, TRUE, TRUE },
		.txqUnderFlowIntEnabled = { FALSE, FALSE, FALSE, FALSE },
		.txqSize = { IfxGeth_QueueSize_2048Bytes, IfxGeth_QueueSize_2048Bytes, IfxGeth_QueueSize_2048Bytes, IfxGeth_QueueSize_2048Bytes },

		.rxqCnt = IFXGETH_NUM_RX_QUEUES,
		.rxqArbitrationAlgo = IfxGeth_RxArbitrationAlgorithm_wsp,
		.rxqSize = { IfxGeth_QueueSize_2048Bytes, IfxGeth_QueueSize_2048Bytes, IfxGeth_QueueSize_2048Bytes, IfxGeth_QueueSize_2048Bytes },
		.rxqDaBasedMap = { FALSE, FALSE, FALSE, FALSE },
		.rxqStoreAndForwarded = { TRUE, TRUE, TRUE, TRUE },
		.rxqOverFlowIntEnabled = { TRUE, TRUE, TRUE, TRUE },
		.rxqMapping = { 0, 1, 2, 3 },

		/* DMA Configuration */
		.dmaFixedBurstEnable = TRUE,
		.dmaAddrAlignedBeatsEnable = TRUE,
		.dmaTxRxArbitScheme = 1,
		.dmaMaxBurstLen = IfxGeth_DmaBurstLength_8,
		.dmaTxChanCnt = IFXGETH_NUM_TX_CHANNELS,
		.dmaRxChanCnt = 1,// IFXGETH_NUM_RX_CHANNELS,

	},

};


GETH_STATS_t xGethStatus[IFXGETH_NUM_MODULES] =
{
		{ .xmit = 0u, .recv = { 0u, 0u, 0u, 0u }, .broadcast = 0u, .prp_managment = 0u, .ping = 0u, .prp_dropped = 0u, .freertos_queue_error = 0u,
		  .recv_errors = { 0u, 0u, 0u, 0u },
		},
};

__far volatile uint32_t u32GethIsrCnt[IFXGETH_NUM_MODULES][IFXGETH_NUM_RX_CHANNELS] = { {0u, 0u, 0u, 0u} };

GETH_RX_CHANNEL_t xRxChannel[IFXGETH_NUM_MODULES][IFXGETH_NUM_RX_CHANNELS] =
{
	{
			{.u16RxBufferSize = GETH_RX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
			{.u16RxBufferSize = GETH_RX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
			{.u16RxBufferSize = GETH_RX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
			{.u16RxBufferSize = GETH_RX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
	},

};

GETH_TX_CHANNEL_t xTxChannel[IFXGETH_NUM_MODULES][IFXGETH_NUM_TX_CHANNELS] =
{
		{
				{.u16TxBufferSize = GETH_TX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
				{.u16TxBufferSize = GETH_TX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
				{.u16TxBufferSize = GETH_TX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
				{.u16TxBufferSize = GETH_TX_BUFFER_SIZE, .bVlanEnable = FALSE, .u16CurDescIdx = 0x0u },
		},

};




void __interrupt(CPU0_ISR_PRIORITY_ETH0_DMACH0) __vector_table(0) GETH_Dev_geth0_rx0_isr(void)
{
	Ifx_GETH *pGeth;
	uint16 intSrc;


	intSrc = 0x0u;
	pGeth = xGethConfig[0].pGeth;

	if( pGeth->DMA_CH[0].STATUS.B.RI == 1 )
	{
		u32GethIsrCnt[0][0]++;

		GETH_OS_notify_rx( 0x0u, 0x0u );
	}

	/* All Flags clear */
	pGeth->DMA_CH[0].STATUS.U = (0x003FFFC7u);
}/* END ethif_DmaCh0RxIsr() */

void __interrupt(CPU0_ISR_PRIORITY_ETH0_DMACH0+1) __vector_table(0) GETH_Dev_geth0_rx1_isr(void)
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[0].pGeth;
	if( pGeth->DMA_CH[1].STATUS.B.RI == 1 )
	{
		u32GethIsrCnt[0][1]++;

		GETH_OS_notify_rx( 0x0u, 0x1u );


	}

	/* All Flags clear */
	pGeth->DMA_CH[1].STATUS.U = (0x003FFFC7u);
}/* END ethif_DmaCh0RxIsr() */

void __interrupt(CPU0_ISR_PRIORITY_ETH0_DMACH0+2) __vector_table(0) GETH_Dev_geth0_rx2_isr(void)
{
	Ifx_GETH *pGeth;
	uint16 intSrc;


	intSrc = 2u;
	pGeth = xGethConfig[0].pGeth;
	if( pGeth->DMA_CH[2].STATUS.B.RI == 1 )
	{
		u32GethIsrCnt[0][2]++;
		GETH_OS_notify_rx( 0x0u, 0x2u );
	}

	/* All Flags clear */
	pGeth->DMA_CH[2].STATUS.U = (0x003FFFC7u);
}/* END ethif_DmaCh0RxIsr() */


void __interrupt(CPU0_ISR_PRIORITY_ETH0_DMACH0+3) __vector_table(0) GETH_Dev_geth0_rx3_isr(void)
{
	Ifx_GETH *pGeth;
	uint16 intSrc;


	intSrc = 3u;
	pGeth = xGethConfig[0].pGeth;
	if( pGeth->DMA_CH[3].STATUS.B.RI == 1 )
	{
		u32GethIsrCnt[0][3]++;
		GETH_OS_notify_rx( 0x0u, 0x3u );
	}

	/* All Flags clear */
	pGeth->DMA_CH[3].STATUS.U = (0x003FFFC7u);
}/* END ethif_DmaCh0RxIsr() */


void __interrupt(CPU0_ISR_PRIORITY_ETH1_DMACH0) __vector_table(0) GETH_Dev_geth1_rx0_isr(void)
{
	Ifx_GETH *pGeth;
	uint16 intSrc;


	intSrc = (1u<<8) | (0u);
	pGeth = xGethConfig[1].pGeth;
	if( pGeth->DMA_CH[0].STATUS.B.RI == 1 )
	{
		u32GethIsrCnt[1][0]++;
		GETH_OS_notify_rx( 0x1u, 0x0u );
	}

	/* All Flags clear */
	pGeth->DMA_CH[0].STATUS.U = (0x003FFFC7u);
}/* END ethif_DmaCh0RxIsr() */


boolean GETH_Dev_config_rx_desc( uint8 gethIdx )
{

	if( gethIdx != 0 && gethIdx != 1 )
	{
		return FALSE;
	}

	for( uint8 chanIdx = 0; chanIdx < IFXGETH_NUM_RX_CHANNELS; chanIdx++ )
	{
		for( int descIdx = 0; descIdx < IFXGETH_MAX_RX_DESCRIPTORS; descIdx++ )
		{
			xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx].RDES0.U = (uint32_t)xGethConfig[gethIdx].pRxChannel[chanIdx].rxBuffer[descIdx];
			xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx].RDES2.U = 0;	/* Not used */

			xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx].RDES3.R.BUF1V = (1u);	/* Valid */
			xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx].RDES3.R.BUF2V = (0u); /* Not Valid */
			xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx].RDES3.R.IOC = (1u);	/* Interrupt Enabled */
			xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx].RDES3.R.OWN = (1u);	/* Owned by DMA */
		}

		xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx = (0x00u);
		xGethConfig[gethIdx].pRxChannel[chanIdx].bVlanEnable = FALSE;

	}

	return TRUE;

}


boolean GETH_Dev_config_tx_desc( uint8 gethIdx )
{

	if( gethIdx != 0 && gethIdx != 1 )
	{
		return FALSE;
	}

	for( uint8 chanIdx = 0; chanIdx < IFXGETH_NUM_TX_CHANNELS; chanIdx++ )
	{
		for( int descIdx = 0; descIdx < IFXGETH_MAX_TX_DESCRIPTORS; descIdx++ )
		{
			xGethConfig[gethIdx].pTxChannel[chanIdx].xTxDescList[descIdx].TDES0.U = 0;
			xGethConfig[gethIdx].pTxChannel[chanIdx].xTxDescList[descIdx].TDES1.U = 0;
			xGethConfig[gethIdx].pTxChannel[chanIdx].xTxDescList[descIdx].TDES2.U = 0;
			xGethConfig[gethIdx].pTxChannel[chanIdx].xTxDescList[descIdx].TDES3.U = 0;
		}

		xGethConfig[gethIdx].pTxChannel[chanIdx].u16CurDescIdx = (0x00u);
		xGethConfig[gethIdx].pTxChannel[chanIdx].bVlanEnable = FALSE;

	}

	return TRUE;

}

static void GETH_Dev_config_dma_tx_channel( uint8 gethIdx, uint8 chanIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;

	pGeth->DMA_CH[chanIdx].TX_CONTROL.B.OSF = 1u;			/* Enable the operate on Second Packet( Page 2900, DMA_CHi_TX_CONTROL ) */
	pGeth->DMA_CH[chanIdx].TX_CONTROL.B.TXPBL = 8;			/* set TX PBL to 32 * 8 = 256 */

	/* Enable Interrupt: For TX, we don't enable interrupt yet */
#if 0
    volatile Ifx_SRC_SRCR *srcSFR;
    srcSFR = IfxGeth_getSrcPointer( pGeth, (IfxGeth_ServiceRequest)((uint32)IfxGeth_ServiceRequest_2 + chanIdx));
    srcSFR->B.SRPN = (gethIdx == 0)? CPU0_ISR_PRIORITY_ETH0_DMACH0 : CPU0_ISR_PRIORITY_ETH1_DMACH0;
    srcSFR->B.TOS  = IfxCpu_Irq_getTos( IfxCpu_getCoreIndex() );
    srcSFR->B.CLRR = 1u;
    srcSFR->B.SRE = 1u;
#endif

	/* Enable TSO */
	/* Program split header */

	/* Set the TX Descriptor parameters */
	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].TXDESC_RING_LENGTH.U = (uint32_t)(IFXGETH_MAX_TX_DESCRIPTORS - 1);
	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].TXDESC_LIST_ADDRESS.U = (uint32_t)xGethConfig[gethIdx].pTxChannel[chanIdx].xTxDescList;


}

boolean GETH_Dev_start_rx_channel( uint8 gethIdx, uint8 chanIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;
	pGeth->DMA_CH[chanIdx].RX_CONTROL.B.SR = (1u);

	return TRUE;
}
boolean GETH_Dev_start_mac_rx( uint8 gethIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;
	pGeth->MAC_CONFIGURATION.B.RE = (1u);

	return TRUE;
}


boolean GETH_Dev_start_rx( uint8 gethIdx )
{
	GETH_Dev_start_mac_rx( gethIdx );

	for( uint8 chanIdx = 0; chanIdx < IFXGETH_NUM_RX_CHANNELS; chanIdx++ )
	{
		GETH_Dev_start_rx_channel( gethIdx, chanIdx );
	}

	return TRUE;
}

boolean GETH_Dev_start_mac_tx( uint8 gethIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;
	pGeth->MAC_CONFIGURATION.B.TE = (1u);

	return TRUE;
}

boolean GETH_Dev_start_tx_channel( uint8 gethIdx, uint8 chanIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;
	pGeth->DMA_CH[chanIdx].TX_CONTROL.B.ST = (1u);

	return TRUE;
}

boolean GETH_Dev_start_tx( uint8 gethIdx )
{
	GETH_Dev_start_mac_tx( gethIdx );

	for( uint8 chanIdx = 0; chanIdx < IFXGETH_NUM_TX_CHANNELS; chanIdx++ )
	{
		GETH_Dev_start_tx_channel( gethIdx, chanIdx );
	}

	return TRUE;
}


static void GETH_Dev_config_dma_rx_channel( uint8 gethIdx, uint8 chanIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;

	pGeth->DMA_CH[chanIdx].RX_CONTROL.B.RBSZ_13_Y = (uint32)((2048 >> 2) & IFX_GETH_DMA_CH_RX_CONTROL_RBSZ_13_Y_MSK);
	pGeth->DMA_CH[chanIdx].RX_CONTROL.B.RXPBL = 8;			/* set RX PBL to 8 */
	/* We don't need receive interrupt timer because we enable IOC in the RXDESC */
	pGeth->DMA_CH[chanIdx].RX_INTERRUPT_WATCHDOG_TIMER.B.RWT = 0x0u;


	/* Enable TSO */
	/* Program split header */

	/* Enable Rx Interrupt */
	/* Enable receive interrupt interrupt */
	pGeth->DMA_CH[chanIdx].INTERRUPT_ENABLE.B.RIE = 1u;

	/* Set the RX Descriptor parameters */
	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].RXDESC_RING_LENGTH.U = (uint32_t)(IFXGETH_MAX_RX_DESCRIPTORS - 1);
	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].RXDESC_LIST_ADDRESS.U = (uint32_t)xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList;
	/* Fixme: RXDESC tail pointer, does it pointer to the xRxDescList[8] or xRxDescList[7].
	 * The iLLD function point it to xRxDescList[8]
	 * Here we also point to xRxDescList[8]
	 */
	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].RXDESC_TAIL_POINTER.U = (uint32_t)&xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[IFXGETH_MAX_RX_DESCRIPTORS-1];


    volatile Ifx_SRC_SRCR *srcSFR;
    srcSFR = IfxGeth_getSrcPointer( pGeth, (IfxGeth_ServiceRequest)((uint32)IfxGeth_ServiceRequest_6 + chanIdx));
    srcSFR->B.SRPN = chanIdx + ( (gethIdx == 0)? CPU0_ISR_PRIORITY_ETH0_DMACH0 : CPU0_ISR_PRIORITY_ETH1_DMACH0 );
    srcSFR->B.TOS  = IfxCpu_Irq_getTos( IfxCpu_getCoreIndex() );
    srcSFR->B.CLRR = 1u;
    srcSFR->B.SRE = 1u;

}

void GETH_Dev_config_dma_channel( uint8 gethIdx, uint8 chanIdx )
{
	uint32 intStatus;
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;


	/* Stop Transmit */
	pGeth->DMA_CH[chanIdx].TX_CONTROL.B.ST = 0u;

	/* Clear all interrupt status for this channel */
	intStatus = pGeth->DMA_CH[chanIdx].STATUS.U;
	pGeth->DMA_CH[chanIdx].STATUS.U = intStatus;

	/* Enable burst legth unit */
	pGeth->DMA_CH[chanIdx].CONTROL.B.PBLX8 = 0u;			/* Debug: the GETH wont work with this bit setting to 1,  ( Page 2899, DMA_CHi_CONTROL ) */

	GETH_Dev_config_dma_tx_channel( gethIdx, chanIdx );
	GETH_Dev_config_dma_rx_channel( gethIdx, chanIdx );
}

/**
 * @brief This API sets the DMA_MODE and DMA_SYSBUS register
 */
void GETH_Dev_dma_setmode( uint8 gethIdx )
{
	Ifx_GETH *pGeth;

	pGeth = xGethConfig[gethIdx].pGeth;

	pGeth->DMA_MODE.B.DA = 1u;			/* Weighted Round-Robin with RX:TX or TX:RX( Page 2892: DMA_MODE ) */
	pGeth->DMA_SYSBUS_MODE.B.FB = 1u;	/* Fixed Burst Length( Page 2895 ) */
	pGeth->DMA_SYSBUS_MODE.B.MB = 0u;	/* Mixed Burst Mode */
	pGeth->DMA_SYSBUS_MODE.B.AAL = 1u;	/*Address Aligned Beats( Page 2895 ) */

	pGeth->MAC_CONFIGURATION.B.TE = 0u;

}

/* For config DMA, refer to the steps on Page 2657 */
void GETH_Dev_config_dma( uint8 gethIdx )
{
	GETH_Dev_dma_setmode( gethIdx );

	for( uint8 chanIdx = 0; chanIdx < IFXGETH_NUM_TX_CHANNELS; chanIdx++ )
	{
		GETH_Dev_config_dma_channel( gethIdx, chanIdx );
	}
}


void GETH_Dev_config_mac( uint8 gethIdx )
{
	Ifx_GETH *pGeth;
    Ifx_GETH_MAC_CONFIGURATION macConfig;


	pGeth = xGethConfig[gethIdx].pGeth;

	/* Set the MII interface to fullDuplex, IfxGeth_LineSpeed_100Mbps */
	pGeth->MAC_CONFIGURATION.B.DM = IfxGeth_DuplexMode_fullDuplex;
	pGeth->MAC_CONFIGURATION.B.FES = 1u;
	pGeth->MAC_CONFIGURATION.B.PS = 1u;

	/* Set PreambleLength to 7Bytes */
	pGeth->MAC_CONFIGURATION.B.PRELEN = IfxGeth_PreambleLength_7Bytes;

	/*
	 * Below set the default MTU size = 1518( 1522 bytes for tagged ).
	 * for different MTU settings,
	 * please refer MAC_CONFIGURATION and MAC_EXT_CONFIGURATION
	 * registers on the page 2693
	 * */
	macConfig.U = pGeth->MAC_CONFIGURATION.U;
    macConfig.B.JE     = 0;
    macConfig.B.S2KP   = 0;
    macConfig.B.GPSLCE = 0;
    macConfig.B.JD     = 0;
    macConfig.B.TE 	   = 0u;		/* Stop MTL */
    pGeth->MAC_CONFIGURATION.U = macConfig.U;

    /* Disable padding and FCS stripping */
    pGeth->MAC_CONFIGURATION.B.ACS = 0u;
    pGeth->MAC_CONFIGURATION.B.CST = 0u;

    /* Set Mac Address */
    pGeth->MAC_ADDRESS_HIGH0.U = 0 | ((uint32)network_def_config[0][4] << 0U) | ((uint32)network_def_config[0][5] << 8U) | 0x80000000U;

    pGeth->MAC_ADDRESS_LOW0.U = 0 | ((uint32)network_def_config[0][0] << 0U) | ((uint32)network_def_config[0][1] << 8U) |
    							((uint32)network_def_config[0][2] << 16U)| (network_def_config[0][3] << 24U);
}

uint32 GETH_Dev_calculate_queue_size( uint32 fifo_size, uint8 queue_count )
{
	uint32 p_fifo = 0x0F; /* per queue fifo size programmable value */
	uint32 q_fifo_size;


	q_fifo_size = fifo_size / queue_count;
	if( q_fifo_size >= (8*1024) )
	{
		p_fifo = 0x1F;
	}
	else if( q_fifo_size >= (4*1024) )
	{
		p_fifo = 0x0F;	/* Queue Size = 4K */
	}
	else if( q_fifo_size >= (2*1024) )
	{
		p_fifo = 0x07;	/* Queue Size = 2K */
	}
	else if( q_fifo_size >= (1*1024) )
	{
		p_fifo = 0x03;	/* Queue Size = 1K */
	}
	else if( q_fifo_size >= 512 )
	{
		p_fifo = 0x01;	/* Queue Size = 512 */
	}
	else if( q_fifo_size >= 256 )
	{
		p_fifo = 0x0;	/* Queue Size = 256 */
	}

	return( p_fifo );
}

void GETH_Dev_config_mtl_rx_queue( uint8 gethIdx, uint8 queueIdx )
{
	Ifx_GETH *pGeth;
	uint32 u;

	pGeth = xGethConfig[gethIdx].pGeth;

	switch( queueIdx )
	{
	case 0:
		pGeth->MTL_RXQ0.OPERATION_MODE.B.FEP = 1u;		/* Page 2869: Forward error packet */
		pGeth->MTL_RXQ0.OPERATION_MODE.B.RSF = 1u;		/* Receive Queue Store and Forward mode with the Packet size <= 1522(including VLan), if >1522, we need to disable it */
		pGeth->MTL_RXQ0.OPERATION_MODE.B.FUP = 1u;		/* Forward under size good packet */
		break;
	case 1:
		pGeth->MTL_RXQ1.OPERATION_MODE.B.FEP = 1u;		/* Page 2869: Forward error packet */
		pGeth->MTL_RXQ1.OPERATION_MODE.B.RSF = 1u;		/* Receive Queue Store and Forward mode with the Packet size <= 1522(including VLan), if >1522, we need to disable it */
		pGeth->MTL_RXQ1.OPERATION_MODE.B.FUP = 1u;		/* Forward under size good packet */
		break;
	case 2:
		pGeth->MTL_RXQ2.OPERATION_MODE.B.FEP = 1u;
		pGeth->MTL_RXQ2.OPERATION_MODE.B.RSF = 1u;		/* Receive Queue Store and Forward mode with the Packet size <= 1522(including VLan), if >1522, we need to disable it */
		pGeth->MTL_RXQ2.OPERATION_MODE.B.FUP = 1u;		/* Forward under size good packet */
		break;
	case 3:
		pGeth->MTL_RXQ3.OPERATION_MODE.B.FEP = 1u;
		pGeth->MTL_RXQ3.OPERATION_MODE.B.RSF = 1u;		/* Receive Queue Store and Forward mode with the Packet size <= 1522(including VLan), if >1522, we need to disable it */
		pGeth->MTL_RXQ3.OPERATION_MODE.B.FUP = 1u;		/* Forward under size good packet */
		break;
	}

	const uint32 RX_MTL_FIFO_SIZE = 8 * 1024;	/* Page 2522 */
	u = GETH_Dev_calculate_queue_size( RX_MTL_FIFO_SIZE, IFXGETH_NUM_RX_QUEUES );
	IfxGeth_mtl_setRxQueueSize( pGeth, (IfxGeth_RxMtlQueue)queueIdx, (IfxGeth_QueueSize)u );

	/* MTL Qeueu to DMA channel mapping: one to one mapping ( Page 2859 )*/
	IfxGeth_mtl_setRxQueueDmaChannelMapping( pGeth, queueIdx, queueIdx );

	/* Enable Rx Queue */
	pGeth->MAC_RXQ_CTRL0.U |= (2 << (queueIdx * 2));

	/* Set the flow control: only each queue gets the fifo size >= 8K, so here we ignore it */

	/* Enable overflow interrupt */

}



void GETH_Dev_config_mtl_tx_queue( uint8 gethIdx, uint8 queueIdx )
{
	Ifx_GETH *pGeth;
	uint32 u;


	pGeth = xGethConfig[gethIdx].pGeth;

	/* Flush Tx Queue: MTL_TXQi_OPERATION_MODE( Page 2875 ) */
	switch( queueIdx )
	{
	case 0:
		pGeth->MTL_TXQ0.OPERATION_MODE.B.FTQ = 1u;
		u = 1000;
		while( u-- > 0 )
		{
			delayMillSecond( 1 );
			if( pGeth->MTL_TXQ0.OPERATION_MODE.B.FTQ == 0u )
			{
				break;
			}
		}
		break;
	case 1:
		pGeth->MTL_TXQ1.OPERATION_MODE.B.FTQ = 1u;
		u = 1000;
		while( u-- > 0 )
		{
			delayMillSecond( 1 );
			if( pGeth->MTL_TXQ1.OPERATION_MODE.B.FTQ == 0u )
			{
				break;
			}
		}

		break;
	case 2:
		pGeth->MTL_TXQ2.OPERATION_MODE.B.FTQ = 1u;
		u = 1000;
		while( u-- > 0 )
		{
			delayMillSecond( 1 );
			if( pGeth->MTL_TXQ2.OPERATION_MODE.B.FTQ == 0u )
			{
				break;
			}
		}

		break;
	case 3:
		pGeth->MTL_TXQ3.OPERATION_MODE.B.FTQ = 1u;
		u = 1000;
		while( u-- > 0 )
		{
			delayMillSecond( 1 );
			if( pGeth->MTL_TXQ3.OPERATION_MODE.B.FTQ == 0u )
			{
				break;
			}
		}

		break;
	}

	/* Enable Transmit Store and Forward */
	IfxGeth_mtl_setTxStoreAndForward( pGeth, queueIdx, FALSE );
	/* TX Queue Operation Mode: Enable Transmit Queue bit TSN = 0x02 */
	IfxGeth_mtl_enableTxQueue(  pGeth, (IfxGeth_TxMtlQueue)queueIdx );

	/* Tx Queue Weight Setting: Do we need to set it? */
	switch( queueIdx )
	{
	case 0:
		pGeth->MTL_TXQ0.QUANTUM_WEIGHT.B.ISCQW = (0x10 + gethIdx );
		break;
	case 1:
		pGeth->MTL_TXQ1.QUANTUM_WEIGHT.B.ISCQW = (0x10 + gethIdx );
		break;
	case 2:
		pGeth->MTL_TXQ2.QUANTUM_WEIGHT.B.ISCQW = (0x10 + gethIdx );
		break;
	case 3:
		pGeth->MTL_TXQ3.QUANTUM_WEIGHT.B.ISCQW = (0x10 + gethIdx );
		break;
	}

	const uint32 TX_MTL_FIFO_SIZE = 4 * 1024;	/* Page 2522 */
	u = GETH_Dev_calculate_queue_size( TX_MTL_FIFO_SIZE, IFXGETH_NUM_TX_QUEUES );
	IfxGeth_mtl_setTxQueueSize( pGeth, (IfxGeth_TxMtlQueue)queueIdx, (IfxGeth_QueueSize)u );

	/* Last Step: Set the MTL TX Interrupt */

}

void GETH_Dev_config_mtl_queue( uint8 gethIdx, uint8 queueIdx )
{
	Ifx_GETH *pGeth;


	pGeth = xGethConfig[gethIdx].pGeth;


	/* Clear all the interrupt status */
	IfxGeth_mtl_clearAllInterruptFlags( pGeth, (IfxGeth_MtlQueue)queueIdx );

	GETH_Dev_config_mtl_tx_queue( gethIdx, queueIdx );
	GETH_Dev_config_mtl_rx_queue( gethIdx, queueIdx );


}

void GETH_Dev_config_mtl( uint8 gethIdx )
{
	Ifx_GETH *pGeth;


	pGeth = xGethConfig[gethIdx].pGeth;

	/* Tx Queue Scheduling Algorithm */
	pGeth->MTL_OPERATION_MODE.B.SCHALG = 0x03; 	/*Strict priority algorithm( Page 2858 ) */
	/* Rx Queue Arbitration Algorithm */
	pGeth->MTL_OPERATION_MODE.B.RAA = 0x01;		/* Weighted Strict Priority ( Page 2857 ) */
	/* Rx Queue Routing configure: refer to MAC_RXQ_CTRL[1|2] register.
	 * By default, all the packets are routed to RX QUEUE 0
	 */

	for( uint8 queueIdx = 0; queueIdx < IFXGETH_NUM_TX_QUEUES; queueIdx++ )
	{
		GETH_Dev_config_mtl_queue( gethIdx, queueIdx );
	}
}


boolean GETH_Dev_stop_tx( uint8 gethIdx )
{
	uint8	retryCnt;
    GETH_CONFIG_t *pGethCfg = &xGethConfig[gethIdx];


    retryCnt = 10u;
    pGethCfg->pGeth->MAC_CONFIGURATION.B.TE = (0u);

    for( int idx = 0; idx < pGethCfg->dmaTxChanCnt; idx++ )
    {
    	pGethCfg->pGeth->DMA_CH[idx].TX_CONTROL.B.ST = (0u);
    	do
    	{
    		uint8 tps0;


    		delayMillSecond( 1 );
    		tps0 = pGethCfg->pGeth->DMA_DEBUG_STATUS0.B.TPS0;
    		if( tps0 == 0u || tps0 == 6u )						/* Page 2898: stop = 0u; suspended = 6u */
    		{
    			break;
    		}
    		retryCnt--;
    	} while( retryCnt > 0u );

    	if( retryCnt == 0u )
    	{
    		return FALSE;
    	}
    }

    return TRUE;
}




boolean GETH_Dev_hw_reset( uint8 gethIdx )
{
	Ifx_GETH *pGeth;


	pGeth =	xGethConfig[gethIdx].pGeth;

    /* Enable Module */
    IfxGeth_enableModule( pGeth );

    /* reset the Module */
    IfxGeth_resetModule( pGeth );
    /* select the Phy Interface Mode */
    IfxGeth_setPhyInterfaceMode( pGeth , IfxGeth_PhyInterfaceMode_rgmii );

    IfxGeth_dma_applySoftwareReset( pGeth  );

    /* wait until reset is finished or timeout. */
    {
        uint32 timeout = 0;

        while( (IfxGeth_dma_isSoftwareResetDone( pGeth  ) == 0) && (timeout < IFXGETH_MAX_TIMEOUT_VALUE) )
        {
            timeout++;
        }
    }

    return TRUE;
}

boolean GETH_Dev_wakeup_rx( uint8 gethIdx, uint8 chanIdx )
{
	Ifx_GETH *pGeth;


	pGeth = xGethConfig[gethIdx].pGeth;

    /* check if receiver suspended */
    if (IfxGeth_dma_isInterruptFlagSet( pGeth, (IfxGeth_DmaChannel)chanIdx, IfxGeth_DmaInterruptFlag_receiveStopped))
    {
        /* check if receive buffer unavailable */
        if (IfxGeth_dma_isInterruptFlagSet( pGeth, (IfxGeth_DmaChannel)chanIdx, IfxGeth_DmaInterruptFlag_receiveBufferUnavailable))
        {
            IfxGeth_dma_clearInterruptFlag( pGeth, (IfxGeth_DmaChannel)chanIdx, IfxGeth_DmaInterruptFlag_receiveBufferUnavailable);
        }

        GETH_Dev_start_mac_rx( gethIdx );
        GETH_Dev_start_rx_channel( gethIdx, chanIdx );
    }

	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].RXDESC_TAIL_POINTER.U = (uint32_t)&xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[IFXGETH_MAX_RX_DESCRIPTORS];

	return TRUE;

}


boolean GETH_Dev_release_rx_desc( uint8 gethIdx, uint8 chanIdx, uint16 descIdx, uint8 dirtyCnt )
{
		void *pDmaBuff;
	    volatile IfxGeth_RxDescr *pDesc;


	    for( uint8 idx = 0; idx < dirtyCnt; idx++ )
	    {
			pDesc = &xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx];
			pDmaBuff = xGethConfig[gethIdx].pRxChannel[chanIdx].rxBuffer[descIdx];

			pDesc->RDES0.U = (uint32_t)pDmaBuff;
			pDesc->RDES1.U = 0u;
			pDesc->RDES2.R.BUF2AP = 0u;
			pDesc->RDES3.U 		  = 0;
			pDesc->RDES3.R.BUF1V  = 1; /* buffer 1 valid */
			pDesc->RDES3.R.BUF2V  = 0; /* buffer 2 not valid */
			pDesc->RDES3.R.IOC    = 1; /* interrupt enabled */
			pDesc->RDES3.R.OWN    = 1; /* owned by DMA */

			descIdx = ( descIdx+1 ) % IFXGETH_MAX_RX_DESCRIPTORS;
	    }

		//xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx = ( descIdx + 1 ) % IFXGETH_MAX_RX_DESCRIPTORS;

		return TRUE;
}


uint16 GETH_Dev_get_rx_index( uint8 gethIdx, uint8 chanIdx )
{
	return( xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx );
}

boolean GETH_Dev_rx_packet_avail( uint8 gethIdx, uint8 chanIdx )
{
	Ifx_GETH *pGeth;
	uint16 descIdx;
    volatile IfxGeth_RxDescr *pDesc;


	pGeth = xGethConfig[gethIdx].pGeth;
	descIdx = xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx;
	pDesc = &xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx];

	if( pDesc->RDES3.W.OWN == 1u )
	{
		return FALSE;
	}

	return TRUE;
}


void* GETH_Dev_recv_packet( uint8 gethIdx, uint8 chanIdx, uint16_t *pFrameLen )
{
	uint16 descIdx;
	void *pDmaBuff;
    volatile IfxGeth_RxDescr *pDesc;


	descIdx = xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx;
	pDesc = &xGethConfig[gethIdx].pRxChannel[chanIdx].xRxDescList[descIdx];
	pDmaBuff = xGethConfig[gethIdx].pRxChannel[chanIdx].rxBuffer[descIdx];
	*pFrameLen = (0u);

	if( GETH_Dev_rx_packet_avail(gethIdx, chanIdx) == TRUE )
	{
		xGethStatus[gethIdx].recv[chanIdx]++;
		if( pDesc->RDES3.W.LD == 1u && pDesc->RDES3.W.ES == 0u )
		{
			if( pDesc->RDES1.W.IPCE == 1u )
			{
				PRINTF( "Received packet on port %d, channel %d: IP Payload Error.\r\n", gethIdx, chanIdx );
			}

			if( pDesc->RDES1.W.IPCB == 1u )
			{
				PRINTF( "Received packet on port %d, channel %d: IP Checksum bypassed.\r\n", gethIdx, chanIdx );
			}

			if( pDesc->RDES1.W.IPHE == 1u )
			{
				PRINTF( "Received packet on port %d, channel %d: IP Checksum Header Error.\r\n", gethIdx, chanIdx );
			}

			*pFrameLen = (uint16)pDesc->RDES3.W.PL;		/* Remove the CRC */
			if( pDesc->RDES3.W.PL > 4u )
			{
				*pFrameLen -= 4u;		/* Remove the CRC */
			}
			else
			{
				PRINTF( "Received packet on port %d, channel %d: Received packet's length <= 4 bytes.\r\n", gethIdx, chanIdx );
			}
		}
		else
		{
			xGethStatus[gethIdx].recv_errors[chanIdx]++;
			if( pDesc->RDES3.W.OE == 1u )
			{
				PRINTF( "Received packet on port %d, channel %d: Receive overflow.\r\n", gethIdx, chanIdx );
			}
			if( pDesc->RDES3.W.CE == 1u )
			{
				PRINTF( "Received packet on port %d, channel %d: CRC Error.\r\n", gethIdx, chanIdx );
			}
			if( pDesc->RDES3.W.RE == 1u )
			{
				PRINTF( "Received packet on port %d, channel %d: Receive Error.\r\n", gethIdx, chanIdx );
			}
			if( pDesc->RDES3.W.LD == 0u )
			{
				PRINTF( "Received packet on port %d, channel %d: Last Descriptor Error.\r\n", gethIdx, chanIdx );
			}

		    return( NULL_PTR );
		}

		return( pDmaBuff );
	}

	return( NULL_PTR );
}



void GETH_Dev_hw_init( uint8 gethIdx )
{
	GETH_Phy_reset_PHY();

	GETH_Dev_hw_reset( gethIdx );

	GETH_Phy_set_ECU8_rgmii_input_pins( xGethConfig[gethIdx].pGeth  );
	GETH_Phy_set_ECU8_rgmii_output_pins( xGethConfig[gethIdx].pGeth );

	GETH_Dev_config_mac( gethIdx );
	GETH_Dev_config_mtl( gethIdx );
	GETH_Dev_config_dma( gethIdx );

	GETH_Dev_config_rx_desc( gethIdx );
	GETH_Dev_config_tx_desc( gethIdx );


}

sint32 GETH_Dev_enable_geth( boolean bState, uint8 gethIdx )
{
	sint32 i32Rtn = -1;
	Ifx_GETH *pGeth;
	GETH_CONFIG_t *pGethConfig;


	pGethConfig = &xGethConfig[gethIdx];

	if( pGethConfig == NULL )
	{
		return ( i32Rtn );
	}
	pGeth  = pGethConfig->pGeth;

	if( bState == TRUE )
	{
		pGethConfig->pTxChannel = xTxChannel[gethIdx];
		pGethConfig->pRxChannel = xRxChannel[gethIdx];


		GETH_Dev_hw_init( gethIdx );

		i32Rtn = 0;
	}
	else if( bState == FALSE )
	{

		IfxGeth_dma_applySoftwareReset( pGeth );
		while( IfxGeth_dma_isSoftwareResetDone(pGeth) );

#if 0
		/* Stop any transfers */
		ethif_StopReceiver();
		ethif_StopTransmitter();

		/* Suspend reception of received data (RxD pipe to LWIP) */
		ethif_SuspendRxPipeTask();
#endif


		i32Rtn = 0;
	}


	return(i32Rtn);
}/* END ethif_EnableMac() */

void * GETH_Dev_get_tx_buffer( uint8 gethIdx, uint8 chanIdx )
{
	uint16 currDescIdx = xGethConfig[gethIdx].pTxChannel[chanIdx].u16CurDescIdx;

	return( (void*)(xGethConfig[gethIdx].pTxChannel[chanIdx].txBuffer[currDescIdx]) );
}

void * GETH_Dev_get_tx_desc( uint8 gethIdx, uint8 chanIdx )
{
	uint16 curDescIdx = xGethConfig[gethIdx].pTxChannel[chanIdx].u16CurDescIdx;

	return( (void*)&(xGethConfig[gethIdx].pTxChannel[chanIdx].xTxDescList[curDescIdx]) );
}

uint16 GETH_Dev_forward_tx_desc_index( uint8 gethIdx, uint8 chanIdx )
{
	uint16 currDescIdx = xGethConfig[gethIdx].pTxChannel[chanIdx].u16CurDescIdx;

	xGethConfig[gethIdx].pTxChannel[chanIdx].u16CurDescIdx = ( currDescIdx + 1u ) % IFXGETH_MAX_TX_DESCRIPTORS;

	return currDescIdx;

}

void GETH_Dev_forward_rx_desc_index( uint8 gethIdx, uint8 chanIdx )
{
	uint16 descIdx;


	descIdx = xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx;
	xGethConfig[gethIdx].pRxChannel[chanIdx].u16CurDescIdx = ( descIdx + 1 ) % IFXGETH_MAX_RX_DESCRIPTORS;
}

void GETH_Dev_wakeup_tx( uint8 gethIdx, uint8 chanIdx )
{

	if( IfxGeth_dma_isInterruptFlagSet(xGethConfig[gethIdx].pGeth, (IfxGeth_DmaChannel)chanIdx, IfxGeth_DmaInterruptFlag_transmitStopped) )
	{
		/* check if transmit buffer unavailable */
		if (IfxGeth_dma_isInterruptFlagSet( xGethConfig[gethIdx].pGeth, (IfxGeth_DmaChannel)chanIdx, IfxGeth_DmaInterruptFlag_transmitBufferUnavailable) )
		{
			IfxGeth_dma_clearInterruptFlag( xGethConfig[gethIdx].pGeth, (IfxGeth_DmaChannel)chanIdx, IfxGeth_DmaInterruptFlag_transmitBufferUnavailable );
		}

		/* check MTL underflow flag */
		if (IfxGeth_mtl_isInterruptFlagSet( xGethConfig[gethIdx].pGeth, (IfxGeth_MtlQueue)chanIdx, IfxGeth_MtlInterruptFlag_txQueueUnderflow) )
		{
			IfxGeth_mtl_clearInterruptFlag( xGethConfig[gethIdx].pGeth, (IfxGeth_MtlQueue)chanIdx, IfxGeth_MtlInterruptFlag_txQueueUnderflow );
		}

		GETH_Dev_start_mac_tx( gethIdx );
		GETH_Dev_start_tx_channel( gethIdx, chanIdx );
	}
}

uint16 GETH_Dev_dma_xmit( uint8 gethIdx, uint8 chanIdx, uint8 *pData, uint16_t dataLen )
{
	uint16 preIdx;
	uint8 *pDamBuf;
	IfxGeth_TxDescr *pCurDesc;



	GETH_OS_lock();
	pDamBuf = GETH_Dev_get_tx_buffer( gethIdx, chanIdx );
	pCurDesc = GETH_Dev_get_tx_desc( gethIdx, chanIdx );


	xGethStatus[gethIdx].xmit++;

	while( pCurDesc->TDES3.R.OWN != (00u) )
		;

	if( chanIdx == 3 )
	{
#if GMAC_VLAN_PACKET_BASED

#if !GMAC_VLAN_ID_FROM_REG
		pCurDesc->TDES3.C.VT = GMAC_VLAN_TAG_ID + 2;
		pCurDesc->TDES3.C.VLTV = (0x01u);
		pCurDesc->TDES3.C.CTXT = (0x01u);
		pCurDesc->TDES3.C.OWN = (0x01u);

		ray_forwardTxDMAIndex( gethIdx, chanIdx );
		pCurDesc = ray_getTxDMADesc(  gethIdx, chanIdx );
		pDamBuf = ray_getTxDMABuff( gethIdx, chanIdx );
#endif
		if( xGethConfig[gethIdx].pTxChannel[chanIdx].bVlanEnable == TRUE )
		{
			pCurDesc->TDES2.R.VTIR = (0x02u);
		}

#else	//#if GMAC_VLAN_PACKET_BASED

#if !GMAC_VLAN_ID_FROM_REG
		pCurDesc->TDES3.C.VLTV = (0x01u);
		pCurDesc->TDES3.C.CTXT = (0x01u);
		pCurDesc->TDES3.C.OWN = (0x01u);
		ray_forwardTxDMAIndex( gethIdx, chanIdx );
		pCurDesc = ray_getTxDMADesc(  gethIdx, chanIdx );
		pDamBuf = ray_getTxDMABuff( gethIdx, chanIdx );
		pCurDesc->TDES2.R.VTIR = (0x02u);
#endif

#endif //#if GMAC_VLAN_PACKET_BASED

	}

	/* Step 1: Update the buffer pointer and data length */
	memcpy( (void*)pDamBuf, (void*)pData, dataLen );

	pCurDesc->TDES0.U = (uint32_t)pDamBuf;

	pCurDesc->TDES2.R.B1L = dataLen;
	pCurDesc->TDES1.R.BUF2AP = (0x00u);
	pCurDesc->TDES2.R.B2L = (0x00u);

	/* Step 2: update total length of packet */
	pCurDesc->TDES3.R.FL_TPL = dataLen;

	/* Step 4: Enable CRC and Pad Insertion( NOTE: set this only for FIRST descriptor */
	pCurDesc->TDES3.R.CPC = (0x00u);
	/* Step 5: Enable hardware checksum */
	pCurDesc->TDES3.R.CIC_TPL = (0x03u);
	/* Step 6: Disable SA Insertion */
	pCurDesc->TDES3.R.SAIC = (0x00u);

	/* Normal Descriptor */
	pCurDesc->TDES3.R.CTXT = (0x00u);

	/* Step 7: Set the interrupt to the last descriptor */
	pCurDesc->TDES2.R.IOC = (0x00u);
	/* Step 8: Set the Last descriptor flag */
	pCurDesc->TDES3.R.LD = (0x01u);

	/* Step 3: Set the first descriptor */
	pCurDesc->TDES3.R.FD = (0x01u);

	/* Step 19: set OWN bit of first descriptor at the end to avoid race condition */
	pCurDesc->TDES3.R.OWN = (0x01u);


	/* Step 10: Move the pointer 1 step forward */
	preIdx = GETH_Dev_forward_tx_desc_index( gethIdx, chanIdx );
	pCurDesc = GETH_Dev_get_tx_desc( gethIdx, chanIdx );

	/* Step 11: issue a poll command to TX DMA by writting addess of next immediate free descriptor */
	xGethConfig[gethIdx].pGeth->DMA_CH[chanIdx].TXDESC_TAIL_POINTER.U = (uint32_t)pCurDesc;

	/* Step 12: In case DMA TX stopping running */
	GETH_Dev_wakeup_tx( gethIdx, chanIdx );

	GETH_OS_unlock();

	return( preIdx );

}




