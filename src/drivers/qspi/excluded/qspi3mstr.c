/**
 ******************************************************************************
 * @file    qspi3mstr.c
 * @author	rmilne
 * @version V0.1.0
 * @date    Jul2020
 * @brief	Configures QSPI3 as a master in single move mode for control of
 *			TLF35584 Multi Voltage Safety Micro Processor Supply chip..
 *
 * From TC37xAA_DS_v0.71.pdf:
 *	P01.4  	QSPI3_SLSO10	[O4]
 *	P01.5 	QSPI3_MRSTC		[I]
 *	P01.6 	QSPI3_MTSR		[O4]
 *	P01.7 	QSPI3_SCLK		[O4]
 *
 * This SPI interface uses DMA interrupts.
 *
 * NOTE1: Clock register settings for 100MHz fPER clock.
 * fOSC = 20MHz
 * PERPLLCON0.DIVBY = 0
 * PERPLLCON0.NDIV = 31
 * PERPLLCON0.PDIV = 0
 * PERPLLCON1.K3DIV = 1
 * if DIVBY == 0 then fPLL2 = (N x fOSC)/(P x K3 x 1.6)
 * N = PERPLLCON0.NDIV + 1 = 32
 * P = PERPLLCON0.PDIV + 1 = 1
 * K3 = PERPLLCON1.K3DIV + 1 = 2
 * fPLL2 = 200MHz
 * CCUCON0.CLKSEL = 1 (fPPL2 is used as clock source for fSOURCE2)
 * CCUCON1.CLKSELQSPI = 2 (fSOURCE2 is clock source for fSOURCEQSPI)
 * CCUCON1.QSPIDIV = 2 (fQSPI = fSOURCEQSPI / 2)
 * fPER = fQSPI = 100Mhz
 * GLOBALCON.CLKSEL = 1 (clock enable)
 * GLOBALCON.TQ = 0 (Global Time Quantum = fPER/(GLOBALCON.TQ+1) = 100 Mhz)
 *
 * NOTE2: CS (SLSn) active level can be configured as active high or active
 * low on a per-channel basis, that is channel CS active levels can be
 * individually configured.
 *
 * NOTE3: SCLK BAUD calculation.
 * The baud rate depends on fPER as well as as a common divider (GLOBALCON.TQ)
 * and per channel clock configuration quantums (ECON[n].Q/A/B/C).  The per
 * channel timing segments of ECON permit different baud rates for mod8 slave
 * selects.
 * If ECON[n].B=0 and ECON[n].C=0, then ECON[n].C=1 (forced per hardware).
 * (Q + 1) * ((A + 1) + B + C) must be 4 or greater.
 *
 * 	 									fPER
 * BR[n] = ____________________________________________________________________
 *         (GLOBALCON.TQ+1)(ECON[n].Q+1)((ECON[n].A+1) + ECON[n].B + ECON[n].C)
 *
 * NOTE4: SCLK duty cycle depends on ECON[n] quantum values
 *
 * 		  A+1   B+C   A+1   B+C   A+1   B+C
 * ------     -------     -------     -------
 *      |     |     |     |     |     |     |     ......
 *      -------     -------     -------     ------
 *
 * Therefore if one desires a 50% duty cycle (A+1) = (B+C)
 * Example: ECON[n].A=1 ECON[n].B=1 ECON[n].C=1
 *
 * NOTE5: Each QSPI can support up to 16 channels (CS) lines.  Each of the
 * channels can be uniquely configured via the ECON[n] where n applies to
 * the chip select.  Each ECON register supports chip selects n and n+8
 * n = 0 : SLS0/SLS8
 * n = 1 : SLS1/SLS9
 * n = 2 : SLS2/SLS10
 * n = 3 : SLS3/SLS11
 * n = 4 : SLS4/SLS12
 * n = 5 : SLS5/SLS13
 * n = 6 : SLS6/SLS14
 * n = 7 : SLS7/SLS15
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2020 Neutron Controls.</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS for
 * the sole purpose of a reference design, WITHOUT WARRANTIES OR CONDITIONS OF
 * ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
 *
 ******************************************************************************
 */


/*----- Includes -------------------------------------------------------------*/
/* C includes */
#include <stdint.h>
/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Scu/Std/IfxScuWdt.h"
#include "Qspi/Std/IfxQspi.h"
#include "Qspi/SpiMaster/IfxQspi_SpiMaster.h"
#include "aurix_pin_mappings.h"
#include "machine.h"
/* Application includes */
#if (__RL_SHELL == 1)
#include "shell.h"
#endif /* (__RL_SHELL == 1) */
#include "qspi3mstr.h"


/*----- Private Types --------------------------------------------------------*/


/** \brief QSPI control structure
 */
typedef struct QSPIMSTR_Cfg_s
{
	Ifx_QSPI* 			lpQspiModule;		/**< \brief Pointer to peripheral register space */

	uint8_t				pisel_mris;			/**< \brief Master Mode Receive Input Select */
	uint16_t 			u16SlaveListSize;	/**< \brief Number of slaves */
	QSPI_Slave_Ctl_t* 	lpSlaveList;		/**< \brief Pointer to slave descriptors */
	QspiCs_t 			active_slave;		/**< \brief Index to slave list of active slave */

	Ifx_DMA_CH_CHCFGR	txDmaChannelCfg;	/**< \brief DMARAM Channel Configuration Register for tx channel */
	Ifx_DMA_CH_CHCFGR	rxDmaChannelCfg;	/**< \brief DMARAM Channel Configuration Register for rx channel */
	Ifx_DMA_CH_ADICR 	txDmaAddrIntCtl;	/**< \brief DMARAM Channel Address and Interrupt Control Register for tx channel */
	Ifx_DMA_CH_ADICR 	rxDmaAddrIntCtl;	/**< \brief DMARAM Channel Address and Interrupt Control Register for rx channel */

	QSPI_SrcSrcr_t		srcrTxd;			/**< \brief SRC structure for QSPI tx ISR */
	QSPI_SrcSrcr_t		srcrRxd;			/**< \brief SRC structure for QSPI rx ISR */
	QSPI_SrcSrcr_t		srcrErr;			/**< \brief SRC structure for QSPI error ISR */
	QSPI_SrcSrcr_t		srcrDmaTxCh;		/**< \brief SRC structure for DMA tx ISR */
	QSPI_SrcSrcr_t		srcrDmaRxCh;		/**< \brief SRC structure for DMA rx ISR */

} QSPI3MSTR_Cfg_t;


/*----- Protected Variables --------------------------------------------------*/


/*----- Externals ------------------------------------------------------------*/


