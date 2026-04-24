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
#include "adbms6830_reg.h"
#include "adbms6830_driver.h"
#include "adbms6830_balance.h"
#include "adbms6830_help.h"

/* =========================================================
 * macro
 * ========================================================= */

// Define a priority for the interrupt
#define ISR_PRIORITY_STM2_TICK  12

/* =========================================================
 * Project globals
 * ========================================================= */
static Adbms6830_Context_t        g_bmsDrv;
static Adbms6830_BalanceContext_t g_bmsBal;
static Adbms6830_CommandSet_t     g_bmsCmds;
static Adbms6830_Hal_t            g_bmsHal;

/* 1 ms system tick, incremented from a timer ISR */
volatile static uint32_t g_sysTickMs = 0U;
static bool g_core2BmsRuntimeInit = false;

/* =========================================================
 * Platform-specific hooks you must provide
 * ========================================================= */
static Adbms6830_Status_t adbms_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void adbms_DelayUs(uint32_t us);
static void adbms_DelayMs(uint32_t ms);


static void App_FillDefaultCfga( Adbms6830_Context_t *ctx );
static void App_FillDefaultCfgb( Adbms6830_Context_t *ctx );
static uint16_t App_EncodeCellThreshold_mV(int16_t threshold_mV);
static void App_InitCore2BmsRuntime(void);


/*Static functions */
IFX_INTERRUPT(stm2_isr_handler, 2, ISR_PRIORITY_STM2_TICK);
void stm2_isr_handler(void)
{
    uint32_t nextCompare;

    // Enable global interrupts to allow higher priority tasks to nest if needed
    IfxCpu_enableInterrupts();

    // Schedule the next compare match using an absolute compare value.
    nextCompare = IfxStm_getTicksFromMilliseconds(&MODULE_STM2, 1U);
    IfxStm_increaseCompare(&MODULE_STM2, IfxStm_Comparator_0, nextCompare);

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

	retVal = qspi0_send_receive_iLLD( eQspiHwCs02, len, (uint8_t *)tx, rx );
	if( retVal == SpiIf_Status_ok )
	{
		if (qspimstr_waitForRxDone_iLLD() == true)
		{
			return ADBMS6830_OK;
		}

		return ADBMS6830_ERR_TIMEOUT;
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
        ADBMS6830_CFGA_t cfga;

        (void)memset(&cfga, 0, sizeof(cfga));

        cfga.CFGA0.B.REFON = 1U;
        cfga.CFGA5.B.COMMBK = 1U;
        cfga.CFGA5.B.FC = 7U;

        ctx->cfga[ic].data[0] = cfga.CFGA0.U8;
        ctx->cfga[ic].data[1] = cfga.CFGA1.U8;
        ctx->cfga[ic].data[2] = cfga.CFGA2.U8;
        ctx->cfga[ic].data[3] = cfga.CFGA3.U8;
        ctx->cfga[ic].data[4] = cfga.CFGA4.U8;
        ctx->cfga[ic].data[5] = cfga.CFGA5.U8;
    }
}

static void App_FillDefaultCfgb(Adbms6830_Context_t *ctx)
{
    uint8_t ic;
    uint16_t uvCode = App_EncodeCellThreshold_mV(ADBMS6830_CELL_UV_THRESHOLD);
    uint16_t ovCode = App_EncodeCellThreshold_mV(ADBMS6830_CELL_OV_THRESHOLD);

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        ADBMS6830_CFGB_t cfgb;

        (void)memset(&cfgb, 0, sizeof(cfgb));

        cfgb.CFGB0.B.VUV = (uint8_t)(uvCode & 0x00FFU);
        cfgb.CFGB1.B.VUV = (uint8_t)((uvCode >> 8U) & 0x0FU);
        cfgb.CFGB1.B.VOV = (uint8_t)(ovCode & 0x000FU);
        cfgb.CFGB2.B.VOV = (uint8_t)((ovCode >> 4U) & 0x00FFU);
        cfgb.CFGB3.B.DCTO = 0U;
        cfgb.CFGB3.B.DTRNG = 0U;
        cfgb.CFGB3.B.DTMEN = 0U;

        ctx->cfgb[ic].data[0] = cfgb.CFGB0.U8;
        ctx->cfgb[ic].data[1] = cfgb.CFGB1.U8;
        ctx->cfgb[ic].data[2] = cfgb.CFGB2.U8;
        ctx->cfgb[ic].data[3] = cfgb.CFGB3.U8;
        ctx->cfgb[ic].data[4] = cfgb.CFGB4.U8;
        ctx->cfgb[ic].data[5] = cfgb.CFGB5.U8;
    }
}

static uint16_t App_EncodeCellThreshold_mV(int16_t threshold_mV)
{
    int32_t scaled;

    scaled = (((int32_t)threshold_mV - (int32_t)ADBMS6830_CELL_CONVERSION_FACTOR) *
              (int32_t)ADBMS6830_CELL_VOLTAGE_CONVERSION) /
             ((int32_t)ADBMS6830_NO_OF_CELLS * 1000);

    return (uint16_t)((uint32_t)scaled & 0x0FFFU);
}

static void App_InitCore2BmsRuntime(void)
{
    IfxStm_CompareConfig stmCompareConfig;

    if (g_core2BmsRuntimeInit == true)
    {
        return;
    }

    IfxStm_initCompareConfig(&stmCompareConfig);
    stmCompareConfig.comparator = IfxStm_Comparator_0;
    stmCompareConfig.compareOffset = IfxStm_ComparatorOffset_0;
    stmCompareConfig.compareSize = IfxStm_ComparatorSize_32Bits;
    stmCompareConfig.ticks = IfxStm_getTicksFromMilliseconds(&MODULE_STM2, 1U);
    stmCompareConfig.triggerPriority = ISR_PRIORITY_STM2_TICK;
    stmCompareConfig.typeOfService = IfxSrc_Tos_cpu2;

    (void)IfxStm_initCompare(&MODULE_STM2, &stmCompareConfig);

    g_sysTickMs = 0U;
    g_core2BmsRuntimeInit = true;
    IfxCpu_enableInterrupts();
}

/* =========================================================
 * Example main
 * ========================================================= */
void adbms_main_on_core2(void)
{

    App_InitCore2BmsRuntime();
    qspi0mstr_Init_iLLD();

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
        /* Loop:
		  1. MUTE (stop balancing)
		  2. Measure cells
		  3. Calculate ΔV
		  4. Update DCC/PWM
		  5. UNMUTE (resume balancing)
         */
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
