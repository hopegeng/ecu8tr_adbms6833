/**
 ******************************************************************************
 * @file    qspi0mstr.c
 * @author	JW
 * @version V0.1.0
 * @date    Jun2018
 * @author	rmilne
 * @version V0.2.0
 * @date    May2020 - Gen2 update with asynchronous DMA
 * @brief	Configures QSPI0 as a master.
 * 			Full DUPLEX
 * 			4 Wire connection (SCLK/MOSI/MISO/CS)
 * 			CS HW or manual options.
 *
 * From TC37xAA_DS_v0.71.pdf:
 *	SLS_CAN		:	P20.13 	QSPI0_SLSO2	[O3]
 *	MOSI_CAN	:	P20.14 	QSPI0_MTSR	[O3]
 *	MISO_CAN	:	P20.12 	QSPI0_MRST	[I]
 *	SCK_CAN		:	P20.11 	QSPI0_SCLK	[O3]
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
 * CCUCON1.QSPIDIV = 1 (fQSPI = fSOURCEQSPI)
 * fPER = fQSPI = 200Mhz
 * GLOBALCON.CLKSEL = 1 (clock enable)
 * GLOBALCON.TQ = 9 (Global Time Quantum = fPER/GLOBALCON.TQ+1 = 20 Mhz)
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
 * NOTE6: (From page 72 of LTC2949.pdf)
 * The maximum data rate of an isoSPI link is determined by the length of the
 * cable used. For cables 10 meters or less the maximum 1MHz SPI clock frequency
 * is possible. As the length of the cable increases the maximum possible SPI
 * clock rate decreases. (as per fig.24: 100 meters -> 500KHz)
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
//#include "aurix_pin_mappings.h"
//#include "machine.h"
/* Application includes */
#if (__RL_SHELL == 1)
#include "shell.h"
#endif /* (__RL_SHELL == 1) */
#include "qspi0mstr.h"
#include "machine.h"


/*----- Private Types --------------------------------------------------------*/
#if 0
/** \brief Data buffer control structure
 */
typedef struct job_s
{
    void  	*data;		/**< \brief Pointer to data buffer */
    int16_t	remaining;	/**< \brief Size of data to send */
} job_t;

/** \brief Port pin description structure
 */
typedef struct QSPI_Ports_s
{
	Ifx_P* 	lpPort;	/**< \brief Pointer to port */
	uint8_t u8Pin;	/**< \brief Pin number */
} QSPI_Ports_t;

/** \brief QSPI slave control structure
 * ECON is shared between n and n+8 selects
 */
typedef struct QSPI_Slave_Ctl_s
{
	QSPI_Ports_t 	port;			/**< \brief Slave select port pin description */
	QspiCs_t 		u16Cs;			/**< \brief QSPI Chip Select */
	boolean 		u16ActiveLevel;	/**< \brief Chip select polarity */
	Ifx_QSPI_ECON 	econ;			/**< \brief QSPI Configuration Extension */
    job_t			tx;				/**< \brief Slave tx buffer structure */
    job_t			rx;				/**< \brief Slave rx buffer structure */
    uint16_t		errorFlags;		/**< \brief QSPI error flags */
} QSPI_Slave_Ctl_t;

/** \brief SRC control structure
 */
typedef struct QSPI_SrcSrcr_s
{
	Ifx_SRC_SRCR* 	pSRC_SRCR;	/**< \brief Pointer to Service Request Control Register */
	uint16_t 		u16Srpn;	/**< \brief Service Request Priority Number for SRC */
	IfxSrc_Tos		tos;		/**< \brief Identifier of interrupt service provider */
} QSPI_SrcSrcr_t;
#endif

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
} QSPIMSTR_Cfg_t;


/*----- Protected Variables --------------------------------------------------*/


/*----- Externals ------------------------------------------------------------*/


