/*
 * ltc6812_pec.c
 *
 *  Created on: May 4, 2026
 *      Author: rgeng
 */


/******************************************************************************

                            Online C Compiler.
                Code, Compile, Run and Debug C program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define CRC15_POLY	(uint16_t)(0x4599)
#define CRC10_POLY	(uint16_t)(0x8F)

static uint16_t PEC15_TABLE[256];


static uint16_t Adbms_Pec15_Calc(const uint8_t *data, uint16_t len);
static uint16_t Adbms_Pec10_Calc( const uint8_t *data ,uint8_t temp_cmdCnt, uint16_t len );

static uint8_t Adbms6830_ExtractCmdCounter(const uint8_t *payload, uint16_t dataLen)
{
    return (uint8_t)(payload[dataLen] >> 2U);
}

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


//LTC6812 doesn't use the Pec10
# if 0
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
#endif

int main()
{
    uint8_t cmd[2] = { 0x00, 0x12 };
    uint8_t payload[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0x32 };
    uint16_t pec;
    uint8_t cmdCounter;

    printf("Hello World");

    cmdCounter = Adbms6830_ExtractCmdCounter(payload, 6);
    pec = Adbms_Pec15_Calc( payload, 6 );
    printf( "pec = 0x%x\n", pec );

    pec = Adbms_Pec15_Calc( cmd, 2 );
    printf( "pec = 0x%x\n", pec );


    return 0;
}
