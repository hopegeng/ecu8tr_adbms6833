/*
 * qspimstr.c
 *
 *  Created on: Mar 18, 2022
 *      Author: rlarocque
 */
#include <stdint.h>
#include <stdbool.h>

#include "qspi0mstr.h"
#include "qspimstr.h"

/*
 *******************************************************************************
 *
 * @author  JW
 * @date    Dec2018 (created)
 * @author  rmilne
 * @date    May2020 (Add asynchronous DMA handling)
 * @brief   Send u16Length data bytes from pu8Buff using continuous data mode using the appropriate QSPI port based on board
 *
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
int32_t qspimstr_Send(QspiCs_t CS, uint16_t u16Length, uint8_t* pu8SrcBuff, uint8_t* pu8DstBuff)
{
#if(__BEVOP == 1)
#if defined(__QSPI_MCAL_DRIVER__)
	return qspimcal_Send( u16Length, pu8SrcBuff, pu8DstBuff );
#else
	return qspi0mstr_Send(CS,  u16Length,  pu8SrcBuff,	pu8DstBuff);
#endif
#else
	return qspi2mstr_Send(CS,  u16Length,  pu8SrcBuff,	pu8DstBuff);
#endif
}

/*
 *******************************************************************************
 *
 * @author  RG
 * @date    Jan2021 (created)
 * @brief   Function: qspimstr_waitForRxDone()
 *			Waiting for the SPI buffer is empty
 * @param  	None
 * @retval	None
 *
  ******************************************************************************/
bool qspimstr_waitForRxDone( void )
{
#if (__BEVOP == 1)

#if defined (__QSPI_MCAL_DRIVER__)
	return true;
#else
	return qspi0mstr_waitForRxDone();
#endif //__QSPI_MCAL_DRIVER__

#else
	//return qspi2mstr_waitForRxDone();
	return true;
#endif
}