/*----- Macros ---------------------------------------------------------------*/
#define QSPI0_DMA_TX_CHANNEL 	(IfxDma_ChannelId_1)
#define QSPI0_DMA_RX_CHANNEL 	(IfxDma_ChannelId_2)


/*----- Prototypes -----------------------------------------------------------*/
#define QSPI0MASTER_DEBUG_PRINT 1

#if (QSPI0MASTER_DEBUG_PRINT == 1) && (__RL_SHELL == 1)
#define QSPI0M_DBG_WRITE(...) PRINTF("%sQSPI0 ", shell_GetCpu()); PRINTF(__VA_ARGS__)
#else
#define QSPI0M_DBG_WRITE(...)
#endif /* (QSPI0MASTER_DEBUG_PRINT == 1) && (__RL_SHELL == 1) */

#define kuQSPIMASTER_SLS_CS_ACTIVE_LOW									FALSE
#define kuQSPIMASTER_SLS_CS_ACTIVE_HIGH									TRUE

/* Number of CS */
#define kuQSPIMASTER_QSPI0_NUM_SLAVES									(1u)

#define QSPI0_SLAVE_SETTINGS 	\
{	\
	/* SLSO2, P20.13 (OUTPUT) Active LOW (IfxQspi0_SLSO2_P20_13_OUT) */	\
	.port.lpPort = &MODULE_P20,	\
	.port.u8Pin = (13u),		\
	.u16Cs = eQspiHwCs02,			/* ADBMS6822 slave select is SLSO2 (P20.13) */	\
	.u16ActiveLevel = kuQSPIMASTER_SLS_CS_ACTIVE_LOW,	\
	\
	 /* ECON Register configuration for this slave select		\
	 * isoSPI LTC6820 transceiver maximum baudrate is 1 MHz (see Note6)	\
	 * fPER = 200 (see Note1)			\
	 * GLOBALCON.TQ = 9 (set in qspi0mstr_Init())		\
	 * BR = fPER / (GLOBALCON.TQ + 1) * (Q + 1) * ((A + 1) + B + C) = 1 MHz		\
	 */			\
	.econ.B.Q = 3,		/* Time Quantum */	\
	.econ.B.A = 2,		/* Bit Segment 1 */	\
	.econ.B.B = 0,  	/* Bit Segment 2 */	\
	.econ.B.C = 2,  	/* Bit Segment 3 */	\
	.econ.B.CPH = 1,	/* Clock phase */	\
	.econ.B.CPOL = 0,	/* Clock polarity */	\
	.econ.B.PAREN = 0,	/* Parity check */	\
	.econ.B.BE = 0,		/* Big Endian */	\
	.tx.remaining = 0,						\
	.tx.data = NULL,						\
	.rx.remaining = 0,						\
	.rx.data = NULL							\
}

#define  QSPI0_SLAVE_0		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_1		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_2		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_3		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_4		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_5		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_6		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_7		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_8	 	QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_9		QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_10 	QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_11 	QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_12 	QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_13 	QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_14 	QSPI0_SLAVE_SETTINGS
#define  QSPI0_SLAVE_15 	QSPI0_SLAVE_SETTINGS

/*----- Static member variables ----------------------------------------------*/
/**< \brief event semaphore for synchronization */

/**< \brief Slave (CS) control structure */
__private0 __far QSPI_Slave_Ctl_t qspi0Slaves[16] __align(4) =
{
	QSPI0_SLAVE_0,
	QSPI0_SLAVE_1,
	QSPI0_SLAVE_2,
	QSPI0_SLAVE_3,
	QSPI0_SLAVE_4,
	QSPI0_SLAVE_5,
	QSPI0_SLAVE_6,
	QSPI0_SLAVE_7,
	QSPI0_SLAVE_8,
	QSPI0_SLAVE_9,
	QSPI0_SLAVE_10,
	QSPI0_SLAVE_11,
	QSPI0_SLAVE_12,
	QSPI0_SLAVE_13,
	QSPI0_SLAVE_14,
	QSPI0_SLAVE_15
};


