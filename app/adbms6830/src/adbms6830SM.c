/**
 *******************************************************************************
 * \file    adbms6830SM.c
 * \author	RL
 * \date    May 2022
 * \brief   Safety Manual routines
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
#include <stdbool.h>

#include "adbms6830SM.h"
#include "adbms6830.h"
#include "adbms6830_reg.h"
#include "adbmsCommon.h"
//#include "shellprintf.h"
#include "shell.h"
#include "ecu8tr_tcp.h"

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830SM_CellMeasurements
 * 			Read the Filtered Cell Voltages and verify that they are within range
 * 			This is the SM-26 VCELL_OOR test.
 *
 * \retval	1u : SM-26 Fault occured
 * \retval	0u : No failure
 *
 *****************************************************************************/
bool adbms6830SM_CellMeasurements(void)
{
	bool retVal = false;

	// send snap command
	if (false == adbms6830_SNAP())
	{
		dbgPRINT("Failed to freeze results.  Do not trust results");
	}

	// retrieve filtered results
	adbms6830_ReadFCA();
	adbms6830_ReadFCB();
	adbms6830_ReadFCC();
	adbms6830_ReadFCD();
	adbms6830_ReadFCE();
	adbms6830_ReadFCF();

	//send unsnap command
	adbms6830_UNSNAP();

	// verify that filtered cell results are within range
	// need to cycle through all the node results
	for (uint8_t i=0; i<ADBMS6830_NUMBER_OF_NODES;i++)
	{
		cellInfo.filtCellV[i][0] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC1V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][1] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC2V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][2] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC3V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][3] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC4V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][4] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC5V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][5] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC6V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][6] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC7V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][7] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC8V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][8] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC9V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][9] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC10V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][10] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC11V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][11] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC12V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][12] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC13V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][13] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC14V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][14] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC15V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.filtCellV[i][15] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC16V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;

		for (uint8_t j=0; j<ADBMS6830_NO_OF_CELLS;j++)
		{
			if ((ADBMS6830_CELL_LOW_RANGE > cellInfo.filtCellV[i][j]) || (ADBMS6830_CELL_HIGH_RANGE < cellInfo.filtCellV[i][j]))
			{
				//cell is out of range.  Set fault flag.
				retVal = true;
			}
		}
	}
	return retVal;
}


/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830SM_Freeze_Seq
 * 			Read the Cell Voltage Group A and clear the results.
 * 			Verify that the values are out of range
 * 			Unsnap and re-read the same cells and verify that they are in range.
 * 			This is the SM-8 SM_FREEZE_SEQ test.
 *
 * \retval	1u : SM-8 Fault occured
 * \retval	0u : No failure
 *
 *****************************************************************************/
