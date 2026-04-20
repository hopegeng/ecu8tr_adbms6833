/*
 * adbms6833_platform.c
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */


#include "adbms6833_platform.h"
#include <string.h>

/* ============================================================
 * Stub platform state
 * Replace later with real TC399 QSPI / STM integration
 * ============================================================ */
static bool g_stubEchoMode = true;
static uint32_t g_stubTimeMs = 0u;
static bool g_stubCsAsserted = false;

void AdbmsPlatform_Init(void)
{
    g_stubEchoMode = true;
    g_stubTimeMs = 0u;
    g_stubCsAsserted = false;
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
            (void)memset(rx, 0, len);
        }
    }

    (void)g_stubCsAsserted;
    return BMU_OK;
}

/* ============================================================
 * PEC15 (ADI style)
 * Seed = 0x0010
 * Poly = 0x4599
 * Returned value is left-shifted by 1 per standard usage
 * ============================================================ */
uint16_t AdbmsPlatform_Pec15(const uint8_t *data, uint16_t len)
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

