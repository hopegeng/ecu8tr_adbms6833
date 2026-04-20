/**
 *******************************************************************************
 * @file    adbmsCommon.c
 * @author	RL
 * @date    May 2022
 * @brief   The common low level interface to ADBMS family through isoSPI bus
 *
 ******************************************************************************
 * @attention
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
#include <float.h>
#include <math.h>

/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
#include "Cpu/Std/Ifx_Types.h"
#include "Stm/Std/IfxStm.h"
#include "Port/Io/IfxPort_Io.h"
#include "_PinMap/IfxPort_PinMap.h"
//#include "SafeRTOS/app/inc/temp.h"

/* Application includes */
//#include "machine.h"
#if (__RL_SHELL == 1)
#include "shell.h"
#endif /* (__RL_SHELL == 1) */

#if defined(__QSPI_MCAL_DRIVER__)

#else
#if (__BEVOP)
#include <qspi0mstr.h>
#else
#include "qspi2mstr.h"
#endif
#endif //_QSPI_MCAL_DRIVER__

//#include "dout.h"
#include "qspi.h"
#include "qspimstr.h"
//#include "bmsControl.h"
#include "adbmsCommon.h"
/*----- Macros --------------------------------------------------------*/
#define CRC15_POLY	(uint16_t)(0x4599)
#define CRC10_POLY	(uint16_t)(0x8F)

/*----- Static members -------------------------------------------------------*/
__far uint64_t adbmsCommon_isoSPI_wakeup_timeoutUs = 0;
__private0 __far static uint16_t PEC15_TABLE[256];

/*----- Private Functions Prototypes ----------------------------------------------------*/
static uint64_t adbmsCommon_Micros(void);
static void adbmsCommon_Init_PEC15_Table( void );
/**
 ******************************************************************************
 *
 * @author 	RG
 * @date   	Jan2021
 * @brief	Function: ltc2949_isoSPI_wakeup_timeoutUs_init
 * 			Used to keep track of isoSPI LTC6820 timeout
 *
 * @retval	None
 *
 *****************************************************************************/
void adbmsCommon_isoSPI_wakeup_timeoutUs_init(void)
{
	/* store the next timeout time stamp */
	adbmsCommon_isoSPI_wakeup_timeoutUs = adbmsCommon_Micros() + 4300;
} /* ltc2949_isoSPI_wakeup_timeoutUs_init */

static uint64_t adbmsCommon_Micros(void)
{
	return IfxStm_get( &MODULE_STM0 ) / (IfxStm_getFrequency( &MODULE_STM0) / 1000000);
}	/* END of micros */

/**
 ******************************************************************************
 *
 * @author 	RG
 * @date   	Jan2021
 * @brief	Function: adbmsCommon_delayMillseconds
 * 			Delay a period of time in microsecond
 *
 * @param	delay: The delay duration in microsecond
 * @retval	None
 *
 *****************************************************************************/
void adbmsCommon_delayMillseconds(uint32_t delay)
{
#if defined(_RL__FreeRTOS_CPU0__)
	xTaskDelay( delay );
	return;
#else
	uint64_t stm_cnt;


	stm_cnt = IfxStm_get( &MODULE_STM0 ) + ( delay * ((uint64_t)IfxScuCcu_getStmFrequency()) / 1000 );
	while( IfxStm_get( &MODULE_STM0 )  < stm_cnt )
	{
	}
#endif
}	/* END of delayMillseconds */


/**
 ******************************************************************************
 *
 * @author 	RG
 * @date   	Jan2021
 * @brief	Function: adbmsCommon_Init_PEC15_Table
 * 			Initialize the PEC15 table
 *
 * @retval	None.
 *
 *****************************************************************************/
static void adbmsCommon_Init_PEC15_Table( void )
{
	uint16_t i;
	uint16_t bit;
	uint16_t remain;

	for ( i = 0; i < 256; i++)
	{
		remain = i << 7;

		for (bit = 8; bit > 0; --bit)
		{
			if (remain & 0x4000)
			{
				remain = ((remain << 1));
				remain = (remain ^ CRC15_POLY);
			}
			else
			{
				remain = ((remain << 1));
			}
		}

		PEC15_TABLE[i] = remain & (0xFFFFu);
	}
} /* END of init_PEC15_Table */


/**
 ******************************************************************************
 *
 * @author 	RG
 * @date   	Jan2021
 * @brief	Function: calc_pec15
 * 			Calculate the pec15 value of a byte array
 *
 * @param	data: The pointer to a byte array on which PEC15 is calculated
 * @param	len: The length of byte array
 * @retval	calculated PEC15.
 *
 *****************************************************************************/
uint16_t adbmsCommon_Calc_pec15( uint8_t *data , uint32_t len )
{
	uint32_t i;
	uint32_t remain;
	uint32_t address;


	remain = 16;//PEC seed
	for( i = 0; i < len; i++ )
	{
		address = ((remain >> 7) ^ data[i]) & 0xff;//calculate PEC table address
		remain = (remain << 8 ) ^ PEC15_TABLE[address];
	}

	//The CRC15 has a 0 in the LSB so the final value must be multiplied by 2
	return (remain*2);
} /* END of calc_pec15 */


