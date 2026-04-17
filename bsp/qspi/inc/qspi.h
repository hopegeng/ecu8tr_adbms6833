/*
 * qspi.h
 *
 *  Created on: Jul 13, 2020
 *      Author: robm
 */

#ifndef BSP_QSPI_INC_QSPI_H_
#define BSP_QSPI_INC_QSPI_H_

#include "stdint.h"
#include "Qspi/Std/IfxQspi.h"

typedef enum QspiCs_e	/**< \brief QSPI Chip Select options */
{
	eQspiHwCs00,		/**< \brief Hardware controlled chip select 0 (SLS0) */
	eQspiHwCs01,		/**< \brief Hardware controlled chip select 1 (SLS1) */
	eQspiHwCs02,		/**< \brief Hardware controlled chip select 2 (SLS2) */
	eQspiHwCs03,		/**< \brief Hardware controlled chip select 3 (SLS3) */
	eQspiHwCs04,		/**< \brief Hardware controlled chip select 4 (SLS4) */
	eQspiHwCs05,		/**< \brief Hardware controlled chip select 5 (SLS5) */
	eQspiHwCs06,		/**< \brief Hardware controlled chip select 6 (SLS6) */
	eQspiHwCs07,		/**< \brief Hardware controlled chip select 7 (SLS7) */
	eQspiHwCs08,		/**< \brief Hardware controlled chip select 8 (SLS8) */
	eQspiHwCs09,		/**< \brief Hardware controlled chip select 9 (SLS9) */
	eQspiHwCs10,		/**< \brief Hardware controlled chip select 10 (SLS10) */
	eQspiHwCs11,		/**< \brief Hardware controlled chip select 11 (SLS11) */
	eQspiHwCs12,		/**< \brief Hardware controlled chip select 12 (SL120) */
	eQspiHwCs13,		/**< \brief Hardware controlled chip select 13 (SLS13) */
	eQspiHwCs14,		/**< \brief Hardware controlled chip select 14 (SLS14) */
	eQspiHwCs15,		/**< \brief Hardware controlled chip select 15 (SLS15) */

	eQspiHwCsNone,		/**< \brief Manual chip select */
} QspiCs_t;

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

#endif /* BSP_QSPI_INC_QSPI_H_ */
