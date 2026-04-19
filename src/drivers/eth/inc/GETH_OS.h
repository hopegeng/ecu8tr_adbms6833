/*
 * GETH_OS.h
 *
 *  Created on: Dec, 2023
 *      Author: rgeng
 */

#ifndef BSP_ETH_INC_GETH_OS_H_
#define BSP_ETH_INC_GETH_OS_H_

extern void GETH_OS_start_rx_task(void);
extern sint32 GETH_OS_init_LWIP( boolean bEnable );
extern void GETH_OS_notify_rx( uint8 gethIdx, uint8 chanIdx );
extern void GETH_OS_lock( void );
extern void GETH_OS_unlock( void );
extern void start_network( void );

#endif /* BSP_ETH_INC_GETH_OS_H_ */
