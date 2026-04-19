/*
 * GETH_Debug.h
 *
 *  Created on: Jan 2024
 *      Author: rgeng
 */

#ifndef BSP_ETH_INC_GETH_DEBUG_H_
#define BSP_ETH_INC_GETH_DEBUG_H_


extern void GETH_Debug_print_upd_packet( uint8 gethIdx, uint8 *pFrame, uint16 frameLen );
extern void GETH_Debug_dump_rx_desc( uint8 gethIdx, uint8 chanIdx, uint8 offset );

#endif /* BSP_ETH_INC_GETH_DEBUG_H_ */
