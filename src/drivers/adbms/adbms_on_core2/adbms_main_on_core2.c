/*
 * adbms_main.c
 *
 *  Created on: Apr 22, 2026
 *      Author: hopeg
 */


#include <stdint.h>
#include <stdbool.h>

#include "tools.h"
#include "qspi.h"
#include "qspi0mstr_illd.h"
#include "adbms6830_driver.h"
#include "adbms6830_balance.h"

/* =========================================================
 * macro
 * ========================================================= */

// Define a priority for the interrupt
#define ISR_PRIORITY_STM2_TICK  10

/* =========================================================
 * Project globals
 * ========================================================= */
static Adbms6830_Context_t        g_bmsDrv;
static Adbms6830_BalanceContext_t g_bmsBal;
static Adbms6830_CommandSet_t     g_bmsCmds;
static Adbms6830_Hal_t            g_bmsHal;

/* 1 ms system tick, incremented from a timer ISR */
volatile static uint32_t g_sysTickMs = 0U;

/* =========================================================
 * Platform-specific hooks you must provide
 * ========================================================= */
static Adbms6830_Status_t adbms_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void adbms_DelayUs(uint32_t us);
static void adbms_DelayMs(uint32_t ms);


static void App_FillDefaultCfga( Adbms6830_Context_t *ctx );
static void App_FillDefaultCfgb( Adbms6830_Context_t *ctx );


/*Static functions */
IFX_INTERRUPT(stm2_isr_handler, 0, ISR_PRIORITY_STM2_TICK);
void stm2_isr_handler(void)
{
    // Enable global interrupts to allow higher priority tasks to nest if needed
    IfxCpu_enableInterrupts();

    // Schedule the next compare match (1ms from the current match value)
    // This handles the timing precisely without cumulative drift
    IfxStm_updateCompare(&MODULE_STM2, IfxStm_Comparator_0,
                         IfxStm_getTicksFromMilliseconds(&MODULE_STM2, 1));

    // Increment your global millisecond counter
    g_sysTickMs++;
}

static Adbms6830_Status_t adbms_QspiTransfer( const uint8_t *tx, uint8_t *rx, uint16_t len )
{
	SpiIf_Status retVal;

    if ((tx == 0) || (rx == 0) || (len == 0U))
    {
        return ADBMS6830_ERR_PARAM;
    }

	retVal = qspi0_send_receive_iLLD( eQspiHwCs02, len, rx, rx  );	/* 2 = pec15 */
	if( retVal == SpiIf_Status_ok )
	{
		return ADBMS6830_OK;
	}

	return ADBMS6830_ERR_COMM;
}

static void adbms_DelayUs( uint32_t us )
{
	delay_us_stm2( us );
}

static void adbms_DelayMs( uint32_t us )
{
	delay_ms_stm2( us );
}

static void App_FillDefaultCfga(Adbms6830_Context_t *ctx)
{
    uint8_t ic;

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        /* clear first */
        for (uint8_t i = 0U; i < ADBMS6830_BYTES_PER_CFGR; i++)
        {
            ctx->cfga[ic].data[i] = 0U;
        }

        /* TODO:
           Fill with your approved CFGA bit settings:
           - REFON
           - ADC/filter mode
           - UV/OV settings
           - GPIO / COMM related settings
        */
    }
}

static void App_FillDefaultCfgb(Adbms6830_Context_t *ctx)
{
    uint8_t ic;

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        for (uint8_t i = 0U; i < ADBMS6830_BYTES_PER_CFGR; i++)
        {
            ctx->cfgb[ic].data[i] = 0U;
        }

        /* TODO:
           Fill with your approved CFGB bit settings:
           - DCC bits off at startup
           - DCTO
           - DTMEN
           - other discharge / balancing options
        */
    }
}

/* =========================================================
 * Example main
 * ========================================================= */
void adbms_main_on_core2(void)
{

    /* HAL hooks */
    g_bmsHal.spiTransfer = adbms_QspiTransfer;
    g_bmsHal.delayUs     = adbms_DelayUs;
    g_bmsHal.delayMs     = adbms_DelayMs;

    /* Driver init */
    Adbms6830_Init(&g_bmsDrv, 1U);                 /* example: 1 ADBMS6830 in chain */
    Adbms6830_SetDefaultCommands(&g_bmsCmds);     /* replace placeholder opcodes later */
    Adbms6830_BalanceInit(&g_bmsBal);

    /* Your application policy */
    g_bmsBal.cfg.chargingActive = false;
    g_bmsBal.cfg.faultActive    = false;

    /* Fill config registers before the first CONFIGURE state */
    App_FillDefaultCfga(&g_bmsDrv);
    App_FillDefaultCfgb(&g_bmsDrv);

    /* Optional: force known startup state */
    g_bmsDrv.svcState = ADBMS6830_SVC_BOOT;

    while (1)
    {
        /* keep driver time base updated */
        g_bmsDrv.tickMs = g_sysTickMs;

        /* run state machine */
        (void)Adbms6830_Task(&g_bmsDrv,
                             &g_bmsHal,
                             &g_bmsCmds,
                             100U,   /* measurement period: 100 ms */
                             3U);    /* ADC conversion wait: 3 ms, adjust to your mode */

        /* Example: after fresh measurements, evaluate balancing */
        if (g_bmsDrv.svcState == ADBMS6830_SVC_STANDBY)
        {
            Adbms6830_BalanceEvaluate(&g_bmsBal, &g_bmsDrv);

            /* only apply balancing if charging */
            if (g_bmsBal.cfg.chargingActive == true)
            {
                (void)Adbms6830_SendMute(&g_bmsHal, &g_bmsCmds);
                (void)Adbms6830_BalanceApplyDcc(&g_bmsBal, &g_bmsDrv, &g_bmsHal, &g_bmsCmds);
                (void)Adbms6830_SendUnmute(&g_bmsHal, &g_bmsCmds);
            }
        }

        /* optional: other application work */
    }
}