boolean bIsQSPI0Init = FALSE;
__private0 __far volatile uint32_t qspi0mstrEvent __align(4) = (0u);

/**< \brief QSPI0 configuration */
QSPIMSTR_Cfg_t qspi0mstrCfg __align(4) =
{
	.lpQspiModule = &MODULE_QSPI0,

	.pisel_mris = (0xa - 0xa), /* MRST input line A */

	/* Slave Select Control Array */
	.u16SlaveListSize = kuQSPIMASTER_QSPI0_NUM_SLAVES,
	.lpSlaveList = qspi0Slaves,
	.active_slave = eQspiHwCs02,

	/* QSPI SRC controls
	 * NB: QSPI tx/rx SRCs use DMA channel priorities and type of service.
	 */
	.srcrTxd.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.QSPI.QSPI[0].TX),
	.srcrTxd.u16Srpn = ((uint16_t)QSPI0_DMA_TX_CHANNEL),
	.srcrTxd.tos = IfxSrc_Tos_dma,
	.srcrRxd.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.QSPI.QSPI[0].RX),
	.srcrRxd.u16Srpn = ((uint16_t)QSPI0_DMA_RX_CHANNEL),
	.srcrRxd.tos = IfxSrc_Tos_dma,
	.srcrErr.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.QSPI.QSPI[0].ERR),
	.srcrErr.u16Srpn = ((uint16_t)QSPI0_ERR_ISR_PRIORITY),
	.srcrErr.tos = QSPI0_CPU_TOS,

	/* DMA SRC controls
	 * NB: DMA tx/rx SRCs use CPU priorities and type of service.
	 */
	.srcrDmaTxCh.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.DMA.DMA[0].CH[QSPI0_DMA_TX_CHANNEL]),
	.srcrDmaTxCh.u16Srpn = ((uint16_t)QSPI0_TxD_ISR_PRIORITY),
	.srcrDmaTxCh.tos = QSPI0_CPU_TOS,
	.srcrDmaRxCh.pSRC_SRCR = ((Ifx_SRC_SRCR*)&MODULE_SRC.DMA.DMA[0].CH[QSPI0_DMA_RX_CHANNEL]),
	.srcrDmaRxCh.u16Srpn = ((uint16_t)QSPI0_RxD_ISR_PRIORITY),
	.srcrDmaRxCh.tos = QSPI0_CPU_TOS,

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
const Ifx_QSPI_BACON qspi0Bacon __align(4) =
{
	.B.BYTE = 0u,	/* [0|1] = [bits|bytes] data length (DL field) */
	.B.LAST = 0u,	/* [0|1] = [not last|last] word in frame */
	.B.DL = 7u, 	/* 8 bits per data block */
	.B.CS = 2u,		/* Chip Select channel */
	.B.MSB = 1u,	/* Shift out Most Significant Bit first */
	/* TIDLEa,b = TPER * 4^IPRE * (IDLE+1)
	 *          = 10 nsec * 16 * 4 = 640 nsec */
	.B.IPRE = 2u,
	.B.IDLE = 3u,
	/* TLEAD = TPER * 4^LPRE * (LEAD+1)
	 *       = 10 nsec * 16 * 2 =  320 nsec */
	.B.LPRE = 2u,
	.B.LEAD = 1u,
	/* TTRAIL = TPER * 4^TPRE * (TRAIL+1)
	 *        = 10 nsec * 4 * 8 = 320 nsec */
	.B.TPRE = 1u,
	.B.TRAIL = 7u
};


/*------------------------ Function Implementations --------------------------*/


