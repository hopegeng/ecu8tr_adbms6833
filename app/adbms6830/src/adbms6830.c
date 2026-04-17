/**
 *******************************************************************************
 * \file    adbms6830.c
 * \author	RL
 * \date    May 2022
 * \brief   The low level interface to ADBMS6830 through isoSPI bus
 *
 ******************************************************************************
 * \attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2020 Neutron Controls</center></h2>
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
 *******************************************************************************
 */

/* C includes */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Stm/Std/IfxStm.h"
#include "Port/Io/IfxPort_Io.h"
#include "_PinMap/IfxPort_PinMap.h"

/* Application includes */
#if (__RL_SHELL == 0)
#include "shell.h"
#endif /* (__RL_SHELL == 1). */

#if defined(__QSPI_MCAL_DRIVER__)

#else
#if (__BEVOP)
//#include "qspi0mstr.h'
#else
#include "qspi2mstr.h"
#endif
#endif //_QSPI_MCAL_DRIVER__

//#include "dout.h"
#include "adbms6830.h"
#include "adbms6830_reg.h"
#include "adbmsCommon.h"
#include "ecu8tr_tcp.h"
//#include "bmsControl.h"

//#include "qspimstr.h"

#include "qspi0mstr_illd.h"

/*----- Macros --------------------------------------------------------*/
#if (__BEVOP)
#define SPI_CHIP_SELECT		2
#else
#define SPI_CHIP_SELECT		eQspiHwCs00
#endif

#define ADBMS2950_TIMEOUT_CHECK(CURRENT_TIME,TIMEOUT) ( CURRENT_TIME >= TIMEOUT )

#define RD_WR_BUFF_SIZE		(20u * ADBMS6830_NUMBER_OF_NODES)


/*----- Static members -------------------------------------------------------*/
//__private0 __far static uint8_t ucWriteBuffer[RD_WR_BUFF_SIZE];
//__private0 __far static uint8_t ucReadBuffer[RD_WR_BUFF_SIZE];
uint8_t ucWriteBuffer[RD_WR_BUFF_SIZE];
uint8_t ucReadBuffer[RD_WR_BUFF_SIZE];


/*----- Private Functions ----------------------------------------------------*/

static uint64_t adbms6830_Micros(void)
{
	return (IfxStm_get( &MODULE_STM0 ) / (IfxStm_getFrequency( &MODULE_STM0) / 1000000));
}	/* END of micros */

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar 2022
 * \brief	Function: adbms6830_read
 * 			To read data from adbms6830
 *
 * \param	addr: The memory or register address to read from
 * \param	pRxBuff: the read buffer
 * \param   rd_len: The amount of data bytes being read in
 * \param 	numNodes: The number of nodes in the ring
 * \retval	True: On Success
 * \retval	False: On Failure
 *****************************************************************************/
uint16_t received_pec10;
uint16_t pec10;
uint8_t *pPayLoad ;
static bool adbms6830_read( uint16_t addr, uint8_t *pRxBuff, uint16_t rd_len)
{
	uint16_t pec15;
//	uint16_t pec10;
//	uint16_t received_pec10;
	uint16_t tx_len = 0x00u;
	uint8_t cmd_cntr = 0;
	uint8_t bytes_to_read = 0;

	bool retVal = false;
	QspiCs_t cs = CS_get();
	memset( ucWriteBuffer, 0x0, RD_WR_BUFF_SIZE );
	memset( ucReadBuffer, 0x0, RD_WR_BUFF_SIZE );
	ucWriteBuffer[tx_len++] = (uint8_t)(addr >> 8);
	ucWriteBuffer[tx_len++] = (uint8_t)addr;
	pec15 = adbmsCommon_Calc_pec15( ucWriteBuffer, 2 );
	ucWriteBuffer[tx_len++] = (uint8_t)(pec15>>8);
	ucWriteBuffer[tx_len++] = (uint8_t)pec15;

	adbms6830_wakeup( true );
	adbmsCommon_delayMillseconds(1);

	bytes_to_read = (rd_len * ADBMS6830_NUMBER_OF_NODES) + (PEC10_LENGTH * ADBMS6830_NUMBER_OF_NODES);

	qspi0_send_receive_iLLD(cs, (tx_len + bytes_to_read), ucWriteBuffer, ucReadBuffer );	/* 2 = pec15 */
	if(qspimstr_waitForRxDone_iLLD() == false)
	{
		//memset(pRxBuff, 0xFF, sizeof(pRxBuff));
		return false;
	}
	else
	{
		// load pPayload with pointer to start of data
		pPayLoad = ucReadBuffer + tx_len; //

		//loop through number of nodes
		for (uint8_t i=0; i<ADBMS6830_NUMBER_OF_NODES; i++)
		{
			received_pec10 = (((uint16_t)pPayLoad[rd_len*(i+1)+(2*i)] & 0x03u) << 8)  | (uint16_t)pPayLoad[rd_len*(i+1)+(2*i)+1];
			cmd_cntr = pPayLoad[rd_len*(i+1)+(2*i)] >> 2;

			pec10 = adbmsCommon_Calc_pec10(	pPayLoad + (rd_len+2)*i, cmd_cntr, rd_len );

			if( pec10 == received_pec10 )
			{
				//TODO: this requires work to make it operate with more than one node
				memmove( (pRxBuff+(rd_len*i)), pPayLoad+((rd_len+2)*i), rd_len );
				adbms6830Info.reg[i].command_Counter = cmd_cntr;
				retVal = true;
			}
			else
			{
				//memset(pRxBuff, 0xFF, sizeof(pRxBuff));
				dbgPRINT( "Received wrong PEC10." );
				__debug();
			}
		}
	}

	return retVal;
} /* END of adbms6830_read */

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	March 2022
 * \brief	Function: adbms6830_write
 * 			Write data to adbms6830 over the isoSPI interface
 *
 * \param	addr: type of uint16_t, the register address to write
 * \param   pData: Data buffer
 * \param   len: data buffer length
 * \retval	True: On success
 * \retval	False: On Failure
 *****************************************************************************/
