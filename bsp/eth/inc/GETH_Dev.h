/*
 * GETH_Dev.h
 *
 *  Created on: Dec 2023
 *      Author: rgeng
 */


#ifndef BSP_ETH_INC_GETH_DEV_H_
#define BSP_ETH_INC_GETH_DEV_H_


extern void GETH_Dev_hw_init( uint8 gethIdx );
extern boolean GETH_Dev_start_tx( uint8 gethIdx );
extern boolean GETH_Dev_start_rx( uint8 gethIdx );
extern boolean GETH_Dev_hw_reset( uint8 gethIdx );
extern void* GETH_Dev_recv_packet( uint8_t gethIdx, uint8_t chanIdx, uint16_t *pFrameLen );
extern boolean GETH_Dev_rx_packet_avail( uint8 gethIdx, uint8 chanIdx );
extern boolean GETH_Dev_release_rx_desc( uint8 gethIdx, uint8 chanIdx, uint16 descIdx, uint8 dirtyCnt );
extern boolean GETH_Dev_wakeup_rx( uint8_t gethIdx, uint8 chanIdx );
extern sint32 GETH_Dev_enable_geth(boolean bState, uint8 gethIdx );
extern uint16 GETH_Dev_dma_xmit( uint8_t gethIdx, uint8_t chanIdx, uint8_t *pData, uint16_t dataLen );
extern uint16 GETH_Dev_get_rx_index( uint8 gethIdx, uint8 chanIdx );
extern void GETH_Dev_forward_rx_desc_index( uint8 gethIdx, uint8 chanIdx );

#endif /* BSP_ETH_INC_GETH_DEV_H_ */