/*----- Macros ---------------------------------------------------------------*/
#define QSPI3_DMA_TX_CHANNEL 	(IfxDma_ChannelId_3)
#define QSPI3_DMA_RX_CHANNEL 	(IfxDma_ChannelId_4)


/*----- Prototypes -----------------------------------------------------------*/
#define QSPI3MASTER_DEBUG_PRINT 1

#if (QSPI3MASTER_DEBUG_PRINT == 1) && (__RL_SHELL == 1)
#define QSPI3M_DBG_WRITE(...) PRINTF("%sQSPI3 ", shell_GetCpu()); PRINTF(__VA_ARGS__)
#else
#define QSPI3M_DBG_WRITE(...)
#endif /* (QSPI3MASTER_DEBUG_PRINT == 1) && (__RL_SHELL == 1) */

#define kuQSPIMASTER_SLS_CS_ACTIVE_LOW									FALSE
#define kuQSPIMASTER_SLS_CS_ACTIVE_HIGH									TRUE

/* Number of CS */
#define kuQSPIMASTER_QSPI3_NUM_SLAVES									(1u)


/*----- Static member variables ----------------------------------------------*/
#pragma section fardata "dlmucpu0.nc_ram"		/**< \brief Initialized non cached data section (LMU) */
__far volatile uint32_t qspi3mstrEvent __align(4) = (0u);	/**< \brief event semaphores for synchronization */
#pragma section fardata restore

boolean bQspi3IsInit = FALSE;

/**< \brief Slave (CS) control structure */
QSPI_Slave_Ctl_t qspi3Slaves[kuQSPIMASTER_QSPI3_NUM_SLAVES] __align(4) =
{
	{
		/* SLSO10, P01.4 (OUTPUT) Active LOW (IfxQspi3_SLSO10_P01_4_OUT) */
		.port.lpPort = &MODULE_P01,
		.port.u8Pin = (4u),
		.u16Cs = TLF35584_SLSO,	/* LTC35584 slave select is SLSO10 (P01.4) */
		.u16ActiveLevel = kuQSPIMASTER_SLS_CS_ACTIVE_LOW,

		/* ECON Register configuration for TLF35584 slave select
		 * fPER = 100MHz (see Note1)
		 * GLOBALCON.TQ = 9 (set in qspi3mstr_Init())
		 * BaudRate = fPER / (GLOBALCON.TQ + 1) * (Q + 1) * ((A + 1) + B + C) = 1 MHz
		 */
		.econ.B.Q = 1,		/* Time Quantum */
		.econ.B.A = 2,		/* Bit Segment 1 */
		.econ.B.B = 0,  	/* Bit Segment 2 */
		.econ.B.C = 2,  	/* Bit Segment 3 */
		.econ.B.CPH = 0,	/* Clock phase */
		.econ.B.CPOL = 0,	/* Clock polarity */
		.econ.B.PAREN = 1,	/* Parity check */
		.econ.B.BE = 0,		/* Big Endian */

		.tx.remaining = 0,
		.tx.data = NULL,
		.rx.remaining = 0,
		.rx.data = NULL
	},
};

/**< \brief QSPI3 configuration */
QSPI3MSTR_Cfg_t qspi3mstrCfg __align(4) =
{
	.lpQspiModule = &MODULE_QSPI3,
	.pisel_mris = (0xc - 0xa), /* MRST input line C */
	/* Slave Select Control Array */
	.u16SlaveListSize = kuQSPIMASTER_QSPI3_NUM_SLAVES,
	.lpSlaveList = qspi3Slaves,
	.active_slave = 0x00,

	/* QSPI SRC controls
	 */
	.srcrTxd.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.QSPI.QSPI[3].TX),
	.srcrTxd.u16Srpn = ((uint16_t)QSPI3_DMA_TX_CHANNEL),
	.srcrTxd.tos = IfxSrc_Tos_dma,
	.srcrRxd.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.QSPI.QSPI[3].RX),
	.srcrRxd.u16Srpn = ((uint16_t)QSPI3_DMA_RX_CHANNEL),
	.srcrRxd.tos = IfxSrc_Tos_dma,
	.srcrErr.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.QSPI.QSPI[3].ERR),
	.srcrErr.u16Srpn = ((uint16_t)QSPI3_ERR_ISR_PRIORITY),
	.srcrErr.tos = QSPI3_CPU_TOS,

	/* DMA SRC controls
	 * NB: DMA tx/rx SRCs use CPU priorities and type of service.
	 */
	.srcrDmaTxCh.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.DMA.DMA[0].CH[QSPI3_DMA_TX_CHANNEL]),
	.srcrDmaTxCh.u16Srpn = ((uint16_t)QSPI3_TxD_ISR_PRIORITY),
	.srcrDmaTxCh.tos = QSPI3_CPU_TOS,
	.srcrDmaRxCh.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.DMA.DMA[0].CH[QSPI3_DMA_RX_CHANNEL]),
	.srcrDmaRxCh.u16Srpn = ((uint16_t)QSPI3_RxD_ISR_PRIORITY),
	.srcrDmaRxCh.tos = QSPI3_CPU_TOS,

	/* DMARAM Channel c Configuration Register for tx channel
	 * B.TREL	= 0	// Transfer Reload Value
	 * B.BLKM	= 0	// Block Mode = One DMA transfer has 1 DMA move
	 * B.RROAT	= 0	// Reset Request Only After Transaction = channel TSR.CH is reset after each transfer
	 * B.CHMODE	= 0	// Channel Operation Mode = single
	 * B.CHDW	= 0	// Channel Data Width = 8-bit
	 * B.PATSEL = 0	// Pattern Select = none
	 * B.SWAP	= 0	// Swap Data CRC Byte Order = none
	 * B.PRSEL	= 0	// Peripheral Request Select = DMA hardware request selected
	 */
	.txDmaChannelCfg.U = 0,
	.txDmaChannelCfg.B.CHDW = 1u,

	/* DMARAM Channel c Configuration Register for rx channel
	 * B.TREL	= 0	// Transfer Reload Value
	 * B.BLKM	= 0	// Block Mode = One DMA transfer has 1 DMA move
	 * B.RROAT	= 0	// Reset Request Only After Transaction = channel TSR.CH is reset after each transfer
	 * B.CHMODE	= 0	// Channel Operation Mode = single
	 * B.CHDW	= 0	// Channel Data Width = 8-bit
	 * B.PATSEL = 0	// Pattern Select = none
	 * B.SWAP	= 0	// Swap Data CRC Byte Order = none
	 * B.PRSEL	= 0	// Peripheral Request Select = DMA hardware request selected
	 */
	.rxDmaChannelCfg.U = 0,
	.rxDmaChannelCfg.B.CHDW = 1u,


	/* DMARAM Channel c Address and Interrupt Control Register for tx channel
	 * B.SMF   = 0	// Source Address Modification Factor = 1 x CHDW
	 * B.INCS  = 1 	// Increment of Source Address = added
	 * B.DMF   = 0 	// Destination Address Modification Factor = 1 x CHDW
	 * B.INCD  = 1 	// Increment of Destination Address = added
	 * B.CBLS  = 0 	// Circular Buffer Length Source = SADR is not updated
	 * B.CBLD  = 0 	// Circular Buffer Length Destination = DADR is not updated
	 * B.SHCT  = 0 	// Shadow Control = Move Operation
	 * B.SCBE  = 0 	// Source Circular Buffer Enable = disabled
	 * B.DCBE  = 1 	// Destination Circular Buffer Enable = enabled
	 * B.STAMP = 0	// Time Stamp = No action
	 * B.WRPSE = 0 	// Wrap Source Enable = Wrap source buffer interrupt trigger disabled
	 * B.WRPDE = 0 	// Wrap Destination Enable = Wrap destination buffer interrupt trigger disabled
	 * B.INTCT = 2 	// Interrupt Control = Interrupt generated and CHSR.ICH set when CHSR.TCOUNT value equals IRDV
	 * B.IRDV  = 0 	// Interrupt Raise Detect Value
	 */
	.txDmaAddrIntCtl.U = 0x08200088,

	/* DMARAM Channel c Address and Interrupt Control Register for rx channel
	 * B.SMF   = 0	// Source Address Modification Factor = 1 x CHDW
	 * B.INCS  = 1	// Increment of Source Address = added
	 * B.DMF   = 0	// Destination Address Modification Factor = 1 x CHDW
	 * B.INCD  = 1	// Increment of Destination Address = added
	 * B.CBLS  = 0	// Circular Buffer Length Source = SADR is not updated
	 * B.CBLD  = 0	// Circular Buffer Length Destination = DADR is not updated
	 * B.SHCT  = 0	// Shadow Control = Move Operation
	 * B.SCBE  = 1	// Source Circular Buffer Enable = enabled
	 * B.DCBE  = 0 	// Destination Circular Buffer Enable = disabled
	 * B.STAMP = 0 	// Time Stamp = No action
	 * B.WRPSE = 0 	// Wrap Source Enable = Wrap source buffer interrupt trigger disabled
	 * B.WRPDE = 0 	// Wrap Destination Enable = Wrap destination buffer interrupt trigger disabled
	 * B.INTCT = 2 	// Interrupt Control = Interrupt generated and CHSR.ICH set when CHSR.TCOUNT value equals IRDV
	 * B.IRDV  = 0 	// Interrupt Raise Detect Value
	 */
	.rxDmaAddrIntCtl.U = 0x08100088

};

