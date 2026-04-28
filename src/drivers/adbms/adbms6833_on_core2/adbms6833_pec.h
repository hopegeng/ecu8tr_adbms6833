#ifndef ADBMS6833_PEC_H
#define ADBMS6833_PEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ADBMS6833 family communication protection.
 *
 * NOTE:
 * The exact PEC usage for your target silicon / document revision must be
 * confirmed against the official ADI documentation used by your project.
 *
 * This module provides a CRC-15 implementation that matches the common ADI
 * battery-monitor command PEC style and a generic CRC-10 helper you can adapt
 * if your command set uses a 10-bit data PEC.
 */


#define CRC15_POLY	(uint16_t)(0x4599)
#define CRC10_POLY	(uint16_t)(0x8F)

static uint16_t PEC15_TABLE[256];


static uint16_t Adbms_Pec15_Calc(const uint8_t *data, uint16_t len);
static uint16_t Adbms_Pec10_Calc( const uint8_t *data ,uint8_t temp_cmdCnt, uint16_t len );

static void adbms_Init_PEC15_Table( void )
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
}


static uint16_t Adbms_Pec15_Calc( const uint8_t *data , uint16_t len )
{
	uint32_t i;
	uint32_t remain;
	uint32_t address;
	static bool isInit = false;

	if( isInit == false )
	{
		adbms_Init_PEC15_Table();
		isInit = true;
	}

	remain = 16;//PEC seed
	for( i = 0; i < len; i++ )
	{
		address = ((remain >> 7) ^ data[i]) & 0xff;//calculate PEC table address
		remain = (remain << 8 ) ^ PEC15_TABLE[address];
	}

	//The CRC15 has a 0 in the LSB so the final value must be multiplied by 2
	return (remain*2);
} /* END of calc_pec15 */

static uint16_t Adbms_Pec10_Calc( const uint8_t *data ,uint8_t temp_cmdCnt, uint16_t len )
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


#ifdef __cplusplus
}
#endif

#endif /* ADBMS6833_PEC_H */