bool adbms6830_write(uint16_t addr, uint8_t* pData, uint16_t len )
{
	uint16_t pec15;
	uint16_t pec10;
	uint16_t idx = 0u;
	bool ret = false;
	bool retVal = false;
	QspiCs_t cs = CS_get();

	memset( ucWriteBuffer, 0x0, RD_WR_BUFF_SIZE );
	memset( ucReadBuffer, 0x0, RD_WR_BUFF_SIZE );

	ucWriteBuffer[idx++] = (uint8_t)(addr >> 8);
	ucWriteBuffer[idx++] = (uint8_t)addr;
	pec15 = adbmsCommon_Calc_pec15( ucWriteBuffer, 2 );
	//PRINTF("PEC15=, %d\r\n", pec15);
	ucWriteBuffer[idx++] = (uint8_t)(pec15>>8);
	ucWriteBuffer[idx++] = (uint8_t)pec15;

	for(uint8 i=0; i<ADBMS6830_NUMBER_OF_NODES; ++i)
	{
		for( uint16_t loop = 0; loop < len; loop ++ )
		{
			ucWriteBuffer[idx++] = pData[loop];
		}

		pec10 = adbmsCommon_Calc_pec10(	pData, 0x00, len );

		ucWriteBuffer[idx++] = (uint8_t)(pec10>>8);
		ucWriteBuffer[idx++] = (uint8_t)pec10;
	}

	adbms6830_wakeup( true);
	qspi0_send_receive_iLLD(cs, idx, ucWriteBuffer, ucReadBuffer );	/* 2 = pec15 */
	ret = qspimstr_waitForRxDone_iLLD();

	if( false == ret )
	{
		dbgPRINT("Error in Transmitting");
		__debug();
	}
	else
	{
		retVal = true;
	}

	return retVal;
} /* END of adbms6830_write */


/**
 ******************************************************************************
 *
 * \author 	RG
 * \date   	Jan2021
 * \brief	Function: adbms6830_wakeup
 * 			Wake up the isoSPI LTC6820 chip before transferring data to ADBMS2950
 *
 * \param	force: A boolean to indicate that wakeup the LTC6820 even if it is not in
 *          sleep mode.
 * \retval	None
 *
 *****************************************************************************/
 static void adbms6830_wakeup(bool force)
{
	if( !force && !ADBMS2950_TIMEOUT_CHECK(adbms6830_Micros(), adbmsCommon_isoSPI_wakeup_timeoutUs) )  //PRQA S 3432 #testing to see if this works - but this would be my justification why
	{
		//PRINTF( "LTC2949: The isoSPI interface is in active mode.\r\n" );.
	}
	else
	{


		// timeout!

		// this will generate several isoSPI pulses: long-, long+, ....
		// the first part in the daisychain will enable its isoSPI
		// circuit and send a long+ pulse to the 2nd device
		// that will wake up isoSPI of the 2nd device and again
		// send a long+ to the next device and so on.
		// All those long+ pulses are equivalent to CS-high events.
		// only after all isoSPI ports are awake a CS low can
		// be generated

		// Note:
		// Before all devices of the chain are awake the LTC2949
		// will never see a long- pulse and thus it won't see a CS-low
		// event. Only after all isoSPI circuits are awake they can
		// actually pass through a long- pulse which triggers a CS-
		// low event on the 2949 that wakes up the LTC2949 core from sleep!
		//

		uint8_t tx_buff[1] = {0};
		uint8_t rx_buff[1] = {0};
		QspiCs_t cs = CS_get();

		/* Wake up isoSPI */
		tx_buff[0] = 0x0;
		qspi0_send_receive_iLLD(cs, 1, tx_buff, rx_buff);
		//qspimstr_Send( eQspiHwCs00,1, tx_buff, rx_buff );
		qspimstr_waitForRxDone_iLLD();
		//adbmsCommon_delayMillseconds(1);
		adbmsCommon_isoSPI_wakeup_timeoutUs_init();
	}
} /* adbms6830_wakeup */



/*----- Public Functions -----------------------------------------------------*/

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	May2022
 * \brief	Function: adbms6830_Config
 * 			Configure the adbms6830
 *
 * \retval	True: success
 * \retval	False: failed
 *
 *****************************************************************************/