/*----- Constant member variables --------------------------------------------*/
/* TPER = 1 / fPER = 10 nsec
 * This driver uses continuous mode since the number of data BYTES to send is
 * unknown => BACON.B.LAST = 0 */
const static Ifx_QSPI_BACON qspiBacon __align(4) =
{
	.B.BYTE = 0u,				/* [0|1] = [bits|bytes] data length (DL field) */
	.B.LAST = 0u,				/* [0|1] = [not last|last] word in frame */
	.B.DL = 14u, 				/* 14 bits per data block */
	.B.CS = TLF35584_SLSO,		/* Chip Select channel */
	.B.MSB = 1u,				/* Shift out Most Significant Bit first */
	/*
	 * 		TIDLEa,b = TPER * 4^IPRE * (IDLE+1) = 10 nsec * 16 * 4 = 640 nsec
	 */
	.B.IPRE = 2u,
	.B.IDLE = 3u,
	/*
	 * TLEAD = TPER * 4^LPRE * (LEAD+1) = 10 nsec * 16 * 2 =  320 nsec
	 */
	.B.LPRE = 2u,
	.B.LEAD = 1u,
	/*
	 * TTRAIL = TPER * 4^TPRE * (TRAIL+1) = 10 nsec * 4 * 8 = 320 nsec
	 */
	.B.TPRE = 1u,
	.B.TRAIL = 7u
};

/*----- Constant member variables --------------------------------------------*/


/*----- Private Functions ----------------------------------------------------*/

/**
 *******************************************************************************
 *
 * @author  RM
 * 			RG
 * @date    Mar2021 (created)
 * @brief   qspi3DmaTxISR
 *          DMA tx channel interrupt handler.
 *          Since the tx channel ADICR.INTCT = b10, the tx channel CHSR.ICH is
 *          set when CHSR.TCOUNT decrements to ADICR.IRDV (= zero).
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
__private2 __far static volatile uint8_t ray_tx = 0;
__private2 __far static volatile uint8_t ray_rx = 0;
IFX_INTERRUPT(qsp3DmaTxISR, QSPI3_CPU_VEC_TBL, QSPI3_TxD_ISR_PRIORITY)
{
    Ifx_DMA* pDma = &MODULE_DMA;


    /* Test if DMA tx channel interrupt is set */
    if (pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	ray_tx++;
    	QSPI3MSTR_Cfg_t* lpCntrl = &qspi3mstrCfg;
    	/* Slave control structure determines pin configuration for CS deactivation */
    	QSPI_Slave_Ctl_t* pSlave  = &lpCntrl->lpSlaveList[lpCntrl->active_slave];
        Ifx_QSPI_BACON    bacon;


        /* Set DMA tx channel CICH bit to clear interrupt */
        pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCSR.B.CICH = 1;
        /* Disable DMA tx channel transaction */
        pDma->TSR[QSPI3_DMA_TX_CHANNEL].B.DCH = 1;

        /* Write BACON to QSPI tx FIFO to signal last byte of transfer */
        bacon.U = qspiBacon.U;
        bacon.B.LAST = 1;
        lpCntrl->lpQspiModule->BACONENTRY.U = bacon.U;

        /* Write last data byte to QSPI tx FIFO */
        lpCntrl->lpQspiModule->DATAENTRY[pSlave->u16Cs%8].U = ((uint8 *)pSlave->tx.data)[pSlave->tx.remaining - 1];
    }
}

