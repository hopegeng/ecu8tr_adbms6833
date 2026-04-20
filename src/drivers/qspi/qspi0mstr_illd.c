/*
 * qspi0mstr_illd.c
 *
 *  Created on: Oct 22, 2024
 *      Author: Ahmed Afifi
 */

#include <IfxQspi_SpiMaster.h>
#include "CompilerTasking.h"
#include "IfxCpu.h"
#include "qspi0mstr_illd.h"
#include "FreeRTOS.h"
#include "task.h"

static bool g_qspi0IlldInitialized = false;

IfxQspi_SpiMaster spi;
IfxQspi_SpiMaster_Channel spiChannel;
IfxQspi_SpiMaster_ChannelConfig spiMasterChannelConfig;

// CS2 pin configuration
IfxQspi_SpiMaster_Output slsOutput2 = {
    &IfxQspi0_SLSO2_P20_13_OUT,
    IfxPort_OutputMode_pushPull,
    IfxPort_PadDriver_cmosAutomotiveSpeed3
};

// CS6 pin configuration
IfxQspi_SpiMaster_Output slsOutput6 = {
    &IfxQspi0_SLSO6_P20_10_OUT,
    IfxPort_OutputMode_pushPull,
    IfxPort_PadDriver_cmosAutomotiveSpeed3
};

#define SPI_BUFFER_SIZE 8

// qspi priorities
#define IFX_INTPRIO_QSPI0_TX  1 // DMA channel 1
#define IFX_INTPRIO_QSPI0_RX  2 // DMA channel 2
#define IFX_INTPRIO_QSPI0_ER  0x30

// dma priorities
#define IFX_INTPRIO_DMA_CH1  10
#define IFX_INTPRIO_DMA_CH2  11


IFX_INTERRUPT(qspi0DmaTxISR, 0, IFX_INTPRIO_DMA_CH1 )
{
    IfxQspi_SpiMaster_isrDmaTransmit(&spi);
}

IFX_INTERRUPT(qspi0DmaRxISR, 0, IFX_INTPRIO_DMA_CH2)
{
   IfxQspi_SpiMaster_isrDmaReceive(&spi);
}

IFX_INTERRUPT(qspi0ErISR, 0, IFX_INTPRIO_QSPI0_ER)
{
    IfxQspi_SpiMaster_isrError(&spi);

    // Process errors. Eg: parity Error is checked below
    /*IfxQspi_SpiMaster_Channel *chHandle   = IfxQspi_SpiMaster_activeChannel(&spi);
    if( chHandle->errorFlags.parityError == 1)
    {
        // Parity Error
    }*/
}

void qspi0mstr_Init_iLLD(void)
{
	if (g_qspi0IlldInitialized == true)
	{
		return;
	}

	/* Initialize Module */

	// create module config
	IfxQspi_SpiMaster_Config spiMasterConfig;
	IfxQspi_SpiMaster_initModuleConfig(&spiMasterConfig, &MODULE_QSPI0);

	// set the desired mode and maximum baudrate
	spiMasterConfig.base.mode             = SpiIf_Mode_master;
	spiMasterConfig.base.maximumBaudrate  = 10000000;

	// ISR priorities and interrupt target (with Dma usage)
	spiMasterConfig.base.txPriority       = IFX_INTPRIO_DMA_CH1;
	spiMasterConfig.base.rxPriority       = IFX_INTPRIO_DMA_CH2;
	spiMasterConfig.base.erPriority       = IFX_INTPRIO_QSPI0_ER;

	// dma configuration.
	spiMasterConfig.dma.txDmaChannelId = IfxDma_ChannelId_1;
	spiMasterConfig.dma.rxDmaChannelId = IfxDma_ChannelId_2;
	spiMasterConfig.dma.useDma = 1;


	// pin configuration
	const IfxQspi_SpiMaster_Pins pins = {
	    &IfxQspi0_SCLK_P20_11_OUT, IfxPort_OutputMode_pushPull, // SCLK
	    &IfxQspi0_MTSR_P20_14_OUT, IfxPort_OutputMode_pushPull, // MTSR
	    &IfxQspi0_MRSTA_P20_12_IN, IfxPort_InputMode_pullDown,  // MRST
	    IfxPort_PadDriver_cmosAutomotiveSpeed3 // pad driver mode
	};
	spiMasterConfig.pins = &pins;

	// initialize module
	IfxQspi_SpiMaster_initModule(&spi, &spiMasterConfig);


	/* Initialize SPI Channel */

	// create channel config
	IfxQspi_SpiMaster_initChannelConfig(&spiMasterChannelConfig, &spi);

	// set the baudrate for this channel
	spiMasterChannelConfig.base.baudrate = 1000000;

	// Choose CS6
	spiMasterChannelConfig.sls.output = slsOutput6;
	// initialize channel
	IfxQspi_SpiMaster_initChannel(&spiChannel, &spiMasterChannelConfig);

	// Choose CS2
	spiMasterChannelConfig.sls.output = slsOutput2;
	// initialize channel
	IfxQspi_SpiMaster_initChannel(&spiChannel, &spiMasterChannelConfig);

	g_qspi0IlldInitialized = true;
}

extern SpiIf_Status qspi0_send_receive_iLLD(QspiCs_t CS, uint16 u16Length, uint8* pu8SrcBuff, uint8* pu8DstBuff)
{

	SpiIf_Status retVal = SpiIf_Status_unknown;

	if(CS == eQspiHwCs02)
	{
		// Choose CS2
		spiMasterChannelConfig.sls.output = slsOutput2;
		// initialize channel
		IfxQspi_SpiMaster_initChannel(&spiChannel, &spiMasterChannelConfig);
	}
	else if(CS == eQspiHwCs06)
	{
		// Choose CS6
		spiMasterChannelConfig.sls.output = slsOutput6;
		// initialize channel
		IfxQspi_SpiMaster_initChannel(&spiChannel, &spiMasterChannelConfig);
	}

	// send/receive new stream
	retVal = IfxQspi_SpiMaster_exchange(&spiChannel, pu8SrcBuff, pu8DstBuff, u16Length);

	return retVal;
}

/*
 *******************************************************************************
 *
 * @author  AA
 * @date    Jan2021 (created)
 * @brief   Function: qspimstr_waitForRxDone()
 *			Waiting for the SPI buffer is empty
 * @param  	None
 * @retval	None
 *
  ******************************************************************************/
bool qspimstr_waitForRxDone_iLLD( void )
{
	while( IfxQspi_SpiMaster_getStatus(&spiChannel) != SpiIf_Status_ok );
	return true;
}