/**
 ******************************************************************************
 *
 * @author 	RG
 * @date   	Jan2021
 * @brief	Function: ltc2949_init
 * 			To initialize the ltc2949 to the default state
 *
 * @retval	None

 *****************************************************************************/
void adbmsCommon_init( void )
{
	adbmsCommon_Init_PEC15_Table();
} /* END of ltc2949_init */

/**
 ******************************************************************************
 *
 * @author 	RL
 * @date   	March 2021
 * @brief	Function: calc_pec10
 * 			Calculate the pec10 value of a byte array
 *
 * @param	data: The pointer to a byte array on which PEC15 is calculated
 * @param 	temp_cmdCnt: The command counter extracted from PEC0
 * @param	len: The length of byte array
 * @retval	calculated PEC15.
 *
 *****************************************************************************/
uint16_t adbmsCommon_Calc_pec10( uint8_t *data ,uint8_t temp_cmdCnt, uint16_t len )
{
	uint16_t pec_calc = 16; //initial value 10000b
	uint16_t pec_temp = 0;
	uint16_t in0, in1, in2, in3, in7 = 0;
	int8_t i, j;
	uint8_t dat_bits[] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
	uint16_t pec_bits[] = {0x001,0x002,0x004,0x008,0x010,0x020,0x040,0x080,0x100,0x200}; //10bits to combine, remember to wipe after use

	//Calculte PEC for data bits
	for ( i = 0; i < len; i++)
	{ //byte DATx...already arranged in order from DATx to DATx-1...etc
		for ( j = 7; j >= 0; j--)
		{ //bits in DATx
			in0 = (((data[i] & dat_bits[j]) ? 0x01 : 0) ^ ((pec_calc & pec_bits[9]) ? 0x01 : 0)); //converts to single bits each side and doing a XOR operation
			in1 = (in0 ^ (pec_calc & pec_bits[0]? 0x01 : 0)) ? 0x02 : 0; //instead of just doing 1 or 0...going a step further knowing where it would be used later
			in2 = (in0 ^ (pec_calc & pec_bits[1]? 0x01 : 0)) ? 0x04 : 0;
			in3 = (in0 ^ (pec_calc & pec_bits[2]? 0x01 : 0)) ? 0x08 : 0;
			in7 = (in0 ^ (pec_calc & pec_bits[6]? 0x01 : 0)) ? 0x80 : 0;

			pec_temp = ((pec_calc & pec_bits[8]) ? pec_bits[9] : 0); //using the operation to check and then fill the bit where it should go
			pec_temp |= ((pec_calc & pec_bits[7]) ? pec_bits[8] : 0);
			pec_temp |= in7;
			pec_temp |= ((pec_calc & pec_bits[5]) ? pec_bits[6] : 0);
			pec_temp |= ((pec_calc & pec_bits[4]) ? pec_bits[5] : 0);
			pec_temp |= ((pec_calc & pec_bits[3]) ? pec_bits[4] : 0);
			pec_temp |= in3;
			pec_temp |= in2;
			pec_temp |= in1;
			pec_temp |= in0;

			pec_calc = pec_temp; //replace initial pec value for the next loop
		}
	}

	//include 6bits of command counter for PEC calculation
	for ( j = 5; j >= 0; j--)
	{ //bits in CNTR (6bits)
		in0 = (((temp_cmdCnt & dat_bits[j]) ? 0x01 : 0) ^ ((pec_calc & pec_bits[9]) ? 0x01 : 0));
		in1 = (in0 ^ (pec_calc & pec_bits[0]? 0x01 : 0)) ? 0x02 : 0;
		in2 = (in0 ^ (pec_calc & pec_bits[1]? 0x01 : 0)) ? 0x04 : 0;
		in3 = (in0 ^ (pec_calc & pec_bits[2]? 0x01 : 0)) ? 0x08 : 0;
		in7 = (in0 ^ (pec_calc & pec_bits[6]? 0x01 : 0)) ? 0x80 : 0;

		pec_temp = (pec_calc & pec_bits[8]) ? pec_bits[9] : 0;
		pec_temp |= (pec_calc & pec_bits[7]) ? pec_bits[8] : 0;
		pec_temp |= in7;
		pec_temp |= (pec_calc & pec_bits[5]) ? pec_bits[6] : 0;
		pec_temp |= (pec_calc & pec_bits[4]) ? pec_bits[5] : 0;
		pec_temp |= (pec_calc & pec_bits[3]) ? pec_bits[4] : 0;
		pec_temp |= in3;
		pec_temp |= in2;
		pec_temp |= in1;
		pec_temp |= in0;

		pec_calc = pec_temp; //replace initial pec value for the next loop
	}
	return pec_calc;
} /* END of adbmsCommon_Calc_pec10 */