/**
 *******************************************************************************
 *
 * @author  RM
 * 			RG
 * @date    Mar2021 (created)
 * @brief   qspi3DmaRxISR
 *          DMA rx channel interrupt handler
 *          Since the rx channel ADICR.INTCT = b10, the rx channel CHSR.ICH is
 *          set when CHSR.TCOUNT decrements to ADICR.IRDV (= zero).
 *          A global notification bit is set for synchronization.
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
IFX_INTERRUPT(qspi3DmaRxISR, QSPI3_CPU_VEC_TBL, QSPI3_RxD_ISR_PRIORITY)
{
    Ifx_DMA* pDma = &MODULE_DMA;

    /* Test if DMA rx channel interrupt is set */
    if (pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	QSPI3MSTR_Cfg_t* lpCntrl = &qspi3mstrCfg;
    	QSPI_Slave_Ctl_t* pSlave  = &lpCntrl->lpSlaveList[lpCntrl->active_slave];

    	ray_rx++;
    	/* Set DMA rx channel CICH bit to clear interrupt */
    	pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCSR.B.CICH = 1;

    	/* Deactivate slave */
		IfxPort_State action = (pSlave->u16ActiveLevel == kuQSPIMASTER_SLS_CS_ACTIVE_LOW) ? IfxPort_State_high : IfxPort_State_low;
		IfxPort_setPinState(pSlave->port.lpPort, pSlave->port.u8Pin, action);

		/* Set notification bit for QSPI rx completion */
		sem_Set(&qspi3mstrEvent, kuQSPI3_RX_EVENT);
    }
}


/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    Jul2020
 * @brief   qspi3ErISR
 *          QSPI3 error interrupt handler
 *          Copies sticky error flags to the slave control struct, clears the
 *          flags and sets the global notification bit used for synchronization.
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
IFX_INTERRUPT(qspi3ErISR, QSPI3_CPU_VEC_TBL, QSPI3_ERR_ISR_PRIORITY)
{
	Ifx_DMA*			pDma	= &MODULE_DMA;
	QSPI3MSTR_Cfg_t* 	lpCntrl	= &qspi3mstrCfg;
    QSPI_Slave_Ctl_t* 	pSlave;


    /* Read error flags */
    pSlave = &lpCntrl->lpSlaveList[lpCntrl->active_slave];
	pSlave->errorFlags = lpCntrl->lpQspiModule->STATUS.B.ERRORFLAGS;
	/* Clear error flags */
	lpCntrl->lpQspiModule->FLAGSCLEAR.U = 0xFFFFU;

	/* Deactivate slave */
	IfxPort_State action = (pSlave->u16ActiveLevel == kuQSPIMASTER_SLS_CS_ACTIVE_LOW) ? IfxPort_State_high : IfxPort_State_low;
	IfxPort_setPinState(pSlave->port.lpPort, pSlave->port.u8Pin, action);

	/* Set notification bit for QSPI error detection */
	sem_Set(&qspi3mstrEvent, kuQSPI3_ERR_EVENT);

	/* Test DMA channel interrupts */
    if (pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	/* Set DMA tx channel CICH bit to clear interrupt */
    	pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCSR.B.CICH = 1;
    }
    if (pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	/* Set DMA rx channel CICH bit to clear interrupt */
    	pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCSR.B.CICH = 1;
    }
}


/*
 *******************************************************************************
 *
 * @author  rmilne
 * @date    Jul2020
 * @brief   Initialize/Deinitialize QSPI peripheral for asynchronous DMA
 *			operation.
 *
 *	The Type of Service (TOS) of the QSPI RX/TX service request nodes are set
 *	to DMA. The QSPI is configured for MIXENTRY to enable a change of the basic
 *	configuration BACON within the same data sequence as the command messages.
 *	In this way, the requirements regarding the delay between messages can be
 *	easily met by changing the idle phase TIDLEA,B.
 *	Note that the DMA configuration is not in the initialization but in the
 *	service routine.
 *
 * @param   bState
 * 			TRUE = Enable the QSPI module as configured by lpCntrl.
 * 			FALSE = Disable and reset QSPI module.
 * @retval  0 = Successful; < 0 = failure.
 *
 ******************************************************************************/
