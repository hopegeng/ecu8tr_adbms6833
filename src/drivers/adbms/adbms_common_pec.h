#ifndef ADBMS_COMMON_PEC_H
#define ADBMS_COMMON_PEC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS_COMMON_CRC15_POLY     ((uint16_t)0x4599u)

static uint16_t AdbmsCommon_Pec15Table[256];

static void AdbmsCommon_InitPec15Table(void)
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
                remain = (uint16_t)((remain << 1u) ^ ADBMS_COMMON_CRC15_POLY);
            }
            else
            {
                remain = (uint16_t)(remain << 1u);
            }
        }

        AdbmsCommon_Pec15Table[i] = remain;
    }
}

static uint16_t AdbmsCommon_Pec15Calc(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t remain = 16u;
    static bool isInit = false;

    if (isInit == false)
    {
        AdbmsCommon_InitPec15Table();
        isInit = true;
    }

    for (i = 0u; i < len; i++)
    {
        uint8_t tableIndex = (uint8_t)(((remain >> 7u) ^ data[i]) & 0xFFu);
        remain = (uint16_t)((remain << 8u) ^ AdbmsCommon_Pec15Table[tableIndex]);
    }

    return (uint16_t)(remain << 1u);
}

#ifdef __cplusplus
}
#endif

#endif /* ADBMS_COMMON_PEC_H */

#if defined(ADBMS_COMMON_ENABLE_PEC10) && !defined(ADBMS_COMMON_PEC10_H)
#define ADBMS_COMMON_PEC10_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static uint16_t AdbmsCommon_Pec10Calc(const uint8_t *data, uint8_t cmdCounter, uint16_t len)
{
    uint16_t pecCalc = 16u;
    uint16_t pecTemp;
    uint16_t in0;
    uint16_t in1;
    uint16_t in2;
    uint16_t in3;
    uint16_t in7;
    int16_t i;
    int16_t j;
    static const uint8_t dataBits[] =
    {
        0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u
    };
    static const uint16_t pecBits[] =
    {
        0x001u, 0x002u, 0x004u, 0x008u, 0x010u,
        0x020u, 0x040u, 0x080u, 0x100u, 0x200u
    };

    for (i = 0; i < (int16_t)len; i++)
    {
        for (j = 7; j >= 0; j--)
        {
            in0 = (uint16_t)((((data[i] & dataBits[j]) != 0u) ? 1u : 0u) ^
                             (((pecCalc & pecBits[9]) != 0u) ? 1u : 0u));
            in1 = (uint16_t)((in0 ^ (((pecCalc & pecBits[0]) != 0u) ? 1u : 0u)) ? 0x002u : 0u);
            in2 = (uint16_t)((in0 ^ (((pecCalc & pecBits[1]) != 0u) ? 1u : 0u)) ? 0x004u : 0u);
            in3 = (uint16_t)((in0 ^ (((pecCalc & pecBits[2]) != 0u) ? 1u : 0u)) ? 0x008u : 0u);
            in7 = (uint16_t)((in0 ^ (((pecCalc & pecBits[6]) != 0u) ? 1u : 0u)) ? 0x080u : 0u);

            pecTemp = ((pecCalc & pecBits[8]) != 0u) ? pecBits[9] : 0u;
            pecTemp |= ((pecCalc & pecBits[7]) != 0u) ? pecBits[8] : 0u;
            pecTemp |= in7;
            pecTemp |= ((pecCalc & pecBits[5]) != 0u) ? pecBits[6] : 0u;
            pecTemp |= ((pecCalc & pecBits[4]) != 0u) ? pecBits[5] : 0u;
            pecTemp |= ((pecCalc & pecBits[3]) != 0u) ? pecBits[4] : 0u;
            pecTemp |= in3;
            pecTemp |= in2;
            pecTemp |= in1;
            pecTemp |= in0;

            pecCalc = pecTemp;
        }
    }

    for (j = 5; j >= 0; j--)
    {
        in0 = (uint16_t)((((cmdCounter & dataBits[j]) != 0u) ? 1u : 0u) ^
                         (((pecCalc & pecBits[9]) != 0u) ? 1u : 0u));
        in1 = (uint16_t)((in0 ^ (((pecCalc & pecBits[0]) != 0u) ? 1u : 0u)) ? 0x002u : 0u);
        in2 = (uint16_t)((in0 ^ (((pecCalc & pecBits[1]) != 0u) ? 1u : 0u)) ? 0x004u : 0u);
        in3 = (uint16_t)((in0 ^ (((pecCalc & pecBits[2]) != 0u) ? 1u : 0u)) ? 0x008u : 0u);
        in7 = (uint16_t)((in0 ^ (((pecCalc & pecBits[6]) != 0u) ? 1u : 0u)) ? 0x080u : 0u);

        pecTemp = ((pecCalc & pecBits[8]) != 0u) ? pecBits[9] : 0u;
        pecTemp |= ((pecCalc & pecBits[7]) != 0u) ? pecBits[8] : 0u;
        pecTemp |= in7;
        pecTemp |= ((pecCalc & pecBits[5]) != 0u) ? pecBits[6] : 0u;
        pecTemp |= ((pecCalc & pecBits[4]) != 0u) ? pecBits[5] : 0u;
        pecTemp |= ((pecCalc & pecBits[3]) != 0u) ? pecBits[4] : 0u;
        pecTemp |= in3;
        pecTemp |= in2;
        pecTemp |= in1;
        pecTemp |= in0;

        pecCalc = pecTemp;
    }

    return pecCalc;
}

#ifdef __cplusplus
}
#endif

#endif /* ADBMS_COMMON_PEC10_H */