/*----- Private Functions ----------------------------------------------------*/
/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    May2020 (created)
 * @brief   qspi0DmaTxISR
 *          DMA tx channel interrupt handler.
 *          Since the tx channel ADICR.INTCT = b10, the tx channel CHSR.ICH is
 *          set when CHSR.TCOUNT decrements to ADICR.IRDV (= zero).
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
IFX_INTERRUPT(qspi0DmaTxISR, QSPI0_CPU_VEC_TBL, QSPI0_TxD_ISR_PRIORITY)
{
    Ifx_DMA* pDma = &MODULE_DMA;


    /* Test if DMA tx channel interrupt is set */
    if (pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	QSPIMSTR_Cfg_t* lpCntrl = &qspi0mstrCfg;
    	/* Slave control structure determines pin configuration for CS deactivation */
    	QSPI_Slave_Ctl_t* pSlave  = &lpCntrl->lpSlaveList[lpCntrl->active_slave];
        Ifx_QSPI_BACON    bacon;


        /* Set DMA tx channel CICH bit to clear interrupt */
        pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCSR.B.CICH = 1;
        /* Disable DMA tx channel transaction */
        pDma->TSR[QSPI0_DMA_TX_CHANNEL].B.DCH = 1;

        /* Write BACON to QSPI tx FIFO to signal last byte of transfer */
        bacon.U = qspi0Bacon.U;
        bacon.B.LAST = 1;
        lpCntrl->lpQspiModule->BACONENTRY.U = bacon.U;

        /* Write last data byte to QSPI tx FIFO */
        lpCntrl->lpQspiModule->DATAENTRY[lpCntrl->active_slave].U = ((uint8 *)pSlave->tx.data)[pSlave->tx.remaining - 1];
    }
}

/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    May2020 (created)
 * @brief   qspi0DmaRxISR
 *          DMA rx channel interrupt handler
 *          Since the rx channel ADICR.INTCT = b10, the rx channel CHSR.ICH is
 *          set when CHSR.TCOUNT decrements to ADICR.IRDV (= zero).
 *          A global notification bit is set for synchronization.
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
IFX_INTERRUPT(qspi0DmaRxISR, QSPI0_CPU_VEC_TBL, QSPI0_RxD_ISR_PRIORITY)
{
    Ifx_DMA* pDma = &MODULE_DMA;


    /* Test if DMA rx channel interrupt is set */
    if (pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	QSPIMSTR_Cfg_t* lpCntrl = &qspi0mstrCfg;
    	QSPI_Slave_Ctl_t* pSlave  = &lpCntrl->lpSlaveList[lpCntrl->active_slave];


    	/* Set DMA rx channel CICH bit to clear interrupt */
    	pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCSR.B.CICH = 1;

    	/* Deactivate slave */
		IfxPort_State action = (pSlave->u16ActiveLevel == kuQSPIMASTER_SLS_CS_ACTIVE_LOW) ? IfxPort_State_high : IfxPort_State_low;
		IfxPort_setPinState(pSlave->port.lpPort, pSlave->port.u8Pin, action);

		/* Set notification bit for QSPI rx completion */
		sem_Set(&qspi0mstrEvent, kuQSPI0_RX_EVENT);
    }
}

/**
 *******************************************************************************
 *
 * @author  rmilne
 * @date    May2020 (created)
 * @brief   qspi0ErISR
 *          QSPI error interrupt handler
 *          Copies sticky error flags to the slave control struct, clears the
 *          flags and sets the global notification bit used for synchronization.
 *
 * @param   NONE.
 * @retval  NONE.
 *
 ******************************************************************************/