inline int32_t qspi3mstr_Init(boolean bState)
{
	QSPI3MSTR_Cfg_t* lpCntrl = &qspi3mstrCfg;
	int32_t i32Rtn = -1;
	uint16_t u16Psw = IfxScuWdt_getCpuWatchdogPassword();
	volatile uint32_t u32Reg;
	void* lpReg;
	Ifx_SRC_SRCR isr;
	boolean bInterruptState;
	Ifx_DMA* pDma = &MODULE_DMA;


	if((bState == TRUE) && (bQspi3IsInit == FALSE))
	{/* Request to ENABLE the QSPI module */
		/* Clear ENDINIT protection */
		IfxScuWdt_clearCpuEndinit(u16Psw);
		/* Enable module clock and disable sleep mode (see Clock Control Register) */
		lpCntrl->lpQspiModule->CLC.U = 0x8u;
		(void)lpCntrl->lpQspiModule->CLC.U;
		/* Restore ENDINIT protection */
		IfxScuWdt_setCpuEndinit(u16Psw);


		/* Write Master Mode Receive Input Select */

		/* Configure operational parameters */
		/* GLOBALCON (Global Configuration Register) */
		u32Reg = 0u;
		lpReg = ((uint32_t*)&u32Reg);
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.TQ = 9u;		/* Global Time Quantum = fPER/B.TQ+1 (10 Mhz)*/
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.SI = 0u;		/* Disable status injection */
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.EXPECT = 15u; 	/* Longest timeout */
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.EN = 0u;		/* ENSURE in PAUSE MODE */
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.MS = 0u;		/* MASTER mode */
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.CLKSEL = 1u;	/* fPER clock source (fSOURCE2/QSPIDIV) */
		/*
		 * Switching the peripheral clock off and on is done by using two
		 * writes: first a write of zero to the CLKSEL bit field, and then
		 * a second write switching on the clock source. Between the first
		 * and the second write a delay of minimum 4 * (1/fPER) + 2 * (1/fCLC)
		 * must be inserted by software, where fPER is the frequency being
		 * switched off with the first write. This also applies if between
		 * the writes the clock frequency is changed in the CCU.
		 *
		 * This driver doesn't perform fast on-off switching
		 */
		lpCntrl->lpQspiModule->GLOBALCON.U = u32Reg;

		/* GLOBALCON1 (Global Configuration Register 1) */
		u32Reg = 0u;
		lpReg = ((uint32_t*)&u32Reg);
		((Ifx_QSPI_GLOBALCON1*)lpReg)->B.ERRORENS = (uint32)0x1FF;	/* ERROR Interrupt (ENABLED) */
		((Ifx_QSPI_GLOBALCON1*)lpReg)->B.TXEN = 1u;		/* TxD Interrupt (ENABLED) */
		((Ifx_QSPI_GLOBALCON1*)lpReg)->B.RXEN = 1u;		/* RxD Interrupt (ENABLED) */
		((Ifx_QSPI_GLOBALCON1*)lpReg)->B.RXFM = 1;
		((Ifx_QSPI_GLOBALCON1*)lpReg)->B.TXFM = 1;
		lpCntrl->lpQspiModule->GLOBALCON1.U = u32Reg;


		/* Build the SSOC register and configure each ECON register for all
		 * slave selects */
		u32Reg = 0u;
		lpReg = ((uint32_t*)&u32Reg);
		for(uint16_t u16Index = 0; u16Index < lpCntrl->u16SlaveListSize; u16Index++)
		{
			int econ_idx;
			uint16_t ssoc_field = 1 << lpCntrl->lpSlaveList[u16Index].u16Cs;


			/* Build OEN and AOL bitmaps for SSOC register */
			((Ifx_QSPI_SSOC*)lpReg)->B.OEN |= ssoc_field;	/* Enables SLS CS output */
			if(lpCntrl->lpSlaveList[u16Index].u16ActiveLevel == kuQSPIMASTER_SLS_CS_ACTIVE_LOW)
			{
				((Ifx_QSPI_SSOC*)lpReg)->B.AOL &= ~ssoc_field;	/* Sets CS to active LOW */
			}
			else
			{
				((Ifx_QSPI_SSOC*)lpReg)->B.AOL |= ssoc_field;	/* Sets CS to active HIGH */
			}

			/* Set ECON register index (8 registers for 16 slave selects) */
			econ_idx = lpCntrl->lpSlaveList[u16Index].u16Cs % 8;

			lpCntrl->lpQspiModule->ECON[econ_idx].U = lpCntrl->lpSlaveList[u16Index].econ.U;
		}
		/* Write Slave Select Output Control register */
		lpCntrl->lpQspiModule->SSOC.U = u32Reg;
		/* Write Master Mode Receive Input Select */
		lpCntrl->lpQspiModule->PISEL.U = lpCntrl->pisel_mris;

		/* Clean-up the RXEXIT */
		while (lpCntrl->lpQspiModule->STATUS.B.RXFIFOLEVEL > 0)
		{
			lpCntrl->lpQspiModule->RXEXIT.U;
		}

		/* Disable DMA Channel Hardware Transaction Requests */
		pDma->TSR[QSPI3_DMA_TX_CHANNEL].U = 0x00020000;
		pDma->TSR[QSPI3_DMA_RX_CHANNEL].U = 0x00020000;

		/* Initialize DMA tx channel */
		pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCFGR.U = lpCntrl->txDmaChannelCfg.U;
		pDma->CH[QSPI3_DMA_TX_CHANNEL].ADICR.U = lpCntrl->txDmaAddrIntCtl.U;

		/* Initialize DMA rx channel */
		pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCFGR.U = lpCntrl->rxDmaChannelCfg.U;
		pDma->CH[QSPI3_DMA_RX_CHANNEL].ADICR.U = lpCntrl->rxDmaAddrIntCtl.U;

		/* Set ISR service requests */
		/* Disable interrupts and store current interrupt state */
		bInterruptState = IfxCpu_disableInterrupts();
		/* Set DMAc Tx service request; Service request number, CPU services request, Service request enabled */
		if(lpCntrl->srcrDmaTxCh.pSRC_SRCR != ((void*)0))
		{
			isr.U = 0u;

			isr.B.SRPN = lpCntrl->srcrDmaTxCh.u16Srpn;	/* QSPI tx Service Request Priority Number */
			isr.B.SRE = 1u;								/* Enable Service Request */

			isr.B.TOS = lpCntrl->srcrDmaTxCh.tos;		/* Service Request Assigned to CPU */

			lpCntrl->srcrDmaTxCh.pSRC_SRCR->U = isr.U;
		}
		/* Set DMAc Rx service request; Service request number, CPU services request, Service request enabled */
		if(lpCntrl->srcrDmaRxCh.pSRC_SRCR != ((void*)0))
		{
			isr.U = 0u;

			isr.B.SRPN = lpCntrl->srcrDmaRxCh.u16Srpn;	/* QSPI rx Service Request Priority Number */
			isr.B.SRE = 1u;								/* Enable Service Request */
			isr.B.TOS = lpCntrl->srcrDmaRxCh.tos;		/* Service Request Assigned to CPU */

			lpCntrl->srcrDmaRxCh.pSRC_SRCR->U = isr.U;
		}

		/* Set QSPIn TxD service request; Service request number, DMA services request, Service request enabled */
		if(lpCntrl->srcrTxd.pSRC_SRCR != ((void*)0))
		{
			isr.U = 0u;

			isr.B.SRPN = lpCntrl->srcrTxd.u16Srpn;	/* DMA Channel # is priority */
			isr.B.SRE = 1u;							/* Enable Service Request */
			isr.B.TOS = lpCntrl->srcrTxd.tos;		/* Service Request Assigned to DMA */

			lpCntrl->srcrTxd.pSRC_SRCR->U = isr.U;
		}
		/* Set QSPIn RxD service request; Service request number, DMA services request, Service request enabled */
		if(lpCntrl->srcrRxd.pSRC_SRCR != ((void*)0))
		{
			isr.U = 0u;

			isr.B.SRPN = lpCntrl->srcrRxd.u16Srpn;	/* DMA Channel # is priority */
			isr.B.SRE = 1u;							/* Enable Service Request */
			isr.B.TOS = lpCntrl->srcrRxd.tos;		/* Service Request Assigned to DMA */

			lpCntrl->srcrRxd.pSRC_SRCR->U = isr.U;
		}
		/* Set QSPIn Err service request; Service request number, CPU services request, Service request enabled */
		if(lpCntrl->srcrErr.pSRC_SRCR != ((void*)0))
		{
			isr.U = 0u;

			isr.B.SRPN = lpCntrl->srcrErr.u16Srpn;	/* QSPI error Service Request Priority Number */
			isr.B.SRE = 1u;									/* Enable Service Request */
			isr.B.TOS = lpCntrl->srcrErr.tos;		/* Service Request Assigned to CPU */

			lpCntrl->srcrErr.pSRC_SRCR->U = isr.U;
		}

		/* Restore interrupt state */
		IfxCpu_restoreInterrupts(bInterruptState);

		/* Clear all QSPI Flags */
		do{
			lpCntrl->lpQspiModule->FLAGSCLEAR.U = 0xFFF;
		} while(lpCntrl->lpQspiModule->STATUS.U & 0xFFF);

		/* Finally enable the module */
		lpCntrl->lpQspiModule->GLOBALCON.B.EN = 1u;

		bQspi3IsInit = TRUE;
		i32Rtn = 0;

	}
	else if((bState == FALSE) && (bQspi3IsInit == TRUE))
	{/* Request to DISABLE the QSPI module */
		/* Wait for current DMA transaction to complete */
		while (lpCntrl->lpQspiModule->STATUS.B.PHASE != 0)
		{}

		/* Disable DMA Channel Hardware Transaction Requests */
		pDma->TSR[QSPI3_DMA_TX_CHANNEL].U = 0x00020000;
		pDma->TSR[QSPI3_DMA_RX_CHANNEL].U = 0x00020000;

		/* Clear channel Interrupt controls */
		pDma->CH[QSPI3_DMA_TX_CHANNEL].ADICR.U = 0u;
		pDma->CH[QSPI3_DMA_RX_CHANNEL].ADICR.U = 0u;

		/* Restore registers to defaults */
		lpCntrl->lpQspiModule->ECON[0].U = 0x001450u;
		lpCntrl->lpQspiModule->GLOBALCON.U = 0x000F30FFu;
		lpCntrl->lpQspiModule->GLOBALCON1.U = 0x00050000u;

		/* Disable module clock and enable sleep mode (DEFAULT see Clock Control Register) */
		/* Clear ENDINIT protection */
		IfxScuWdt_clearCpuEndinit(u16Psw);
		lpCntrl->lpQspiModule->CLC.U = 0x1u;
		(void)lpCntrl->lpQspiModule->CLC.U;
		/* Restore ENDINIT protection */
		IfxScuWdt_setCpuEndinit(u16Psw);

		bQspi3IsInit = FALSE;
		i32Rtn = 0;
	}

	return(i32Rtn);
} /* END qspi3mstr_Init() */


