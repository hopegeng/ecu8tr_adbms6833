/*
 * uart.h
 *
 *  Created on: Jan 30, 2024
 *      Author: rgeng
 */

#ifndef BSP_UART_INC_UART_H_
#define BSP_UART_INC_UART_H_

#include "Ifx_types.h"

typedef enum
{
	ISOUART_LowSide = 0,	/**< MoT Tx or MoB ring Rx */
	ISOUART_HighSide = 1,	/**< MoB Tx or MoT ring Rx */
	/**************/
	ISOUART_NET_NONE
} ISOUART_NetArch_t;


extern void isouart_init( void );
extern void isouart_deinit( void );
extern boolean isouart_setnetwork( ISOUART_NetArch_t network );
extern void isouart_write( uint8* pMsg, uint16 count );
extern void isouart_getResult( uint8 *pBuffer, uint8 *pLen );
extern boolean isouart_isRing( void );
extern boolean isouart_getRing( void );
extern void isouart_apply_network( ISOUART_NetArch_t network ); //From EEPROM
extern ISOUART_NetArch_t isouart_get_network( void );

#endif /* BSP_UART_INC_UART_H_ */
