/*
 * GETH_Phy.c
 *
 *  Created on: Dec 2024
 *      Author: rgeng
 */

/* IFX includes */
#include "Ifx_reg.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Cpu/Std/IfxCpu.h"
#include "Port/Std/IfxPort.h"
#include "Cpu/Std/Platform_Types.h"
#include "Geth/Std/IfxGeth.h"
#include "Geth/Eth/IfxGeth_Eth.h"
#include "tools.h"


#define ETH_RGMII_TXCLK_PIN             IfxGeth_TXCLK_P11_4_OUT
#define ETH_RGMII_TXD0_PIN              IfxGeth_TXD0_P11_3_OUT
#define ETH_RGMII_TXD1_PIN              IfxGeth_TXD1_P11_2_OUT
#define ETH_RGMII_TXD2_PIN              IfxGeth_TXD2_P11_1_OUT
#define ETH_RGMII_TXD3_PIN              IfxGeth_TXD3_P11_0_OUT
#define ETH_RGMII_TXCTL_PIN             IfxGeth_TXCTL_P11_6_OUT
#define ETH_RGMII_RXCLK_PIN             IfxGeth_RXCLKA_P11_12_IN
#define ETH_RGMII_RXD0_PIN              IfxGeth_RXD0A_P11_10_IN
#define ETH_RGMII_RXD1_PIN              IfxGeth_RXD1A_P11_9_IN
#define ETH_RGMII_RXD2_PIN              IfxGeth_RXD2A_P11_8_IN
#define ETH_RGMII_RXD3_PIN              IfxGeth_RXD3A_P11_7_IN
#define ETH_RGMII_RXCTL_PIN             IfxGeth_RXCTLA_P11_11_IN
#define ETH_RGMII_MDC_PIN               IfxGeth_MDC_P12_0_OUT
#define ETH_RGMII_MDIO_PIN              IfxGeth_MDIO_P12_1_INOUT
#define ETH_RGMII_GREFCLK_PIN           IfxGeth_GREFCLK_P11_5_IN


#define __AN_100_FULL_CAPABILITIES__			/**< Throttle the auto negotation to 100Mbps and full duplex and below */
#define PHY_ADDRESS							(7u)
#define CSR_CLOCK_RANGE		0x01u
#define GMII_GOP_WRITE                     ((0 << 3) | (1 << 2))
#define GMII_GOP_READ_POST_INCR_ADDR       ((1 << 3) | (0 << 2))
#define GMII_GOP_READ                      ((1 << 3) | (1 << 2))
#define GMII_GOP_BUSY                      (1 << 0)

/* pin configuration for ECU8 Board */
static const IfxGeth_Eth_RgmiiPins rgmii_pins[IFXGETH_NUM_MODULES] =
{
	{
	   .txClk = &ETH_RGMII_TXCLK_PIN,
	   .txd0  = &ETH_RGMII_TXD0_PIN,
	   .txd1  = &ETH_RGMII_TXD1_PIN,
	   .txd2  = &ETH_RGMII_TXD2_PIN,
	   .txd3  = &ETH_RGMII_TXD3_PIN,
	   .txCtl = &ETH_RGMII_TXCTL_PIN,
	   .rxClk = &ETH_RGMII_RXCLK_PIN,				/**< P11.12 */
	   .rxd0  = &ETH_RGMII_RXD0_PIN,
	   .rxd1  = &ETH_RGMII_RXD1_PIN,
	   .rxd2  = &ETH_RGMII_RXD2_PIN,
	   .rxd3  = &ETH_RGMII_RXD3_PIN,
	   .rxCtl = &ETH_RGMII_RXCTL_PIN,
	   .mdc   = &ETH_RGMII_MDC_PIN,
	   .mdio  = &ETH_RGMII_MDIO_PIN,
	   .grefClk = &ETH_RGMII_GREFCLK_PIN
	},
};