/*----- Protected Functions --------------------------------------------------*/


/*----- Public Functions -----------------------------------------------------*/
/*
 *******************************************************************************
 *
 * @author  JW
 * @date    Dec2018 (created)
 * @author  RM
 * @date    May2020 (Add asynchronous DMA handling)
 * @brief   Send u16Length data bytes from pu8Buff using continuous data mode.
 *
 * section 37.3.5.1.3 of AURIXTC3XX_UM_part2_v1.4.pdf
 * Continuous Data Mode (short data):
 * In Continuous Data Mode, the QSPI transmits a stream of data with an
 * arbitrary length and an active slave select signal. It can be used with both
 * short and long data.  This mode starts by programming a control word
 * containing BACON.LAST = 0 and the first data. The communication continues
 * with the subsequent data entries written. The mode ends with a control word
 * containing BACON.LAST = 1 and the last data. If LAST = 0, then TRAIL is a
 * delay between data blocks. The W_I_L_D_T_D_T_D_T sequence appears. Each “D”
 * represents data block as defined with BACON.DL and BACON.BYTE (up to 256
 * bits). If LAST = 1 then TRAIL is trailing delay.
 *
 * As per figure 494 of section 37.3.5.1.3, transfers of data with a length
 * greater than one are sent via DMA except for the last BACON and DATA which
 * are sent in the DMA tx ISR.
 *
 * This function can use either hw or manual CS.  When using hw CS ensure this
 * feature is set up in the CS configuration structure.
 *
 * Completion of the operation is signaled by a qspi3mstrEvent bit set in the
 * DMA rx ISR.
 *
 * TODO: Develop FreeRTOS events and queueing ala the CAN driver
 *
 * NB: NON-Blocking function.
 *
 * @param	CS HW or manual Slave selection
 * @param  	u16Length The number of data bytes to send in the u8 data buffer
 * @param  	pu8SrcBuff Pointer to tx byte data buffer (static addressing).
 * @param	pu8DstBuff Pointer to rx byte data buffer (static addressing).
 * @retval 	The return status
 * 			-1 = TxD Timeout.
 * 			-2 = RxD timeout.
 * 			-3 = Invalid parameter
 * 			-4 = QSPI NOT initialized
 * 			Otherwise the number of bytes transferred.
 *
 ******************************************************************************/
