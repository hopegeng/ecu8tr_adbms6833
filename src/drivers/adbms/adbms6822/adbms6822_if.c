/*
 * adbms6822_if.c
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */


#include <stdint.h>
#include "adbms6822_if.h"
#include "adbms6833_platform.h"
#include <string.h>
#include "tools.h"

#define ADBMS6822_WAKE_DUMMY_LEN    (2u)

static Adbms6822_StateType g_adbms6822State;
static uint64_t adbmsCommon_isoSPI_wakeup_timeoutUs = 0;



void Adbms6822_Init(void)
{
    (void)memset(&g_adbms6822State, 0, sizeof(g_adbms6822State));
	adbmsCommon_isoSPI_wakeup_timeoutUs = get_MicroSecondOnSTM0() + 4300;
    AdbmsPlatform_Init();
    g_adbms6822State.initialized = true;
}

const Adbms6822_StateType *Adbms6822_GetState(void)
{
    return &g_adbms6822State;
}

bool Adbms6822_IsAwake(void)
{
    return g_adbms6822State.link_awake;
}

Bmu_ReturnType Adbms6822_WakeChain( bool force )
{
    uint8_t dummyTx[ADBMS6822_WAKE_DUMMY_LEN] = {0x00u, 0x00u};
    uint8_t dummyRx[ADBMS6822_WAKE_DUMMY_LEN] = {0u, 0u};
    Bmu_ReturnType ret;

    if( (force == false) && g_adbms6822State.initialized == false)
    {
        return BMU_E_NOT_OK;
    }

    ret = AdbmsPlatform_SpiTransfer(dummyTx, dummyRx, ADBMS6822_WAKE_DUMMY_LEN);

    if (ret == BMU_OK)
    {
        g_adbms6822State.link_awake = true;
        g_adbms6822State.last_activity_ms = AdbmsPlatform_GetTimeMs();
        g_adbms6822State.tx_count++;
        g_adbms6822State.rx_count++;
    }


    return ret;
}

Bmu_ReturnType Adbms6822_SendRawFrame(const uint8_t *tx,
                                      uint8_t *rx,
                                      uint16_t len)
{
    Bmu_ReturnType ret;

    if ((tx == (const uint8_t *)0) || (len == 0u))
    {
        return BMU_E_PARAM;
    }

    if (g_adbms6822State.initialized == false)
    {
        return BMU_E_NOT_OK;
    }

    Adbms6822_WakeChain( true );

    ret = AdbmsPlatform_SpiTransfer(tx, rx, len);
    AdbmsPlatform_DelayUs( 500 );		//Delay 500us, important

    if (ret == BMU_OK)
    {
        g_adbms6822State.link_awake = true;
        g_adbms6822State.last_activity_ms = AdbmsPlatform_GetTimeMs();
        g_adbms6822State.tx_count++;
        if (rx != (uint8_t *)0)
        {
            g_adbms6822State.rx_count++;
        }
    }

    return ret;
}
