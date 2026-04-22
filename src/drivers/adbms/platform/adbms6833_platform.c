/*
 * adbms6833_platform.c
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#include <string.h>
#include <IfxQspi_SpiMaster.h>
#include "adbms6833_platform.h"
#include "qspi.h"
#include "qspi0mstr_illd.h"

/* ============================================================
 * Stub platform state
 * Replace later with real TC399 QSPI / STM integration
 * ============================================================ */
static bool g_stubEchoMode = true;
static uint32_t g_stubTimeMs = 0u;
static bool g_stubCsAsserted = false;
static QspiCs_t SSEL = eQspiHwCs02;


void AdbmsPlatform_Init(void)
{
    g_stubEchoMode = true;
    g_stubTimeMs = 0u;
    g_stubCsAsserted = false;
    qspi0mstr_Init_iLLD();
}

void AdbmsPlatform_DelayUs(uint32_t delay_us)
{
    /* Stub: coarse time update only */
    g_stubTimeMs += (delay_us / 1000u);
}

uint32_t AdbmsPlatform_GetTimeMs(void)
{
    return g_stubTimeMs;
}

void AdbmsPlatform_SetStubEchoMode(bool enable)
{
    g_stubEchoMode = enable;
}

void AdbmsPlatform_SetStubTimeMs(uint32_t time_ms)
{
    g_stubTimeMs = time_ms;
}

void AdbmsPlatform_CsAssert(void)
{
    g_stubCsAsserted = true;
}

void AdbmsPlatform_CsDeassert(void)
{
    g_stubCsAsserted = false;
}


Bmu_ReturnType AdbmsPlatform_SpiTransfer(const uint8_t *tx,
                                         uint8_t *rx,
                                         uint16_t len)
{
    uint16_t i;

    if ((tx == (const uint8_t *)0) || (len == 0u))
    {
        return BMU_E_PARAM;
    }

    /* Stub behavior:
     * - if rx != NULL and echo mode enabled, copy tx to rx
     * - otherwise fill rx with 0
     */
    if (rx != (uint8_t *)0)
    {
        if (g_stubEchoMode == true)
        {
            for (i = 0u; i < len; i++)
            {
                rx[i] = tx[i];
            }
        }
        else
        {
        	if( SpiIf_Status_ok != qspi0_send_receive_iLLD( SSEL, len, tx, rx ) )
        	{
        		return BMU_E_NOT_OK;
        	}
        }

    	(void)qspimstr_waitForRxDone_iLLD();

    }

    return BMU_OK;
}


uint16_t AdbmsPlatform_Calc_Pec10( uint8_t *data ,uint8_t temp_cmdCnt, uint16_t len )
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


/* ============================================================
 * PEC15 (ADI style)
 * Seed = 0x0010
 * Poly = 0x4599
 * Returned value is left-shifted by 1 per standard usage
 * ============================================================ */
uint16_t AdbmsPlatform_Calc_PEC15(const uint8_t *data, uint16_t len)
{
    uint16_t remainder = 16u; /* seed 0x0010 */
    uint16_t byteIndex;
    uint8_t bit;
    uint8_t currByte;

    if (data == (const uint8_t *)0)
    {
        return 0u;
    }

    for (byteIndex = 0u; byteIndex < len; byteIndex++)
    {
        currByte = data[byteIndex];
        remainder ^= (uint16_t)((uint16_t)currByte << 7);

        for (bit = 0u; bit < 8u; bit++)
        {
            if ((remainder & 0x4000u) != 0u)
            {
                remainder = (uint16_t)((remainder << 1) ^ 0x4599u);
            }
            else
            {
                remainder = (uint16_t)(remainder << 1);
            }
        }
    }

    return (uint16_t)((remainder << 1) & 0xFFFFu);
}