int32_t qspi3mstr_contSend(QspiCs_t CS, uint16_t u16Length, uint16_t* pu8SrcBuff, uint16_t* pu8DstBuff)
{
	int32_t i32Rtn = 0;


	if((bQspi3IsInit == TRUE) && (pu8SrcBuff != NULL) &&
		(pu8DstBuff != NULL) && (u16Length > 0) && (CS == eQspiHwCs10))
	{
        Ifx_DMA				*pDma         	= &MODULE_DMA;
		QSPI3MSTR_Cfg_t*		lpCntrl			= &qspi3mstrCfg;
        boolean				interruptState 	= IfxCpu_disableInterrupts();
        Ifx_QSPI_BACON		bacon;
        QSPI_Slave_Ctl_t*	pSlave;
		uint16_t			u16RxFifoDepth;
		uint32_t			u32Scratch[2];


		pSlave = &lpCntrl->lpSlaveList[lpCntrl->active_slave];
        pSlave->tx.data = pu8SrcBuff;
        pSlave->tx.remaining = (int16_t)u16Length;
        pSlave->rx.data = pu8DstBuff;
        pSlave->rx.remaining = (int16_t)u16Length;
        bacon.U = qspiBacon.U;
		if(CS >= eQspiHwCsNone)
		{
			lpCntrl->lpQspiModule->SSOC.B.OEN = 0u;
		}
		else
		{
			lpCntrl->lpQspiModule->SSOC.B.OEN = 1u << CS;
			/* Set the SLSO signal to be activated and the
			 * corresponding ECON configuration extension */
			bacon.B.CS = CS;
		}

		/* Flush the RxFIFO */
		u16RxFifoDepth 	= lpCntrl->lpQspiModule->STATUS.B.RXFIFOLEVEL;
		for(uint16_t u16Index = 0u; u16Index < u16RxFifoDepth; u16Index++)
		{
			u32Scratch[0] = lpCntrl->lpQspiModule->RXEXIT.U;
		}
		/* Clear all flags */
		lpCntrl->lpQspiModule->FLAGSCLEAR.U = 0x00009FFFu;

		if (u16Length > 1)
		{
			/* DMA tx channel required only if data length greater than one.
			 * The DMA tx channel will move the first length-1 data bytes from
			 * the tx buffer to the QSPI tx FIFO and then trigger the DMA tx
			 * ISR to write the final BACON/DATA bytes to the QSPI tx FIFO.
			 * The length-1 value is loaded to the tx channel CHSR.TCOUNT for
			 * triggering the interrupt when decremented to zero (ADICR.IRDV).
			 */
			pDma->CH[QSPI3_DMA_TX_CHANNEL].ADICR.U = lpCntrl->txDmaAddrIntCtl.U;
			pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCFGR.U = lpCntrl->txDmaChannelCfg.U;
			pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCFGR.B.TREL = pSlave->tx.remaining - 1;
			/* Source of tx channel is tx buffer in slave control structure */
			pDma->CH[QSPI3_DMA_TX_CHANNEL].SADR.U = (uint32)((void *)IFXCPU_GLB_ADDR_DSPR(IfxCpu_getCoreId(), pSlave->tx.data));
			/* Destination of tx channel is QSPI tx FIFO */
			pDma->CH[QSPI3_DMA_TX_CHANNEL].DADR.U = (uint32)&lpCntrl->lpQspiModule->DATAENTRY[CS%8].U;
		}
		else
		{
			/* Disable DMA tx channel hardware transaction*/
			pDma->TSR[QSPI3_DMA_TX_CHANNEL].U = 0x00020000;
		}

		/* The full data length value is loaded to the rx channel CHSR.TCOUNT.
		 * The DMA rx ISR is triggered when the DMA rx channel has moved all
		 * expected data from the QSPI rx FIFO to the rx buffer.  The DMA rx
		 * ISR is responsible for deactivating the CS and event notification.
		 * A timeout while waiting for rx data will trigger the QSPI error ISR.
		 */
		pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCFGR.U = lpCntrl->rxDmaChannelCfg.U;
		pDma->CH[QSPI3_DMA_RX_CHANNEL].ADICR.U = lpCntrl->rxDmaAddrIntCtl.U;
        pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCFGR.B.TREL = u16Length;
        /* Source of rx channel is QSPI rx FIFO */
        pDma->CH[QSPI3_DMA_RX_CHANNEL].SADR.U =(uint32)&lpCntrl->lpQspiModule->RXEXIT.U;
        /* Destination of rx channel is rx buffer in slave control structure */
        pDma->CH[QSPI3_DMA_RX_CHANNEL].DADR.U = (uint32)((void *)IFXCPU_GLB_ADDR_DSPR(IfxCpu_getCoreId(), pSlave->rx.data));

        /* Clear QSPI error flags */
        lpCntrl->lpQspiModule->FLAGSCLEAR.U = 0x00009FFFu;
        /* Clear QSPI service requests */
        lpCntrl->srcrTxd.pSRC_SRCR->B.CLRR = 1;
        lpCntrl->srcrRxd.pSRC_SRCR->B.CLRR = 1;
        lpCntrl->srcrErr.pSRC_SRCR->B.CLRR = 1;
        /* Clear DMA rx channel interrupt flag */
        pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCSR.B.CICH = 1;

        /* Enable DMA hardware transaction for rx channel */
        pDma->TSR[QSPI3_DMA_RX_CHANNEL].B.ECH = 1;

        qspi3mstrEvent = 0;
        if (u16Length > 1)
        {
        	/* Clear DMA tx channel interrupt flag */
        	pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCSR.B.CICH = 1;
            /* Enable DMA hardware transaction for tx channel */
        	pDma->TSR[QSPI3_DMA_TX_CHANNEL].B.ECH = 1;
            /* Start the tx process by writing the initial BACON (LAST=0) to
             * the QSPI tx FIFO. */
            lpCntrl->lpQspiModule->BACONENTRY.U = bacon.U;
        }
        else
        {
        	/* No tx DMA required. */
            bacon.B.LAST = 1;
            /* Write BACON with LAST=1 to the QSPI tx FIFO */
            lpCntrl->lpQspiModule->BACONENTRY.U = bacon.U;
            /* Send the only data byte to QSPI tx FIFO */
			lpCntrl->lpQspiModule->DATAENTRY[pSlave->u16Cs%8].U = pu8SrcBuff[0];
        }

        IfxCpu_restoreInterrupts(interruptState);
	}

	return(i32Rtn);
}/* END qspi3mstr_Send() */


/*
 *******************************************************************************
 *
 * @author  rmilne
 * @date    Jul2020
 * @brief   Send/receive data to tlf35584 using DMA single mode and MIXENTRY/
 *			RXEXIT.
 *
 * MIX_ENTRY continuous data method as per 37.3.5.1.4 of AURIX UM:
 * In order to support sending short multiple frames to single channel without
 * having to send the same configuration each time, the TXFIFO follows this
 * rule:
 *	-if BACON.DL less or equal 15 (up to 16 bit data length)
 *	-if MIX_ENTRY address is used
 *	-then if the upper 16 bit of a 32-bit MIX_ENTRY are 0, the entry is
 *	  considered data, else configuration. Note that there is no valid
 *	  configuration word with upper 16 bits equal to 0.
 *	-in such a case the TXFIFO marks the entry as data.
 * Note: In case the BACON.LAST = 0, the slave select remains active between the
 * frames. The behavior is equal to the continuous mode. The disadvantage of
 * this method is that only up to 16 bits data can be transferred with one FPI
 * bus move. The advantage is that continuous mode is possible using the mix
 * entry.
 *
 * Examples of BACON/DATA arrays are in TLF35584Demo.c of AP32342.
 *
 * NB: NON-Blocking function.
 *
 * TODO: Implement Rx DMA ISR for deassert of CS.
 *
 * @param  	pu32TxBuff Pointer to tx word data buffer (static addressing).
 * @param	pu32RxBuff Pointer to rx word data buffer (static addressing).
 * @param  	u32TxLen The number of data bytes to send via MIXENTRY register
 * @param  	u32RxLen The number of data bytes to read via RXEXIT register
 * @retval 	TRUE
 *
 ******************************************************************************/