IFX_INTERRUPT(qspi0ErISR, QSPI0_CPU_VEC_TBL, QSPI0_ERR_ISR_PRIORITY)
{
	Ifx_DMA*			pDma	= &MODULE_DMA;
	QSPIMSTR_Cfg_t* 	lpCntrl	= &qspi0mstrCfg;
    QSPI_Slave_Ctl_t* 	pSlave	= &lpCntrl->lpSlaveList[lpCntrl->active_slave];


    /* Read error flags */
	pSlave->errorFlags = lpCntrl->lpQspiModule->STATUS.B.ERRORFLAGS;
	/* Clear error flags */
	lpCntrl->lpQspiModule->FLAGSCLEAR.U = 0xFFFFU;

	/* Deactivate slave */
	IfxPort_State action = (pSlave->u16ActiveLevel == kuQSPIMASTER_SLS_CS_ACTIVE_LOW) ? IfxPort_State_high : IfxPort_State_low;
	IfxPort_setPinState(pSlave->port.lpPort, pSlave->port.u8Pin, action);

	/* Set notification bit for QSPI error detection */
	sem_Set(&qspi0mstrEvent, kuQSPI0_ERR_EVENT);

	/* Test DMA channel interrupts */
    if (pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	/* Set DMA tx channel CICH bit to clear interrupt */
    	pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCSR.B.CICH = 1;
    }
    if (pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCSR.B.ICH != 0)
    {
    	/* Set DMA rx channel CICH bit to clear interrupt */
    	pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCSR.B.CICH = 1;
    }
}


/*
 *******************************************************************************
 *
 * @author  JW
 * @date    Jun2018 (created)
 * @author  rmilne
 * @date    May2020 (Add DMA)
 * @brief   Initialize/Deinitialize QSPI peripheral for asynchronous DMA
 *			operation.
 *
 * 			QSPI tx/rx SRCs use DMA channel priorities and type of service.
 * 			DMA tx/rx SRCs use CPU priorities and type of service.
 * 			Port pin definitions are located in aurix_pin_mappings.h
 *
 * @param   bState
 * 			TRUE = Enable the QSPI module as configured by lpCntrl.
 * 			FALSE = Disable and reset QSPI module.
 * @retval  0 = Successful; < 0 = failure.
 *
 ******************************************************************************/
inline int32_t qspi0mstr_Init(boolean bState)
{
	QSPIMSTR_Cfg_t* lpCntrl = &qspi0mstrCfg;
	int32_t i32Rtn = -1;
	uint16_t u16Psw = IfxScuWdt_getCpuWatchdogPassword();
	volatile uint32_t u32Reg;
	void* lpReg;
	Ifx_SRC_SRCR isr;
	boolean bInterruptState;
	Ifx_DMA* pDma = &MODULE_DMA;


	if((bState == TRUE) && (bIsQSPI0Init == FALSE))
	{/* Request to ENABLE the QSPI module */
		/* Clear ENDINIT protection */
		IfxScuWdt_clearCpuEndinit(u16Psw);
		/* Enable module clock and disable sleep mode (see Clock Control Register) */
		lpCntrl->lpQspiModule->CLC.U = 0x8u;
		(void)lpCntrl->lpQspiModule->CLC.U;
		/* Restore ENDINIT protection */
		IfxScuWdt_setCpuEndinit(u16Psw);

		/* Configure operational parameters */
		/* GLOBALCON (Global Configuration Register) */
		u32Reg = 0u;
		lpReg = ((uint32_t*)&u32Reg);
		((Ifx_QSPI_GLOBALCON*)lpReg)->B.TQ = 9u;		/* Global Time Quantum = fPER/B.TQ+1 (20 Mhz)*/
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
			if (lpCntrl->lpSlaveList[u16Index].u16Cs > 7)
			{
				econ_idx = lpCntrl->lpSlaveList[u16Index].u16Cs - 8;
			}
			else
			{
				econ_idx = lpCntrl->lpSlaveList[u16Index].u16Cs;
			}
			lpCntrl->lpQspiModule->ECON[econ_idx].U = lpCntrl->lpSlaveList[u16Index].econ.U;
		}
		/* Write Slave Select Output Control register */
		lpCntrl->lpQspiModule->SSOC.U = u32Reg;
		/* Write Master Mode Receive Input Select */
		lpCntrl->lpQspiModule->PISEL.U = lpCntrl->pisel_mris;

		/* Disable DMA Channel Hardware Transaction Requests */
		pDma->TSR[QSPI0_DMA_TX_CHANNEL].U = 0x00020000;
		pDma->TSR[QSPI0_DMA_RX_CHANNEL].U = 0x00020000;

		/* Initialize DMA tx channel */
		pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCFGR.U = lpCntrl->txDmaChannelCfg.U;
		pDma->CH[QSPI0_DMA_TX_CHANNEL].ADICR.U = lpCntrl->txDmaAddrIntCtl.U;

		/* Initialize DMA rx channel */
		pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCFGR.U = lpCntrl->rxDmaChannelCfg.U;
		pDma->CH[QSPI0_DMA_RX_CHANNEL].ADICR.U = lpCntrl->rxDmaAddrIntCtl.U;

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

		bIsQSPI0Init = TRUE;
		i32Rtn = 0;
	}
	else if((bState == FALSE) && (bIsQSPI0Init == TRUE))
	{/* Request to DISABLE the QSPI module */
		/* Wait for current DMA transaction to complete */
		while (lpCntrl->lpQspiModule->STATUS.B.PHASE != 0)
		{}

		/* Disable DMA Channel Hardware Transaction Requests */
		pDma->TSR[QSPI0_DMA_TX_CHANNEL].U = 0x00020000;
		pDma->TSR[QSPI0_DMA_RX_CHANNEL].U = 0x00020000;

		/* Clear channel Interrupt controls */
		pDma->CH[QSPI0_DMA_TX_CHANNEL].ADICR.U = 0u;
		pDma->CH[QSPI0_DMA_RX_CHANNEL].ADICR.U = 0u;

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

		bIsQSPI0Init = FALSE;
		i32Rtn = 0;
	}

	return(i32Rtn);
} /* END qspi0mstr_Init() */


