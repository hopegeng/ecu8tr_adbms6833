/*
 * GETH_Phy.h
 *
 *  Created on: Jan 2024
 *      Author: rgeng
 */

#ifndef BSP_ETH_INC_GETH_PHY_H_
#define BSP_ETH_INC_GETH_PHY_H_

extern void GETH_Phy_set_ECU8_rgmii_input_pins( Ifx_GETH *pGeth );
extern void GETH_Phy_set_ECU8_rgmii_output_pins( Ifx_GETH *pGeth );
extern void GETH_Phy_reset_PHY( void );
extern void GETH_Phy_init( void );

#endif /* BSP_ETH_INC_GETH_PHY_H_ */