void GETH_Phy_set_ECU8_rgmii_input_pins( Ifx_GETH *pGeth )
{
    IfxPort_InputMode   mode       = IfxPort_InputMode_noPullDevice;
    IfxPort_PadDriver   speedGrade = IfxPort_PadDriver_cmosAutomotiveSpeed4;
    uint8 gethIdx = (uint8)IfxGeth_getIndex( pGeth );
    const IfxGeth_Eth_RgmiiPins *rgmiiPins = &rgmii_pins[gethIdx];

    IfxGeth_Rxclk_In   *rxClk      = rgmiiPins->rxClk;
    IfxGeth_Rxctl_In   *rxCtl      = rgmiiPins->rxCtl;
    IfxGeth_Rxd_In     *rxd0       = rgmiiPins->rxd0;
    IfxGeth_Rxd_In     *rxd1       = rgmiiPins->rxd1;
    IfxGeth_Rxd_In     *rxd2       = rgmiiPins->rxd2;
    IfxGeth_Rxd_In     *rxd3       = rgmiiPins->rxd3;
    IfxGeth_Grefclk_In *grefClk    = rgmiiPins->grefClk;
    IfxGeth_Mdio_InOut *mdio       = rgmiiPins->mdio;

    pGeth->GPCTL.B.ALTI0 = mdio->inSelect;
    pGeth->GPCTL.B.ALTI1 = rxClk->select;
    pGeth->GPCTL.B.ALTI4 = rxCtl->select;
    pGeth->GPCTL.B.ALTI6 = rxd0->select;
    pGeth->GPCTL.B.ALTI7 = rxd1->select;
    pGeth->GPCTL.B.ALTI8 = rxd2->select;
    pGeth->GPCTL.B.ALTI9 = rxd3->select;

    IfxPort_setPinControllerSelection(rxClk->pin.port, rxClk->pin.pinIndex);
    IfxPort_setPinControllerSelection(rxCtl->pin.port, rxCtl->pin.pinIndex);
    IfxPort_setPinControllerSelection(rxd0->pin.port, rxd0->pin.pinIndex);
    IfxPort_setPinControllerSelection(rxd1->pin.port, rxd1->pin.pinIndex);
    IfxPort_setPinControllerSelection(rxd2->pin.port, rxd2->pin.pinIndex);
    IfxPort_setPinControllerSelection(rxd3->pin.port, rxd3->pin.pinIndex);
    IfxPort_setPinControllerSelection(grefClk->pin.port, grefClk->pin.pinIndex);

    IfxPort_setPinModeInput(rxClk->pin.port, rxClk->pin.pinIndex, mode);
    IfxPort_setPinModeInput(rxCtl->pin.port, rxCtl->pin.pinIndex, mode);
    IfxPort_setPinModeInput(rxd0->pin.port, rxd0->pin.pinIndex, mode);
    IfxPort_setPinModeInput(rxd1->pin.port, rxd1->pin.pinIndex, mode);
    IfxPort_setPinModeInput(rxd2->pin.port, rxd2->pin.pinIndex, mode);
    IfxPort_setPinModeInput(rxd3->pin.port, rxd3->pin.pinIndex, mode);
    IfxPort_setPinModeInput(grefClk->pin.port, grefClk->pin.pinIndex, mode);

    IfxPort_setPinPadDriver(rxClk->pin.port, rxClk->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(rxCtl->pin.port, rxCtl->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(rxd0->pin.port, rxd0->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(rxd1->pin.port, rxd1->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(rxd2->pin.port, rxd2->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(rxd3->pin.port, rxd3->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(grefClk->pin.port, grefClk->pin.pinIndex, speedGrade);
}



void GETH_Phy_set_ECU8_rgmii_output_pins( Ifx_GETH *pGeth )
{
    IFX_UNUSED_PARAMETER( pGeth );
    IfxPort_OutputMode  mode       = IfxPort_OutputMode_pushPull;
    IfxPort_PadDriver   speedGrade = IfxPort_PadDriver_cmosAutomotiveSpeed4;
    uint8 gethIdx = (uint8)IfxGeth_getIndex( pGeth );
    const IfxGeth_Eth_RgmiiPins *rgmiiPins = &rgmii_pins[gethIdx];

    IfxGeth_Txclk_Out  *txClk      = rgmiiPins->txClk;
    IfxGeth_Txctl_Out  *txCtl      = rgmiiPins->txCtl;
    IfxGeth_Txd_Out    *txd0       = rgmiiPins->txd0;
    IfxGeth_Txd_Out    *txd1       = rgmiiPins->txd1;
    IfxGeth_Txd_Out    *txd2       = rgmiiPins->txd2;
    IfxGeth_Txd_Out    *txd3       = rgmiiPins->txd3;
    IfxGeth_Mdc_Out    *mdc        = rgmiiPins->mdc;
    IfxGeth_Mdio_InOut *mdio       = rgmiiPins->mdio;

    IfxPort_setPinControllerSelection(mdc->pin.port, mdc->pin.pinIndex);
    IfxPort_setPinControllerSelection(mdio->pin.port, mdio->pin.pinIndex);
    IfxPort_setPinControllerSelection(txClk->pin.port, txClk->pin.pinIndex);
    IfxPort_setPinControllerSelection(txCtl->pin.port, txCtl->pin.pinIndex);
    IfxPort_setPinControllerSelection(txd0->pin.port, txd0->pin.pinIndex);
    IfxPort_setPinControllerSelection(txd1->pin.port, txd1->pin.pinIndex);
    IfxPort_setPinControllerSelection(txd2->pin.port, txd2->pin.pinIndex);
    IfxPort_setPinControllerSelection(txd3->pin.port, txd3->pin.pinIndex);

    IfxPort_setPinModeOutput(mdc->pin.port, mdc->pin.pinIndex, mode, mdc->select);
    IfxPort_setPinModeOutput(txClk->pin.port, txClk->pin.pinIndex, mode, txClk->select);
    IfxPort_setPinModeOutput(txCtl->pin.port, txCtl->pin.pinIndex, mode, txCtl->select);
    IfxPort_setPinModeOutput(txd0->pin.port, txd0->pin.pinIndex, mode, txd0->select);
    IfxPort_setPinModeOutput(txd1->pin.port, txd1->pin.pinIndex, mode, txd1->select);
    IfxPort_setPinModeOutput(txd2->pin.port, txd2->pin.pinIndex, mode, txd2->select);
    IfxPort_setPinModeOutput(txd3->pin.port, txd3->pin.pinIndex, mode, txd3->select);

    IfxPort_setPinPadDriver(mdc->pin.port, mdc->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(txClk->pin.port, txClk->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(txCtl->pin.port, txCtl->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(txd0->pin.port, txd0->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(txd1->pin.port, txd1->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(txd2->pin.port, txd2->pin.pinIndex, speedGrade);
    IfxPort_setPinPadDriver(txd3->pin.port, txd3->pin.pinIndex, speedGrade);
}

void GETH_Phy_reset_PHY( void )
{
      MODULE_P02.IOCR8.B.PC11 = 0x10;
	  MODULE_P02.OUT.B.P11 = 1;

	  delayMillSecond( 100 );
}

#define PHY_WAIT_GMII_READY() while (GETH_MAC_MDIO_ADDRESS.B.GB) {}


static void GETH_Phy_readReg( uint32 u32PhyAddr, uint32 u32RegAddr, uint32* pu32Data )
{
	GETH_MAC_MDIO_ADDRESS.U = (u32PhyAddr << 21) | (u32RegAddr << 16) | (CSR_CLOCK_RANGE << 8) | GMII_GOP_READ | GMII_GOP_BUSY;
	PHY_WAIT_GMII_READY();

	*pu32Data = GETH_MAC_MDIO_DATA.U;

}/* END () */


static void GETH_Phy_writeReg( uint32 u32PhyAddr, uint32 u32RegAddr, uint32 u32Data )
{
	uint32  mdio_addr_reg=0;

	GETH_MAC_MDIO_DATA.U = u32Data;

	mdio_addr_reg |= (u32PhyAddr << 21);
    mdio_addr_reg |= (u32RegAddr << 16);
    mdio_addr_reg |= (CSR_CLOCK_RANGE << 8);
    mdio_addr_reg |= GMII_GOP_WRITE;
    mdio_addr_reg |= GMII_GOP_BUSY;

    GETH_MAC_MDIO_ADDRESS.U = mdio_addr_reg;
    PHY_WAIT_GMII_READY();

}/* END () */


/* GETH module must be enabled before calling this function:
 * IfxGeth_enableModule(&MODULE_GETH);
 * MDIO pins must be intitalized before calling this function:
 * 	IfxPort_setPinModeOutput( ETH_RGMII_MDC_PIN.pin.port, ETH_RGMII_MDC_PIN.pin.pinIndex, IfxPort_OutputMode_pushPull, ETH_RGMII_MDC_PIN.select );
 * 	GETH_GPCTL.B.ALTI0  = ETH_RGMII_MDIO_PIN.inSelect;
 *
 */
void GETH_Phy_init( void )
{
	uint32 reg_value;


	PHY_WAIT_GMII_READY();

    // reset PHY
	GETH_Phy_writeReg( PHY_ADDRESS, 0x00, 0x8000 );   // reset
    uint32 value;

    do
    {
    	GETH_Phy_readReg( PHY_ADDRESS, 0x00, &value );
    } while (value & 0x8000);                   // wait for reset to finish


#if defined(__AN_100_FULL_CAPABILITIES__)						/* To limit the link speed to 100MBps full duplex: !!! This one is for MII interface, not RGMII */
	  /*Reading the value of Basic mode control register*/
    GETH_Phy_readReg( PHY_ADDRESS, 0x00, &reg_value);
	  reg_value &= ~(1<<6);			//Remove the 1000 Mbps speed capability.
	  GETH_Phy_writeReg( PHY_ADDRESS, 0x00, reg_value );

	  GETH_Phy_readReg( PHY_ADDRESS, 0x09, &reg_value );
	  reg_value &= ~(3<<8);
	  GETH_Phy_writeReg( PHY_ADDRESS, 0x09, reg_value );
	  GETH_Phy_writeReg( PHY_ADDRESS, 0x00, 0x1200 );		//Enable AN, restart AN
#else
	    // setup PHY
	  GETH_Phy_writeReg( PHY_ADDRESS, 0x00, 0x1200 );		//Enable AN, restart AN
#endif


    return;
}