bool adbms6830_Config(void)
{
	uint8_t data[6];
	bool retVal = false;

	//initialize faults
	adbms6830Faults.U32 = 0u;

	//Configure CFGA
	adbms6830Info.reg[0].CFGA.CFGA0.B.REFON = 1;
	adbms6830Info.reg[0].CFGA.CFGA5.B.COMMBK = 1;
	adbms6830Info.reg[0].CFGA.CFGA5.B.FC = 7;


	//Initialization with default configurations
	if (false == adbms6830_write(ADBMS6830_SRST_REG, data, 0))
	{
		dbgPRINT("Failed to issue SRST");
		__debug();
	}

    //Initialization with default configurations and Corner Frequency FC[2:0]
	else if (false == adbms6830_WriteCFGA() )
	{
		dbgPRINT("Error in clearing updating CFGA");
		__debug();
	}
	//read default settings of config registers
	else if (false == adbms6830_ReadCFGA())
	{
		dbgPRINT("Error in reading CFGA");
		__debug();
	}
	//Configure CFGB
	// note the only thing to configure here is Cell OV and UV thresholds
	else if (false == adbms6830_ConfigureOVUVthresholds())
	{
		dbgPRINT("Error in configuring OVUV thresholds");
		__debug();
	}
	else if (false == adbms6830_ReadCFGB())
	{
		dbgPRINT("CFGB re-read");
		__debug();
	}

	//
	else if (false == adbms6830_clearFaultFlags() )
	{
		dbgPRINT("Error in clearing Fault Flags");
		__debug();
	}
	else if (false == adbms6830_ReadStatC())
	{
		dbgPRINT("Error in reading STATC");
		__debug();
	}
	else if (true == adbms6830_VerifyFaultStatC())
	{
		dbgPRINT("Fault during initialization");
		adbms6830Faults.B.SM_STATC_FAULT = 1;
		__debug();
	}
	//read serial ID
	else if (false == adbms6830_ReadSID())
	{
		dbgPRINT("Error in reading SID");
		__debug();
	}
	else
	{

		if (false == adbms6830_muteDischarge())
		{
			dbgPRINT("Error in mute command");
			__debug();
		}
		else if (false == adbms6830_write(ADBMS6830_ADCV_BASE_REG|ADBMS6830_ADCV_CONT_OPTION, data,0 ))
		{
			dbgPRINT("Failed to start measurements");
			__debug();
		}
		else
		{
			dbgPRINT("Started continuous measurement");
			retVal = true;
		}
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms2950_ReadRDCFGA
 * 			Read the Configuration Register Group A
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadCFGA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;

	if (false == adbms6830_read(ADBMS6830_RDCFGA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCFGA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].CFGA.CFGA0.U8 = data[0 + 6*i];
			adbms6830Info.reg[i].CFGA.CFGA1.U8 = data[1 + 6*i];
			adbms6830Info.reg[i].CFGA.CFGA2.U8 = data[2 + 6*i];
			adbms6830Info.reg[i].CFGA.CFGA3.U8 = data[3 + 6*i];
			adbms6830Info.reg[i].CFGA.CFGA4.U8 = data[4 + 6*i];
			adbms6830Info.reg[i].CFGA.CFGA5.U8 = data[5 + 6*i];
		}

		retVal = true;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCFGB
 * 			Read the Configuration Register Group B
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadCFGB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDCFGB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCFGB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].CFGB.CFGB0.U8 = data[0 + i*6];
			adbms6830Info.reg[i].CFGB.CFGB1.U8 = data[1 + i*6];
			adbms6830Info.reg[i].CFGB.CFGB2.U8 = data[2 + i*6];
			adbms6830Info.reg[i].CFGB.CFGB3.U8 = data[3 + i*6];
			adbms6830Info.reg[i].CFGB.CFGB4.U8 = data[4 + i*6];
			adbms6830Info.reg[i].CFGB.CFGB5.U8 = data[5 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSID
 * 			Read the Serial ID Register Group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
static bool adbms6830_ReadSID(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDSID_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSID");
		__debug();
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSID.SIDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSID.SIDR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSID.SIDR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSID.SIDR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSID.SIDR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSID.SIDR5 = data[5 + i*6];
			PRINTF( "The SID = 0x%x\r\n", data[0 + i*6] );
			PRINTF( "The SID = 0x%x\r\n", data[1 + i*6] );
			PRINTF( "The SID = 0x%x\r\n", data[2 + i*6] );
			PRINTF( "The SID = 0x%x\r\n", data[3 + i*6] );
			PRINTF( "The SID = 0x%x\r\n", data[4 + i*6] );
			PRINTF( "The SID = 0x%x\r\n", data[5 + i*6] );
			while( 1 )
				;
		}

		retVal = true;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadStatA
 * 			Read the Status Register Group A
 *
 * \retval	1u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadStatA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSTATA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSTATA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSTAT.STATA.STAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATA.STAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATA.STAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATA.STAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATA.STAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATA.STAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Status_Voltages.VREF2 = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Status_Voltages.VREF2 |= data[0 + i*6];

			adbms6830Info.rawData[i].Status_Voltages.ITMP = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Status_Voltages.ITMP |= data[2 + i*6];
		}
		retVal = 1u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadStatB
 * 			Read the Status reg[i]ister Group B
 *
 * \retval	2u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadStatB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSTATB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSTATB");

	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSTAT.STATB.STBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATB.STBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATB.STBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATB.STBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATB.STBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATB.STBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Status_Voltages.VD = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Status_Voltages.VD |= data[0 + i*6];

			adbms6830Info.rawData[i].Status_Voltages.VA = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Status_Voltages.VA |= data[2 + i*6];

			adbms6830Info.rawData[i].Status_Voltages.VRES = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Status_Voltages.VRES |= data[4 + i*6];
		}
		retVal = 2u;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadStatC
 * 			Read the Status Register Group C
 *
 * \param 	numNodes : The number of nodes in the ring
 *
 * \retval	4u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadStatC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSTATC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSTATC");
	}
	else
	{
		for (uint8_t i = 0; i<ADBMS6830_NUMBER_OF_NODES;i++)
		{
			adbms6830Info.reg[i].RDSTAT.STATC.STCR0.U8 = data[0 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATC.STCR1.U8 = data[1 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATC.STCR2.U8 = data[2 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATC.STCR3.U8 = data[3 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATC.STCR4.U8 = data[4 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATC.STCR5.U8 = data[5 + i*6];
		}

		retVal = 4u;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_VerifyFaultStatC
 * 			Verify that Status Register Group C contains no errors
 *
 * \retval	1u : Failure found in STATC register
 * \retval	0u : no failure
 *
 *****************************************************************************/
static bool adbms6830_VerifyFaultStatC(void)
{
	bool retVal = false;

	for (uint8_t i = 0; i<ADBMS6830_NUMBER_OF_NODES;i++)
	{
		if ((ADBMS6830_NOFAULTS < adbms6830Info.reg[i].RDSTAT.STATC.STCR0.U8) || (ADBMS6830_NOFAULTS < adbms6830Info.reg[i].RDSTAT.STATC.STCR1.U8))
		{
			retVal = true;
		}
		if ((ADBMS6830_NOFAULTS < adbms6830Info.reg[i].RDSTAT.STATC.STCR4.U8) || (ADBMS6830_NOFAULTS < adbms6830Info.reg[i].RDSTAT.STATC.STCR5.U8))
		{
			retVal = true;
		}
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadStatD
 * 			Read the Status Register Group D
 *
 * \retval	8u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadStatD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSTATD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSTATD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSTAT.STATD.STDR0.U8 = data[0 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATD.STDR1.U8 = data[1 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATD.STDR2.U8 = data[2 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATD.STDR3.U8 = data[3 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATD.STDR4.reserved = data[4 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATD.STDR5.U8 = data[5 + i*6];
		}
		retVal = 8u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadStatE
 * 			Read the Status Register Group E
 *
 * \retval	16u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadStatE(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (false == adbms6830_read(ADBMS6830_RDSTATE_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSTATE");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSTAT.STATE.STER0.reserved = data[0 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATE.STER1.reserved = data[1 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATE.STER2.reserved = data[2 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATE.STER3.reserved = data[3 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATE.STER4.U8 = data[4 + i*6];
			adbms6830Info.reg[i].RDSTAT.STATE.STER5.U8 = data[5 + i*6];
		}
		retVal = 16u;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCVA
 * 			Read the Cell Voltages C1, C2, C3
 *
 * \retval	1u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadCVA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10]={0};
	uint8_t retVal = 0u;

	if (false == adbms6830_read(ADBMS6830_RDCVA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCVA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCV.RDCVA.CVAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVA.CVAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVA.CVAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVA.CVAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVA.CVAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVA.CVAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Cell.C1V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C1V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Cell.C2V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C2V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Cell.C3V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C3V |= (uint16_t)data[4 + i*6];
		}
		retVal = 1u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCVB
 * 			Read the Cell Voltages C4, C5, C6
 *
 * \retval	2u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadCVB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0;;
	if (false == adbms6830_read(ADBMS6830_RDCVB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCVB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCV.RDCVB.CVBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVB.CVBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVB.CVBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVB.CVBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVB.CVBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVB.CVBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Cell.C4V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C4V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Cell.C5V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C5V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Cell.C6V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C6V |= (uint16_t)data[4 + i*6];
			//PRINTF("CV1= %d, CV2= %d, CV3=, %d\r\n", adbms6830Info.rawData[i].Cell.C4V, adbms6830Info.rawData[i].Cell.C5V, adbms6830Info.rawData[i].Cell.C6V);
		}
		retVal = 2u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCVC
 * 			Read the Cell Voltages C7, C8, C9
 *
 * \retval	4u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadCVC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDCVC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCVC");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCV.RDCVC.CVCR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVC.CVCR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVC.CVCR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVC.CVCR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVC.CVCR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVC.CVCR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Cell.C7V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C7V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Cell.C8V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C8V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Cell.C9V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C9V |= (uint16_t)data[4 + i*6];
		}
		retVal = 4u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCVD
 * 			Read the Cell Voltages C10, C11, C12
 *
 * \retval	8u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadCVD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (false == adbms6830_read(ADBMS6830_RDCVD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCVD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCV.RDCVD.CVDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVD.CVDR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVD.CVDR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVD.CVDR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVD.CVDR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVD.CVDR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Cell.C10V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C10V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Cell.C11V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C11V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Cell.C12V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C12V |= (uint16_t)data[4 + i*6];
		}
		retVal = 8u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCVE
 * 			Read the Cell Voltages C13, C14, C15
 *
 * \retval	16u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadCVE(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDCVE_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCVE");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCV.RDCVE.CVER0 = data[0 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVE.CVER1 = data[1 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVE.CVER2 = data[2 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVE.CVER3 = data[3 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVE.CVER4 = data[4 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVE.CVER5 = data[5 + i*6];

			adbms6830Info.rawData[i].Cell.C13V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C13V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Cell.C14V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C14V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Cell.C15V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C15V |= (uint16_t)data[4 + i*6];
		}
		retVal = 16u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCVF
 * 			Read the Cell Voltage C16
 *
 * \retval	32u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadCVF(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDCVF_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCVF");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCV.RDCVF.CVFR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDCV.RDCVF.CVFR1 = data[1 + i*6];

			adbms6830Info.rawData[i].Cell.C16V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Cell.C16V |= (uint16_t)data[0 + i*6];
		}
		retVal = 32u;
	}

	return retVal;
}



/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadACVA
 * 			Read the Averaged Cell Voltages C1, C2, C3
 *
 * \retval	1u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadACVA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDACA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDACVA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDACV.RDACVA.ACVAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVA.ACVAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVA.ACVAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVA.ACVAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVA.ACVAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVA.ACVAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].AvCell.AC1V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC1V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].AvCell.AC2V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC2V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].AvCell.AC3V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC3V |= (uint16_t)data[4 + i*6];
		}
		retVal = 1u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadACVB
 * 			Read the Averaged Cell Voltages C4, C5, C6
 *
 * \retval	2u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadACVB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;
	if (false == adbms6830_read(ADBMS6830_RDACB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDACVB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDACV.RDACVB.ACVBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVB.ACVBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVB.ACVBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVB.ACVBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVB.ACVBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVB.ACVBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].AvCell.AC4V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC4V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].AvCell.AC5V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC5V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].AvCell.AC6V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC6V |= (uint16_t)data[4 + i*6];
		}
		retVal = 2u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadACVC
 * 			Read the Averaged Cell Voltages C7, C8, C9
 *
 * \retval	4u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadACVC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0;


	if (false == adbms6830_read(ADBMS6830_RDACC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDACVC");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDACV.RDACVC.ACVCR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVC.ACVCR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVC.ACVCR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVC.ACVCR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVC.ACVCR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVC.ACVCR5 = data[5 + i*6];

			adbms6830Info.rawData[i].AvCell.AC7V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC7V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].AvCell.AC8V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC8V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].AvCell.AC9V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC9V |= (uint16_t)data[4 + i*6];
		}
		retVal = 4u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadACVD
 * 			Read the Averaged Cell Voltages C10, C11, C12
 *
 * \retval	8u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadACVD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDACD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDACVD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDACV.RDACVD.ACVDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVD.ACVDR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVD.ACVDR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVD.ACVDR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVD.ACVDR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVD.ACVDR5 = data[5 + i*6];

			adbms6830Info.rawData[i].AvCell.AC10V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC10V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].AvCell.AC11V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC11V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].AvCell.AC12V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC12V |= (uint16_t)data[4 + i*6];
		}
		retVal = 8u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadACVE
 * 			Read the Averaged Cell Voltages C13, C14, C15
 *
 * \retval	16u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadACVE(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDACE_REG, data, 6))
	{
		dbgPRINT("Error in reading RDACVE");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDACV.RDACVE.ACVER0 = data[0 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVE.ACVER1 = data[1 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVE.ACVER2 = data[2 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVE.ACVER3 = data[3 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVE.ACVER4 = data[4 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVE.ACVER5 = data[5 + i*6];

			adbms6830Info.rawData[i].AvCell.AC13V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC13V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].AvCell.AC14V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC14V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].AvCell.AC15V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC15V |= (uint16_t)data[4 + i*6];
		}
		retVal = 16u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadACVF
 * 			Read the Averaged Cell Voltage C16
 *
 * \retval	32u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadACVF(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDACF_REG, data, 6))
	{
		dbgPRINT("Error in reading RDACVF");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDACV.RDACVF.ACVFR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDACV.RDACVF.ACVFR1 = data[1 + i*6];

			adbms6830Info.rawData[i].AvCell.AC16V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].AvCell.AC16V |= (uint16_t)data[0 + i*6];
		}
		retVal = 32u;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadFCA
 * 			Read the Filtered Cell Voltages C1, C2, C3
 *
 * \retval	1u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadFCA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (!true == adbms6830_read(ADBMS6830_RDFCA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDFCA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDFCV.RDFCVA.FCVAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVA.FCVAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVA.FCVAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVA.FCVAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVA.FCVAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVA.FCVAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC1V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC1V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC2V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC2V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC3V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC3V |= (uint16_t)data[4 + i*6];


		}
		retVal = 1u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadFCB
 * 			Read the Filtered Cell Voltages C4, C5, C6
 *
 * \retval	2u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadFCB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (!true == adbms6830_read(ADBMS6830_RDFCB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDFCB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDFCV.RDFCVB.FCVBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVB.FCVBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVB.FCVBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVB.FCVBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVB.FCVBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVB.FCVBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC4V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC4V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC5V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC5V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC6V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC6V |= (uint16_t)data[4 + i*6];
		}
		retVal = 2u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadFCC
 * 			Read the Filtered Cell Voltages C7, C8, C9
 *
 * \retval	4u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadFCC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (!true == adbms6830_read(ADBMS6830_RDFCC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDFCC");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDFCV.RDFCVC.FCVCR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVC.FCVCR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVC.FCVCR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVC.FCVCR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVC.FCVCR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVC.FCVCR5 = data[5 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC7V =  (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC7V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC8V =  (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC8V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC9V =  (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC9V |= (uint16_t)data[4 + i*6];
		}
		retVal = 4u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadFCD
 * 			Read the Filtered Cell Voltages C10, C11, C12
 *
 * \retval	8u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadFCD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (!true == adbms6830_read(ADBMS6830_RDFCD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDFCD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDFCV.RDFCVD.FCVDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVD.FCVDR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVD.FCVDR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVD.FCVDR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVD.FCVDR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVD.FCVDR5 = data[5 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC10V =  (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC10V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC11V =  (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC11V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC12V =  (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC12V |= (uint16_t)data[4 + i*6];
		}
		retVal = 8u;
	}
		return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadFCE
 * 			Read the Filtered Cell Voltages C13, C14, C15
 *
 * \retval	16u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadFCE(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (!true == adbms6830_read(ADBMS6830_RDFCE_REG, data, 6))
	{
		dbgPRINT("Error in reading RDFCE");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDFCV.RDFCVE.FCVER0 = data[0 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVE.FCVER1 = data[1 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVE.FCVER2 = data[2 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVE.FCVER3 = data[3 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVE.FCVER4 = data[4 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVE.FCVER5 = data[5 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC13V =  (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC13V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC14V =  (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC14V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC15V =  (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC15V |= (uint16_t)data[4 + i*6];
		}
		retVal = 16u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadFCF
 * 			Read the Filtered Cell Voltage C16
 *
 * \retval	32u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadFCF(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;

	if (!true == adbms6830_read(ADBMS6830_RDFCF_REG, data, 6))
	{
		dbgPRINT("Error in reading RDFCF");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDFCV.RDFCVF.FCVFR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDFCV.RDFCVF.FCVFR1 = data[1 + i*6];

			adbms6830Info.rawData[i].FiltCell.FC16V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].FiltCell.FC16V |= (uint16_t)data[0 + i*6];
		}
		retVal = 32u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSVA
 * 			Read the S-Pin Voltages C1, C2, C3
 *
 * \retval	1u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadSVA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t	retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSVA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSVA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSV.RDSVA.SVAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVA.SVAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVA.SVAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVA.SVAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVA.SVAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVA.SVAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].S_Volt.S1V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S1V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].S_Volt.S2V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S2V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].S_Volt.S3V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S3V |= (uint16_t)data[4 + i*6];

			cellInfo.sPinV[i][0] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S1V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][1] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S2V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][2] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S3V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		}
		retVal = 1u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSVB
 * 			Read the S-Pin Voltages C4, C5, C6
 *
 * \retval	2u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadSVB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSVB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSVB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSV.RDSVB.SVBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVB.SVBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVB.SVBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVB.SVBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVB.SVBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVB.SVBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].S_Volt.S4V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S4V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].S_Volt.S5V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S5V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].S_Volt.S6V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S6V |= (uint16_t)data[4 + i*6];

			cellInfo.sPinV[i][3] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S4V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][4] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S5V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][5] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S6V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		}
		retVal = 2u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSVC
 * 			Read the S-Pin Voltages C7, C8, C9
 *
 * \retval	4u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadSVC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSVC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSVC");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSV.RDSVC.SVCR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVC.SVCR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVC.SVCR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVC.SVCR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVC.SVCR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVC.SVCR5 = data[5 + i*6];

			adbms6830Info.rawData[i].S_Volt.S7V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S7V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].S_Volt.S8V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S8V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].S_Volt.S9V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S9V |= (uint16_t)data[4 + i*6];

			cellInfo.sPinV[i][6] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S7V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][7] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S8V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][8] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S9V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		}
		retVal = 4u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSVD
 * 			Read the S-Pin Voltages C10, C11, C12
 *
 * \retval	8u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadSVD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSVD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSVD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSV.RDSVD.SVDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVD.SVDR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVD.SVDR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVD.SVDR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVD.SVDR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVD.SVDR5 = data[5 + i*6];

			adbms6830Info.rawData[i].S_Volt.S10V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S10V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].S_Volt.S11V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S11V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].S_Volt.S12V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S12V |= (uint16_t)data[4 + i*6];

			cellInfo.sPinV[i][9] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S10V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][10] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S11V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][11] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S12V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		}
		retVal = 8u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSVE
 * 			Read the S-Pin Voltages C13, C14, C15
 *
 * \retval	16u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadSVE(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSVE_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSVE");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSV.RDSVE.SVER0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVE.SVER1 = data[1 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVE.SVER2 = data[2 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVE.SVER3 = data[3 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVE.SVER4 = data[4 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVE.SVER5 = data[5 + i*6];

			adbms6830Info.rawData[i].S_Volt.S13V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S13V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].S_Volt.S14V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S14V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].S_Volt.S15V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S15V |= (uint16_t)data[4 + i*6];

			cellInfo.sPinV[i][12] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S13V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][13] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S14V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.sPinV[i][14] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S15V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		}
		retVal = 16u;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadSVF
 * 			Read the S-Pin Voltage C16
 *
 * \retval	32u : On success
 * \retval	0u : On failure
 *
 *****************************************************************************/
