#ifndef LTC6812_PEC_H
#define LTC6812_PEC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LTC6812_CRC15_POLY    ((uint16_t)0x4599u)

static uint16_t Ltc6812_Pec15Table[256];

static void Ltc6812_InitPec15Table(void)
{
    uint16_t i;
    uint16_t bit;
    uint16_t remain;

    for (i = 0u; i < 256u; i++)
    {
        remain = (uint16_t)(i << 7u);

        for (bit = 8u; bit > 0u; bit--)
        {
            if ((remain & 0x4000u) != 0u)
            {
                remain = (uint16_t)((remain << 1u) ^ LTC6812_CRC15_POLY);
            }
            else
            {
                remain = (uint16_t)(remain << 1u);
            }
        }

        Ltc6812_Pec15Table[i] = remain;
    }
}


static uint16_t Ltc6812_Pec15Calc(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t remain = 16u;
    static bool isInit = false;

    if (isInit == false)
    {
        Ltc6812_InitPec15Table();
        isInit = true;
    }

    for (i = 0u; i < len; i++)
    {
        uint8_t tableIndex = (uint8_t)(((remain >> 7u) ^ data[i]) & 0xFFu);
        remain = (uint16_t)((remain << 8u) ^ Ltc6812_Pec15Table[tableIndex]);
    }

    return (uint16_t)(remain << 1u);
}


#if 0		//The below is also good for LTC6812

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

#endif


#ifdef __cplusplus
}
#endif

#endif /* LTC6812_PEC_H */