/*----- Protected Functions --------------------------------------------------*/


/*----- Public Functions -----------------------------------------------------*/
/*
 *******************************************************************************
 *
 * @author  JW
 * @date    Dec2018 (created)
 * @author  rmilne
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
 * Completion of the operation is signaled by a qspi0mstrEvent bit set in the
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
int32_t qspi0mstr_Send(QspiCs_t CS, uint16_t u16Length, uint8_t* pu8SrcBuff,
						uint8_t* pu8DstBuff)
{
	int32_t i32Rtn = -1;

#if 0
	PRINTF( "\r\nqspi0mstr_send: \r\n" );
	for( int idx = 0; idx < u16Length; idx++ )
	{
		PRINTF( "0x%02x ", pu8SrcBuff[idx] );
		if( (idx+1) == u16Length )
		{
			PRINTF( "\r\n" );
		}
	}
#endif

	if((bIsQSPI0Init == TRUE) && (pu8SrcBuff != NULL) &&
		(pu8DstBuff != NULL) && (u16Length > 0) && (CS == 2))
	{
        Ifx_DMA				*pDma         	= &MODULE_DMA;
		QSPIMSTR_Cfg_t*		lpCntrl			= &qspi0mstrCfg;
        boolean				interruptState 	= IfxCpu_disableInterrupts();
        Ifx_QSPI_BACON		bacon;
        QSPI_Slave_Ctl_t*	pSlave;
		uint16_t			u16RxFifoDepth;
		uint32_t			u32Scratch[2];

		lpCntrl->active_slave = CS;
		pSlave = &lpCntrl->lpSlaveList[CS];
        pSlave->tx.data = pu8SrcBuff;
        pSlave->tx.remaining = (int16_t)u16Length;
        pSlave->rx.data = pu8DstBuff;
        pSlave->rx.remaining = (int16_t)u16Length;
        bacon.U = qspi0Bacon.U;
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
			pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCFGR.B.TREL = pSlave->tx.remaining - 1;
			/* Source of tx channel is tx buffer in slave control structure */
			pDma->CH[QSPI0_DMA_TX_CHANNEL].SADR.U = (uint32)((void *)IFXCPU_GLB_ADDR_DSPR(IfxCpu_getCoreId(), pSlave->tx.data));
			/* Destination of tx channel is QSPI tx FIFO */
			pDma->CH[QSPI0_DMA_TX_CHANNEL].DADR.U = (uint32)&lpCntrl->lpQspiModule->DATAENTRY[CS].U;
		}
		else
		{
			/* Disable DMA tx channel hardware transaction*/
			pDma->TSR[QSPI0_DMA_TX_CHANNEL].U = 0x00020000;
		}

		/* The full data length value is loaded to the rx channel CHSR.TCOUNT.
		 * The DMA rx ISR is triggered when the DMA rx channel has moved all
		 * expected data from the QSPI rx FIFO to the rx buffer.  The DMA rx
		 * ISR is responsible for deactivating the CS and event notification.
		 * A timeout while waiting for rx data will trigger the QSPI error ISR.
		 */
        pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCFGR.B.TREL = u16Length;
        /* Source of rx channel is QSPI rx FIFO */
        pDma->CH[QSPI0_DMA_RX_CHANNEL].SADR.U =(uint32)&lpCntrl->lpQspiModule->RXEXIT.U;
        /* Destination of rx channel is rx buffer in slave control structure */
        pDma->CH[QSPI0_DMA_RX_CHANNEL].DADR.U = (uint32)((void *)IFXCPU_GLB_ADDR_DSPR(IfxCpu_getCoreId(), pSlave->rx.data));

        /* Clear QSPI error flags */
        lpCntrl->lpQspiModule->FLAGSCLEAR.U = 0x00009FFFu;
        /* Clear QSPI service requests */
        lpCntrl->srcrTxd.pSRC_SRCR->B.CLRR = 1;
        lpCntrl->srcrRxd.pSRC_SRCR->B.CLRR = 1;
        lpCntrl->srcrErr.pSRC_SRCR->B.CLRR = 1;
        /* Clear DMA rx channel interrupt flag */
        pDma->CH[QSPI0_DMA_RX_CHANNEL].CHCSR.B.CICH = 1;

        /* Enable DMA hardware transaction for rx channel */
        pDma->TSR[QSPI0_DMA_RX_CHANNEL].B.ECH = 1;

        qspi0mstrEvent = 0;
        if (u16Length > 1)
        {
        	/* Clear DMA tx channel interrupt flag */
        	pDma->CH[QSPI0_DMA_TX_CHANNEL].CHCSR.B.CICH = 1;
            /* Enable DMA hardware transaction for tx channel */
        	pDma->TSR[QSPI0_DMA_TX_CHANNEL].B.ECH = 1;
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
			lpCntrl->lpQspiModule->DATAENTRY[CS].U = pu8SrcBuff[0];
        }

        IfxCpu_restoreInterrupts(interruptState);
	}

	return(i32Rtn);
}/* END qspi0mstr_Send() */


/*
 *******************************************************************************
 *
 * @author  JW
 * @date    Jun2018 (created)
 * @brief   Function: qspi0mstr_Enable()
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
int32_t qspi0mstr_Enable(boolean bState)
{
	return(qspi0mstr_Init(bState));
}/* END qspi0mstr_Enable() */


/*
 *******************************************************************************
 *
 * @author  RG
 * @date    Jan2021 (created)
 * @brief   Function: qspi0mstr_waitForRxDone()
 *			Waiting for the SPI buffer is empty
 * @param  	None
 * @retval	None
 *
  ******************************************************************************/
boolean qspi0mstr_waitForRxDone( void )
{
	while( TRUE )
	{
		if( sem_TestForEvent( &qspi0mstrEvent, kuQSPI0_RX_EVENT) == TRUE )
		{
			return TRUE;
		}

		if( sem_TestForEvent( &qspi0mstrEvent, kuQSPI0_ERR_EVENT) == TRUE )
		{
			return FALSE;
		}
	}
} /* END of qspi0mstr_waitForRxDone */