uint8_t adbms6830_ReadSVF(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	uint8_t retVal = 0u;


	if (false == adbms6830_read(ADBMS6830_RDSVF_REG, data, 6))
	{
		dbgPRINT("Error in reading RDSVF");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDSV.RDSVF.SVFR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDSV.RDSVF.SVFR1 = data[1 + i*6];

			adbms6830Info.rawData[i].S_Volt.S16V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].S_Volt.S16V |= (uint16_t)data[0 + i*6];

			cellInfo.sPinV[i][15] = ((int16_t)adbms6830Info.rawData[i].S_Volt.S16V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;

		}
		retVal = 32u;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadAUXA
 * 			Read the GPIO Voltages G1, G2, G3
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadAUXA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDAUXA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDAUXA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDAUX.RDAUXA.GPAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXA.GPAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXA.GPAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXA.GPAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXA.GPAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXA.GPAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Aux.G1V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G1V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Aux.G2V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G2V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Aux.G3V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G3V |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadAUXB
 * 			Read the GPIO Voltages G4, G5, G6
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadAUXB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDAUXB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDAUXB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDAUX.RDAUXB.GPBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXB.GPBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXB.GPBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXB.GPBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXB.GPBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXB.GPBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Aux.G4V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G4V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Aux.G5V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G5V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Aux.G6V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G6V |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadAUXC
 * 			Read the GPIO Voltages G7, G8, G9
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadAUXC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDAUXC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDAUXC");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDAUX.RDAUXC.GPCR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXC.GPCR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXC.GPCR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXC.GPCR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXC.GPCR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXC.GPCR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Aux.G7V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G7V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Aux.G8V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G8V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Aux.G9V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G9V |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadAUXD
 * 			Read the GPIO Voltage G10, VM AND VP
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadAUXD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDAUXD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDAUXD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDAUX.RDAUXD.GPDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXD.GPDR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXD.GPDR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXD.GPDR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXD.GPDR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDAUX.RDAUXD.GPDR5 = data[5 + i*6];

			adbms6830Info.rawData[i].Aux.G10V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.G10V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].Aux.VMV  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.VMV |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].Aux.VPV  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].Aux.VPV |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadRAXA
 * 			Read the redundant GPIO Voltages G1, G2, G3
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadRAXA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDRAXA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDRAXA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDRAX.RDRAXA.RGPAR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXA.RGPAR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXA.RGPAR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXA.RGPAR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXA.RGPAR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXA.RGPAR5 = data[5 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G1V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G1V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G2V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G2V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G3V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G3V |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadRAXB
 * 			Read the redundant GPIO Voltages G4, G5, G6
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadRAXB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDRAXB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDRAXB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDRAX.RDRAXB.RGPBR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXB.RGPBR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXB.RGPBR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXB.RGPBR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXB.RGPBR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXB.RGPBR5 = data[5 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G4V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G4V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G5V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G5V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G6V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G6V |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadRAXC
 * 			Read the redundant GPIO Voltages G7, G8, G9
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadRAXC(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDRAXC_REG, data, 6))
	{
		dbgPRINT("Error in reading RDRAXC");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDRAX.RDRAXC.RGPCR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXC.RGPCR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXC.RGPCR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXC.RGPCR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXC.RGPCR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXC.RGPCR5 = data[5 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G7V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G7V |= (uint16_t)data[0 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G8V  = (uint16_t)data[3 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G8V |= (uint16_t)data[2 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G9V  = (uint16_t)data[5 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G9V |= (uint16_t)data[4 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadRAXD
 * 			Read the redundant GPIO Voltage G10
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadRAXD(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDRAXD_REG, data, 6))
	{
		dbgPRINT("Error in reading RDRAXD");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDRAX.RDRAXD.RGPDR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDRAX.RDRAXD.RGPDR1 = data[1 + i*6];

			adbms6830Info.rawData[i].R_Aux.R_G10V  = (uint16_t)data[1 + i*6] << 8;
			adbms6830Info.rawData[i].R_Aux.R_G10V |= (uint16_t)data[0 + i*6];
		}
		retVal = true;
	}

	return retVal;
}


/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms2950_WriteCFGA
 * 			Write to the Configuration Register Group A
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_WriteCFGA(void)
{
	uint8_t data[6];
	bool retVal = false;


	data[0] = adbms6830Info.reg[0].CFGA.CFGA0.U8;
	data[1] = adbms6830Info.reg[0].CFGA.CFGA1.U8;
	data[2]	= adbms6830Info.reg[0].CFGA.CFGA2.U8;
	data[3] = adbms6830Info.reg[0].CFGA.CFGA3.U8;
	data[4] = adbms6830Info.reg[0].CFGA.CFGA4.U8;
	data[5] = adbms6830Info.reg[0].CFGA.CFGA5.U8;

	if (false == adbms6830_write(ADBMS6830_WRCFGA_REG, data, 6))
	{
		dbgPRINT("Error in writing to WRCFGA");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}


/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms2950_WriteCFGB
 * 			Write to the Configuration Register Group B
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_WriteCFGB(void)
{
	uint8_t data[6];
	bool retVal = false;

	data[0] = adbms6830Info.reg[0].CFGB.CFGB0.U8;
	data[1] = adbms6830Info.reg[0].CFGB.CFGB1.U8;
	data[2]	= adbms6830Info.reg[0].CFGB.CFGB2.U8;
	data[3] = adbms6830Info.reg[0].CFGB.CFGB3.U8;
	data[4] = adbms6830Info.reg[0].CFGB.CFGB4.U8;
	data[5] = adbms6830Info.reg[0].CFGB.CFGB5.U8;

	if (false == adbms6830_write(ADBMS6830_WRCFGB_REG, data, 6))
	{
		dbgPRINT("Error in writing to WRCFGB");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadCOMM
 * 			Read the COMM register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadCOMM(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDCOMM_REG, data, 6))
	{
		dbgPRINT("Error in reading RDCOMM");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDCOMM.COMM0.U8 = data[0 + i*6];
			adbms6830Info.reg[i].RDCOMM.COMM1.D0 = data[1 + i*6];
			adbms6830Info.reg[i].RDCOMM.COMM2.U8 = data[2 + i*6];
			adbms6830Info.reg[i].RDCOMM.COMM3.D1 = data[3 + i*6];
			adbms6830Info.reg[i].RDCOMM.COMM4.U8 = data[4 + i*6];
			adbms6830Info.reg[i].RDCOMM.COMM5.D2 = data[5 + i*6];
		}
		retVal = true;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadPWMA
 * 			Read the PWMA register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadPWMA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDPWMA_REG, data, 6))
	{
		dbgPRINT("Error in reading RDPWMA");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].PWMA.PWMR0.U8 = data[0 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR1.U8 = data[1 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR2.U8 = data[2 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR3.U8 = data[3 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR4.U8 = data[4 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR5.U8 = data[5 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadPWMB
 * 			Read the PWMB register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadPWMB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDPWMB_REG, data, 6))
	{
		dbgPRINT("Error in reading RDPWMB");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].PWMA.PWMR0.U8 = data[0 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR1.U8 = data[1 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR2.U8 = data[2 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR3.U8 = data[3 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR4.U8 = data[4 + i*6];
			adbms6830Info.reg[i].PWMA.PWMR5.U8 = data[5 + i*6];
		}
		retVal = true;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ReadRR
 * 			Read the Retention register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ReadRR(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	if (false == adbms6830_read(ADBMS6830_RDRR_REG, data, 6))
	{
		dbgPRINT("Error in reading RDRR");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].RDRR.RRR0 = data[0 + i*6];
			adbms6830Info.reg[i].RDRR.RRR1 = data[1 + i*6];
			adbms6830Info.reg[i].RDRR.RRR2 = data[2 + i*6];
			adbms6830Info.reg[i].RDRR.RRR3 = data[3 + i*6];
			adbms6830Info.reg[i].RDRR.RRR4 = data[4 + i*6];
			adbms6830Info.reg[i].RDRR.RRR5 = data[5 + i*6];
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_WriteCOMM
 * 			Write to the COMM register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_WriteCOMM(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	data[0] = adbms6830Info.reg[0].RDCOMM.COMM0.U8;
	data[1] = adbms6830Info.reg[0].RDCOMM.COMM1.D0;
	data[2] = adbms6830Info.reg[0].RDCOMM.COMM2.U8;
	data[3] = adbms6830Info.reg[0].RDCOMM.COMM3.D1;
	data[4] = adbms6830Info.reg[0].RDCOMM.COMM4.U8;
	data[5] = adbms6830Info.reg[0].RDCOMM.COMM5.D2;

	if (false == adbms6830_write(ADBMS6830_WRCOMM_REG, data, 6))
	{
		dbgPRINT("Error in writing to WRCOMM");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_WritePWMA
 * 			Write to the PWMA register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_WritePWMA(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;

	data[0] = adbms6830Info.reg[0].PWMA.PWMR0.U8;
	data[1] = adbms6830Info.reg[0].PWMA.PWMR1.U8;
	data[2] = adbms6830Info.reg[0].PWMA.PWMR2.U8;
	data[3] = adbms6830Info.reg[0].PWMA.PWMR3.U8;
	data[4] = adbms6830Info.reg[0].PWMA.PWMR4.U8;
	data[5] = adbms6830Info.reg[0].PWMA.PWMR5.U8;

	if (false == adbms6830_write(ADBMS6830_WRPWMA_REG, data, 6))
	{
		dbgPRINT("Error in writing to WRPWMA");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_WritePWMB
 * 			Write to the PWMB register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_WritePWMB(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	data[0] = adbms6830Info.reg[0].PWMB.PSR0.U8;
	data[1] = adbms6830Info.reg[0].PWMB.PSR1.U8;
	data[2] = adbms6830Info.reg[0].PWMB.PSR2.reserved;
	data[3] = adbms6830Info.reg[0].PWMB.PSR3.reserved;
	data[4] = adbms6830Info.reg[0].PWMB.PSR4.reserved;
	data[5] = adbms6830Info.reg[0].PWMB.PSR5.reserved;

	if (false == adbms6830_write(ADBMS6830_WRPWMB_REG, data, 6))
	{
		dbgPRINT("Error in writing to WRPWMB");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_WriteRR
 * 			Write to the Retention register group
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_WriteRR(void)
{
	uint8_t data[ADBMS6830_NUMBER_OF_NODES * 10];
	bool retVal = false;


	data[0] = adbms6830Info.reg[0].RDRR.RRR0;
	data[1] = adbms6830Info.reg[0].RDRR.RRR1;
	data[2] = adbms6830Info.reg[0].RDRR.RRR2;
	data[3] = adbms6830Info.reg[0].RDRR.RRR3;
	data[4] = adbms6830Info.reg[0].RDRR.RRR4;
	data[5] = adbms6830Info.reg[0].RDRR.RRR5;


	if (true == adbms6830_write(ADBMS6830_ULRR_REG, data, 0))
	{
		if (false == adbms6830_write(ADBMS6830_WRRR_REG, data, 6))
		{
			dbgPRINT("Error in writing to retention register.");
		}
		else
		{
			retVal = true;
		}
	}
	else
	{
		dbgPRINT("Error in unlocking retention register.");
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_clearFaultFlags
 * 			Write to the Clear Fault Flags register to clear the faults indicated in
 * 			Status C register
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_clearFaultFlags(void)
{
	uint8_t data[10];
	bool retVal = false;

	adbms6830Info.reg[0].CLR_FLT.CFDO.U8 = 0xFF;
	adbms6830Info.reg[0].CLR_FLT.CFD1.U8 = 0xFF;
	adbms6830Info.reg[0].CLR_FLT.CFD4.U8 = 0xFF;
	adbms6830Info.reg[0].CLR_FLT.CFD5.U8 = 0xFF;

	data[0] = adbms6830Info.reg[0].CLR_FLT.CFDO.U8;
	data[1] = adbms6830Info.reg[0].CLR_FLT.CFD1.U8;
	data[2] = adbms6830Info.reg[0].CLR_FLT.CFD2;
	data[3] = adbms6830Info.reg[0].CLR_FLT.CFD3;
	data[4] = adbms6830Info.reg[0].CLR_FLT.CFD4.U8;
	data[5] = adbms6830Info.reg[0].CLR_FLT.CFD5.U8;

	if (false == adbms6830_write(ADBMS6830_CLRFLAG_REG, data, 6))
	{
		dbgPRINT("Error in clearing fault flags");
	}
	else
	{
		adbms6830Info.reg[0].CLR_FLT.CFDO.U8 = 0;
		adbms6830Info.reg[0].CLR_FLT.CFD1.U8 = 0;
		adbms6830Info.reg[0].CLR_FLT.CFD4.U8 = 0;
		adbms6830Info.reg[0].CLR_FLT.CFD5.U8 = 0;

		retVal = true;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_clearOVUV
 * 			Write to the Clear OVUV register to clear the faults indicated in
 * 			Status D register
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_clearOVUV(void)
{
	uint8_t data[10];
	bool retVal = false;

	data[0] = 0xFFu;
	data[1] = 0xFFu;
	data[2] = 0xFFu;
	data[3] = 0xFFu;
	data[4] = 0xFFu;
	data[5] = 0xFFu;

	if (false == adbms6830_write(ADBMS6830_CLOVUV_REG, data, 6))
	{
		// clear OVUV registers
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR0.U8 = 0xFF;
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR1.U8 = 0xFF;
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR2.U8 = 0xFF;
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR3.U8 = 0xFF;
		}
		dbgPRINT("Error in clearing OVUV faults");
	}
	else
	{
		for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
		{
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR0.U8 = 0;
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR1.U8 = 0;
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR2.U8 = 0;
			adbms6830Info.reg[i].CLR_OVUV.CL_STDR3.U8 = 0;
		}
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_ConfigureOVUVthresholds
 * 			Configure the cell uv and ov thresholds and write it to CFGB
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_ConfigureOVUVthresholds(void)
{
	int16_t ovThreshold = 0;
	int16_t uvThreshold = 0;
	bool retVal = false;


	ovThreshold = ((ADBMS6830_CELL_OV_THRESHOLD - ADBMS6830_CELL_CONVERSION_FACTOR) * ADBMS6830_CELL_VOLTAGE_CONVERSION) / (ADBMS6830_NO_OF_CELLS * 1000);
	uvThreshold = ((ADBMS6830_CELL_UV_THRESHOLD - ADBMS6830_CELL_CONVERSION_FACTOR) * ADBMS6830_CELL_VOLTAGE_CONVERSION) / (ADBMS6830_NO_OF_CELLS * 1000);

	adbms6830Info.reg[0].CFGB.CFGB0.B.VUV = (uint8_t)uvThreshold;
	adbms6830Info.reg[0].CFGB.CFGB1.B.VUV = (uint8_t)(uvThreshold >> 8 & 0x0F);

	adbms6830Info.reg[0].CFGB.CFGB1.B.VOV = (uint8_t)(ovThreshold & 0x0F);
	adbms6830Info.reg[0].CFGB.CFGB2.B.VOV = (uint8_t)(ovThreshold >> 4);

	if (false == adbms6830_WriteCFGB())
	{
		dbgPRINT("Error in writing CFGB");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_TrigAuxMeasurement()\n
 * 			Triggers Aux ADC conversion
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_TrigAuxMeasurement(void)
{
	uint8_t data[10];
	bool retVal = false;

	if (false == adbms6830_write(ADBMS6830_ADAX_BASE_REG, data,0 ))
	{
		dbgPRINT("error in starting AuxA conversion");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_muteDischarge
 * 			Using the Mute command is used to disable cell discharge of all Sx pins
 * 			simultaneously.  Note: it takes 65us max for the internal switches to
 * 			stop the discharge.
 *
 * \retval	True: On success
 * \retval	False: On Failure
 *
 *****************************************************************************/
static bool adbms6830_muteDischarge(void)
{
	uint8_t data[10];
	bool retVal = false;

	data[0] = 0xFFu;


	if (false == adbms6830_write(ADBMS6830_MUTE_REG, data, 0))
	{
		dbgPRINT("Error in sending mute command");
	}
	else
	{
		retVal = true;
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_checkStatC_Faults
 * 			This function checks the STATC Fault registers for faults and will set the
 * 			appropriate fault bit in the adbms6830Info.fault register
 *
 * \retval	True: Failure has been detected
 * \retval	False: No failures
 *
 *****************************************************************************/
bool adbms6830_checkStatC_Faults(void)
{
	bool retVal = false;

	for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
	{

		if ((ADBMS6830_NOFAULTS < adbms6830Info.reg[i].RDSTAT.STATC.STCR0.U8) || (ADBMS6830_NOFAULTS < adbms6830Info.reg[i].RDSTAT.STATC.STCR1.U8))
		{
			adbms6830Info.faults[i].B.CvsS_Mismatch = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.CvsS_Mismatch = 0;
		}


		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.VA_OV)
		{
			adbms6830Info.faults[i].B.VAOV = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.VAOV = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.VA_UV)
		{
			adbms6830Info.faults[i].B.VAUV = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.VAUV = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.VD_OV)
		{
			adbms6830Info.faults[i].B.VDOV = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.VDOV = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.VD_UV)
		{
			adbms6830Info.faults[i].B.VDUV = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.VDUV = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.CED)
		{
			adbms6830Info.faults[i].B.CED = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.CED = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.CMED)
		{
			adbms6830Info.faults[i].B.CMED = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.CMED = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.SED)
		{
			adbms6830Info.faults[i].B.SED = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.SED = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR4.B.SMED)
		{
			adbms6830Info.faults[i].B.SMED = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.SMED = 0;
		}


		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR5.B.VDEL)
		{
			adbms6830Info.faults[i].B.VDEL = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.VDEL = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR5.B.VDE)
		{
			adbms6830Info.faults[i].B.VDE = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.VDE = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR5.B.COMP)
		{
			adbms6830Info.faults[i].B.COMP = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.COMP = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR5.B.SPIFLT)
		{
			adbms6830Info.faults[i].B.SPIFLT = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.SPIFLT = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR5.B.THSD)
		{
			adbms6830Info.faults[i].B.THSD = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.THSD = 0;
		}

		if (ADBMS6830_FAULT_OCCURED == adbms6830Info.reg[i].RDSTAT.STATC.STCR5.B.OSCCHK)
		{
			adbms6830Info.faults[i].B.OSSCHK = 1;
		}
		else
		{
			adbms6830Info.faults[i].B.OSSCHK = 0;
		}


		if (ADBMS6830_NOFAULTS == (adbms6830Info.faults[i].U32 & ADBMS6830_STATC_BITMASK_FAULT))
		{
			retVal = false;
		}
		else
		{
			retVal = true;
		}
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_checkStatD_Faults
 * 			This function checks the STATD Fault registers for faults and will set the
 * 			appropriate fault bit in the adbms6830Info.fault register
 *
 * \retval	True: Failure has been detected
 * \retval	False: No failures
 *
 *****************************************************************************/
bool adbms6830_checkStatD_Faults(void)
{
	bool retVal = false;

	for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
	{
		if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR0.U8 & ADBMS6830_CxOV_FAULT))
		{
			adbms6830Info.faults[i].B.CxOV = 1;
			dbgPRINT("Cell OV Failed");
			retVal = true;
		}
		else if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR1.U8 & ADBMS6830_CxOV_FAULT))
		{
			adbms6830Info.faults[i].B.CxOV = 1;
			retVal = true;
		}
		else if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR2.U8 & ADBMS6830_CxOV_FAULT))
		{
			adbms6830Info.faults[i].B.CxOV = 1;
			retVal = true;
		}
		else if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR3.U8 & ADBMS6830_CxOV_FAULT))
		{
			adbms6830Info.faults[i].B.CxOV = 1;
			retVal = true;
		}
		else
		{
			adbms6830Info.faults[i].B.CxOV = 0;
		}

		if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR0.U8 & ADBMS6830_CxUV_FAULT))
		{
			adbms6830Info.faults[i].B.CxUV = 1;
			dbgPRINT("Cell UV Failed");
			retVal = true;
		}
		else if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR1.U8 & ADBMS6830_CxUV_FAULT))
		{
			adbms6830Info.faults[i].B.CxUV = 1;
			retVal = true;
		}
		else if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR2.U8 & ADBMS6830_CxUV_FAULT))
		{
			adbms6830Info.faults[i].B.CxUV = 1;
			retVal = true;
		}
		else if (ADBMS6830_NOFAULTS < (adbms6830Info.reg[i].RDSTAT.STATD.STDR3.U8 & ADBMS6830_CxUV_FAULT))
		{
			adbms6830Info.faults[i].B.CxUV = 1;
			retVal = true;
		}
		else
		{
			adbms6830Info.faults[i].B.CxUV = 0;
		}
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_SNAP
 * 			This function freezes all result and status registers.
 *
 * \retval	True:  On Success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_SNAP(void)
{
	uint8_t data[10];
	bool retVal = false;

	if (true == adbms6830_write(ADBMS6830_SNAP_REG, data, 0))
	{
		retVal = true;
	}

	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830_UNSNAP
 * 			This function releases the frozen result and status registers.
 *
 * \retval	True:  On Success
 * \retval	False: On Failure
 *
 *****************************************************************************/
bool adbms6830_UNSNAP(void)
{
	uint8_t data[10];
	bool retVal = true;

	if (false == adbms6830_write(ADBMS6830_UNSNAP_REG, data, 0))
	{
		dbgPRINT("Failed to UNSNAP");
		retVal = false;
	}
	return retVal;
}