bool adbms6830SM_Freeze_Seq(void)
{
	bool retVal = false;
	uint8_t data[1];
	bool cellOutOfRange = false;
	bool cellInRange = false;

	//Send snap command and clear cell voltage register groups
	if (false == adbms6830_SNAP())
	{
		dbgPRINT("Failed to issue SNAP command");
	}

	if(false == adbms6830_write(ADBMS6830_CLRCELL_REG, data, 0))
	{
		dbgPRINT("Failed to clear the cell voltages");
	}
	else
	{
		//wait 2ms
		adbmsCommon_delayMillseconds(1); //
		//Cell voltages have been cleared.  Read Cell Voltage Group A
		if (false == adbms6830_ReadCVA())
		{
			dbgPRINT("Failed to Read CVA");
		}
		else
		{
			//verify that Cells 1, 2 and 3 are out of range
			for (uint8_t i=0; i<ADBMS6830_NUMBER_OF_NODES; i++)
			{
				if (ADBMS6830_CELL_LOW_RANGE < cellInfo.cellV[i][0] || ADBMS6830_CELL_LOW_RANGE < cellInfo.cellV[i][1] || ADBMS6830_CELL_LOW_RANGE < cellInfo.cellV[i][2])
				{
					cellOutOfRange = true;
				}
			}

			adbms6830_UNSNAP();

			//Read cell voltage group A again.  The results should be in range
			if (false == adbms6830_ReadCVA())
			{
				dbgPRINT("Failed to Read CVA");
			}
			else
			{
				for (uint8_t i=0; i<ADBMS6830_NUMBER_OF_NODES; i++)
				{
					//check to see if less than low range
					if (ADBMS6830_CELL_LOW_RANGE > cellInfo.cellV[i][0] || ADBMS6830_CELL_LOW_RANGE > cellInfo.cellV[i][1] || ADBMS6830_CELL_LOW_RANGE > cellInfo.cellV[i][2])
					{
						cellInRange = true;
					}
					//check to see if greater than high range
					if (ADBMS6830_CELL_HIGH_RANGE < cellInfo.cellV[i][0] || ADBMS6830_CELL_HIGH_RANGE < cellInfo.cellV[i][1] || ADBMS6830_CELL_HIGH_RANGE < cellInfo.cellV[i][2])
					{
						cellInRange = true;
					}

				}
			}
		}
	}
	if (true == cellOutOfRange || true == cellInRange )
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
 * \brief	Function: adbms6830SM_TrigAuxMeasurements\n
 * 			1. Clear Aux\n
 * 			2. Send ADAX command with CH[4:0] =b00000 for all GPIOs, LDO, VREF, DIE TEMP\n
 * 			3. Send ADAX2 command with CH[3:0] = b0000 for all GPIOs\n
 *
 * \retval	1u : Failed to Trigger
 * \retval	0u : Trigger successful
 *
 *****************************************************************************/
bool adbms6830SM_TrigAuxMeasurements(void)
{
	bool retVal = true;
	uint8_t data[1];

	//Send Clear Aux Command
	if (true == adbms6830_write(ADBMS6830_CLRAUX_REG, data, 0))
	{
		if (true == adbms6830_write(ADBMS6830_ADAX_BASE_REG, data, 0))
		{
			if (true == adbms6830_write(ADBMS6830_ADAX2_BASE_REG, data, 0))
			{
				retVal = false;
			}
		}
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830SM_TrigADSV\n
 * 			1. Send ADSV command with CONT = 1, DCP =0, OW = 0;
 *
 * \retval	1u : Failed to Trigger
 * \retval	0u : Trigger successful
 *
 *****************************************************************************/
bool adbms6830SM_TrigADSV(void)
{
	bool retVal = true;
	uint8_t data[1];

	//Send Clear Aux Command
	if (true == adbms6830_write((ADBMS6830_ADSV_BASE_REG | ADBMS6830_ADCV_CONT_OPTION), data, 0))
	{
		retVal = false;
	}
	return retVal;
}


/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830SM_Cell_OWD_Even\n
 * 			Trigger open wire detection on even channels\n
 * 			Read even channel baseline
 *
 * \retval	1u : Failed OW test
 * \retval	0u : OW test successful
 *
 *****************************************************************************/
bool adbms6830SM_Cell_OWD_Even(void)
{
	bool retVal = false;
	uint8_t data[1];

	if (true == adbms6830_SNAP())
	{
		if (true == adbms6830_write(ADBMS6830_ADSV_BASE_REG | ADBMS6830_ADSV_OW_EVEN, data, 0))
		{
			if (true == adbms6830SM_Read_Cell_OWD())
			{
				retVal = true;
			}

			//clear s-pin voltages
			if (false == adbms6830_write(ADBMS6830_CLRSPIN_REG , data, 0))
			{
				dbgPRINT("Failed to clear spin");
				retVal = true;
			}
			if (false == adbms6830_UNSNAP())
			{
				dbgPRINT("Failed to Unsnap");
				retVal = true;
			}

		}
		else
		{
			dbgPRINT("Failed to trigger ADSV OW even channels");
			adbms6830Faults.B.SM_TRIGGER_ADSV = 1;
		}
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830SM_Cell_OWD_Odd\n
 * 			Trigger open wire detection on odd channels\n
 * 			Read odd channel baseline
 * 			Read even ow data
 *
 * \retval	1u : Failed OW test
 * \retval	0u : OW test successful
 *
 *****************************************************************************/
bool adbms6830SM_Cell_OWD_Odd(void)
{
	bool retVal = false;
	uint8_t data[1];

	if (true == adbms6830_SNAP())
	{
		if (true == adbms6830SM_Read_Cell_OWD())
		{
			retVal = true;
		}

		//clear s-pin voltages
		if (false == adbms6830_write(ADBMS6830_CLRSPIN_REG , data, 0))
		{
			dbgPRINT("Failed to clear spin");
			retVal = true;
		}

		if (false == adbms6830_write(ADBMS6830_ADSV_BASE_REG | ADBMS6830_ADSV_OW_ODD, data, 0))
		{
			dbgPRINT("Failed to trigger ADSV OW even channels");
			adbms6830Faults.B.SM_TRIGGER_ADSV = 1;
		}
	}
	return retVal;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Mar2022
 * \brief	Function: adbms6830SM_Read_Cell_OWD\n
 * 			Read s-pin and verify if within range
 *
 * \retval	1u : Failed OW test
 * \retval	0u : OW test successful
 *
 *****************************************************************************/
bool adbms6830SM_Read_Cell_OWD(void)
{
	bool retVal = false;

	//read RDSVA - F
	adbms6830_ReadSVA();
	adbms6830_ReadSVB();
	adbms6830_ReadSVC();
	adbms6830_ReadSVD();
	adbms6830_ReadSVE();
	adbms6830_ReadSVF();

	//verify all channels are in range
	for (uint8_t i=0; i<ADBMS6830_NUMBER_OF_NODES; i++)
	{
		for (uint8_t j=0; j<ADBMS6830_NO_OF_CELLS; j++)
		{
			if (ADBMS6830_CELL_LOW_RANGE > cellInfo.sPinV[i][j] || ADBMS6830_CELL_HIGH_RANGE < cellInfo.sPinV[i][j])
			{
				//s-pin voltage out of range  failed OW test
				retVal = true;
			}
		}
	}
	return retVal;
}