boolean qspi3mstr_sshotSend( const uint32_t* pu32TxBuff, uint16_t* pu32RxBuff, uint32_t u32TxLen, uint32_t u32RxLen )
{
	boolean bRtn = TRUE;
	Ifx_DMA	*pDma = &MODULE_DMA;
	QSPI3MSTR_Cfg_t* lpCntrl = &qspi3mstrCfg;


    qspi3mstrEvent = 0;
	lpCntrl->srcrRxd.pSRC_SRCR->B.CLRR = 1;    // Clear any pending interrupt
	lpCntrl->srcrTxd.pSRC_SRCR->B.CLRR = 1;    // Clear any pending interrupt

    /*
     *  MODULE_DMA.CH[TX].CHCFGR.B.TREL = u32TxLen;
     *  MODULE_DMA.CH[TX].CHCFGR.B.CHDW = 2;    // 2^(CHDW+3) = 32 bit
     *  MODULE_DMA.CH[TX].CHCFGR.B.BLKM = 0;    // 2^BLKM = 1 move(s)
     *  MODULE_DMA.CH[TX].CHCFGR.B.CHMODE = 0;  // Single Mode
     *  MODULE_DMA.CH[TX].CHCFGR.B.RROAT = 0;   // reset request each transfer
     *  MODULE_DMA.CH[TX].CHCFGR.B.PRSEL = 0;   // No daisy chain selected
     *
     *  MODULE_DMA.CH[TX].ADICR.B.SMF = 0;      // Source Address Modification Factor
     *  MODULE_DMA.CH[TX].ADICR.B.INCS = 1;     // Increment of the source address
     *  MODULE_DMA.CH[TX].ADICR.B.CBLS = 0;     // Circular Buffer Length Source 1 Byte
     *  MODULE_DMA.CH[TX].ADICR.B.SCBE = 0;     // Source circular buffer disabled
     *  MODULE_DMA.CH[TX].ADICR.B.DMF = 0;      // Destination Address Modification Factor
     *  MODULE_DMA.CH[TX].ADICR.B.INCD = 0;     // Decrement of the destination address
     *  MODULE_DMA.CH[TX].ADICR.B.CBLD = 0;     // Circular Buffer Length Destination 1 Byte
     *  MODULE_DMA.CH[TX].ADICR.B.DCBE = 1;     // Destination circular buffer enabled
     */
    pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCFGR.U = 0x00400000 | u32TxLen;		//Last one will be sent out by interrupt
    pDma->CH[QSPI3_DMA_TX_CHANNEL].ADICR.U = 0x00200008;
    pDma->CH[QSPI3_DMA_TX_CHANNEL].SADR.U = (uint32) pu32TxBuff;
    pDma->CH[QSPI3_DMA_TX_CHANNEL].DADR.U = (uint32) &( lpCntrl->lpQspiModule->MIXENTRY.U );

    /*
     *  MODULE_DMA.CH[RX].CHCFGR.B.TREL = NUM_ENTRIES_RXBUFFER_1ST
     *  MODULE_DMA.CH[RX].CHCFGR.B.CHDW = 1;    // 2^(CHDW+3) = 16 bit
     *  MODULE_DMA.CH[RX].CHCFGR.B.BLKM = 0;    // 2^BLKM = 1 move(s)
     *  MODULE_DMA.CH[RX].CHCFGR.B.CHMODE = 0;  // Single Mode
     *  MODULE_DMA.CH[RX].CHCFGR.B.RROAT = 0;   // reset request each transfer
     *  MODULE_DMA.CH[RX].CHCFGR.B.PRSEL = 0;   // No daisy chain selected
     *
     *  MODULE_DMA.CH[RX].ADICR.B.DMF = 0;      // Destination Address Modification Factor
     *  MODULE_DMA.CH[RX].ADICR.B.INCD = 1;     // Increment of the destination address
     *  MODULE_DMA.CH[RX].ADICR.B.INTCT = 2;    // Interrupt Control; 2=Interrupt trigger is generated and bit CHSRz.ICH is set on changing the TCOUNT value and TCOUNT equals IRDV
     *  MODULE_DMA.CH[RX].ADICR.B.IRDV = 0;     // Interrupt Raise Detect Value
     *  MODULE_DMA.CH[RX].ADICR.B.CBLD = 0;     // Circular Buffer Length Source 1 Byte
     *  MODULE_DMA.CH[RX].ADICR.B.DCBE = 0;     // Source circular buffer disabled
     *  MODULE_DMA.CH[RX].ADICR.B.SMF = 0;      // Source Address Modification Factor
     *  MODULE_DMA.CH[RX].ADICR.B.INCS = 0;     // Decrement of the source address
     *  MODULE_DMA.CH[RX].ADICR.B.CBLS = 0;     // Circular Buffer Length Destination 1 Byte
     *  MODULE_DMA.CH[RX].ADICR.B.SCBE = 1;     // Destination circular buffer enabled
     */
    pDma->CH[QSPI3_DMA_RX_CHANNEL].CHCFGR.U = 0x00200000 | u32RxLen;
    pDma->CH[QSPI3_DMA_RX_CHANNEL].ADICR.U = 0x08100080;
    pDma->CH[QSPI3_DMA_RX_CHANNEL].SADR.U = (uint32) &(lpCntrl->lpQspiModule->RXEXIT.U);
    pDma->CH[QSPI3_DMA_RX_CHANNEL].DADR.U = (uint32) pu32RxBuff;

    pDma->TSR[QSPI3_DMA_RX_CHANNEL].B.ECH = 1; // Enable hardware transfer request
    pDma->TSR[QSPI3_DMA_TX_CHANNEL].B.ECH = 1; // Enable hardware transfer request
    pDma->CH[QSPI3_DMA_TX_CHANNEL].CHCSR.B.SCH = 1; // Start the Transaction by one software request

	return (bRtn);
}/* END qspi3mstr_sshotSend() */


/*
 *******************************************************************************
 *
 * @author  JW
 * @date    Jun2018 (created)
 * @brief   Function: qspi3mstr_Enable()
 *
 * @param  	bState of type boolean.
 * 			Request to enable/disable the QSPI interface.
 * @retval	int32_t
 * 			The return status
 * 			> 0 this is returned when the QSPI module is in the state requested.
 * 			0 = Successful
 * 			< 0 failure.
 *
 ******************************************************************************/
int32_t qspi3mstr_Enable(boolean bState)
{
	int32_t i32Rtn;


	i32Rtn = qspi3mstr_Init(bState);

	return(i32Rtn);
}/* END qspi3mstr_Enable() */


/*
 *******************************************************************************
 *
 * @author  RG
 * @date    Mar2021 (created)
 * @brief   Function: qspi3mstr_waitForRxDone()
 *			Waiting for the triggered QSPI RX Event
 * @param  	None
 * @retval	None
 *
  ******************************************************************************/
boolean qspi3mstr_waitForRxDone( void )
{
	while( TRUE )
	{
		if( sem_TestForEvent( &qspi3mstrEvent, kuQSPI3_RX_EVENT) == TRUE )
		{
			return TRUE;
		}

		if( sem_TestForEvent( &qspi3mstrEvent, kuQSPI3_ERR_EVENT) == TRUE )
		{
			return FALSE;
		}
	}
} /* END of qspi3mstr_waitForRxDone */

