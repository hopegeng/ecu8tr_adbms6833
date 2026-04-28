/*
 * adbms_main.c
 *
 *  Created on: Apr 22, 2026
 *      Author: hopeg
 */


#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "tools.h"
#include "qspi.h"
#include "qspi0mstr_illd.h"
#include "adbms6833_reg.h"
#include "adbms6833_driver.h"
#include "adbms6833_balance.h"
#include "adbms6833_help.h"
#include "adbms6833_shared.h"
#include "ecu8tr_cmd.h"

/* =========================================================
 * macro
 * ========================================================= */

// Define a priority for the interrupt
#define ISR_PRIORITY_STM2_TICK  12
#define ADBMS6833_CORE2_DEMO_MODE       (0u)
#define ADBMS6833_AUX_PRINT_PERIOD_MS   (10000u)
#if ADBMS6833_ENABLE_AUX_PRINTF
#define ADBMS6833_AUX_PRINTF(...)       PRINTF(__VA_ARGS__)
#else
#define ADBMS6833_AUX_PRINTF(...)       ((void)0)
#endif

#if ADBMS6833_ENABLE_MAIN_DEBUG_PRINTF
#define ADBMS6833_MAIN_DEBUG_PRINTF(...) PRINTF(__VA_ARGS__)
#else
#define ADBMS6833_MAIN_DEBUG_PRINTF(...) ((void)0)
#endif

/* =========================================================
 * Project globals
 * ========================================================= */
static Adbms6833_Context_t        g_bmsDrv;
static Adbms6833_BalanceContext_t g_bmsBal;
static Adbms6833_CommandSet_t     g_bmsCmds;
static Adbms6833_Hal_t            g_bmsHal;

typedef struct
{
    uint32_t sequence;
    Adbms6833_SharedSnapshot_t snapshot;
} Adbms6833_SharedMemory_t;

static volatile Adbms6833_SharedMemory_t g_adbms6833Shared __attribute__ ((section("NC_cpu0_dlmu"))) = {0};

/* 1 ms system tick, incremented from a timer ISR */
volatile static uint32_t g_sysTickMs = 0U;
static bool g_core2BmsRuntimeInit = false;
static ECU8TR_ADBMS6830_State_t adbms6833_state = ECU8TR_ADBMS6830_IDLE;
static uint32_t g_lastPublishedMeasureMs = 0u;
static uint32_t g_sampleCounter = 0u;
static uint32_t g_lastAuxPrintMs = 0u;

/* =========================================================
 * Platform-specific hooks you must provide
 * ========================================================= */
static Adbms6833_Status_t adbms_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
static void adbms_DelayUs(uint32_t us);
static void adbms_DelayMs(uint32_t ms);


static void App_FillDefaultCfga( Adbms6833_Context_t *ctx );
static void App_FillDefaultCfgb( Adbms6833_Context_t *ctx );
static uint16_t App_EncodeCellThreshold_mV(int16_t threshold_mV);
static void App_InitCore2BmsRuntime(void);
static void App_PublishSharedSnapshot(void);
static void App_PublishDemoSnapshot(void);
static void App_PrintAuxMeasurements(void);
static void Adbms6833_SharedMemoryBarrier(void);

static void Adbms6833_SharedMemoryBarrier(void)
{
    __dsync();
}

void Adbms6833_SharedInit(void)
{
    uint8_t afeIdx;
    uint8_t cellIdx;

    g_adbms6833Shared.sequence = 0u;
    g_adbms6833Shared.snapshot.sample_counter = 0u;
    g_adbms6833Shared.snapshot.sample_timestamp_ms = 0u;
    g_adbms6833Shared.snapshot.valid = false;

    for (afeIdx = 0u; afeIdx < ADBMS6833_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < ADBMS6833_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            g_adbms6833Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx] = 0u;
            g_adbms6833Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = 0;
            g_adbms6833Shared.snapshot.balancing[afeIdx][cellIdx] = 0u;
        }
    }
}

void Adbms6833_SharedPublish(const Adbms6833_SharedSnapshot_t *snapshot)
{
    uint8_t afeIdx;
    uint8_t cellIdx;

    if (snapshot == 0)
    {
        return;
    }

    g_adbms6833Shared.sequence++;
    Adbms6833_SharedMemoryBarrier();

    g_adbms6833Shared.snapshot.sample_counter = snapshot->sample_counter;
    g_adbms6833Shared.snapshot.sample_timestamp_ms = snapshot->sample_timestamp_ms;
    g_adbms6833Shared.snapshot.valid = snapshot->valid;

    for (afeIdx = 0u; afeIdx < ADBMS6833_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < ADBMS6833_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            g_adbms6833Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx] =
                snapshot->cell_voltage_mV[afeIdx][cellIdx];
            g_adbms6833Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] =
                snapshot->cell_temp_raw_0p01C[afeIdx][cellIdx];
            g_adbms6833Shared.snapshot.balancing[afeIdx][cellIdx] =
                snapshot->balancing[afeIdx][cellIdx];
        }
    }

    Adbms6833_SharedMemoryBarrier();
    g_adbms6833Shared.sequence++;
    Adbms6833_SharedMemoryBarrier();
}

bool Adbms6833_SharedRead(Adbms6833_SharedSnapshot_t *snapshot)
{
    uint32_t seqStart;
    uint32_t seqEnd;
    uint8_t afeIdx;
    uint8_t cellIdx;

    if (snapshot == 0)
    {
        return false;
    }

    do
    {
        seqStart = g_adbms6833Shared.sequence;
        Adbms6833_SharedMemoryBarrier();

        if ((seqStart & 0x1u) != 0u)
        {
            continue;
        }

        snapshot->sample_counter = g_adbms6833Shared.snapshot.sample_counter;
        snapshot->sample_timestamp_ms = g_adbms6833Shared.snapshot.sample_timestamp_ms;
        snapshot->valid = g_adbms6833Shared.snapshot.valid;

        for (afeIdx = 0u; afeIdx < ADBMS6833_SHARED_AFE_COUNT; afeIdx++)
        {
            for (cellIdx = 0u; cellIdx < ADBMS6833_SHARED_USED_CELLS_PER_AFE; cellIdx++)
            {
                snapshot->cell_voltage_mV[afeIdx][cellIdx] =
                    g_adbms6833Shared.snapshot.cell_voltage_mV[afeIdx][cellIdx];
                snapshot->cell_temp_raw_0p01C[afeIdx][cellIdx] =
                    g_adbms6833Shared.snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx];
                snapshot->balancing[afeIdx][cellIdx] =
                    g_adbms6833Shared.snapshot.balancing[afeIdx][cellIdx];
            }
        }

        Adbms6833_SharedMemoryBarrier();
        seqEnd = g_adbms6833Shared.sequence;
    } while ((seqStart != seqEnd) || ((seqEnd & 0x1u) != 0u));

    return true;
}


/*Static functions */
IFX_INTERRUPT(stm2_isr_handler_adbms6833, 2, ISR_PRIORITY_STM2_TICK);
void stm2_isr_handler_adbms6833(void)
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

static Adbms6833_Status_t adbms_QspiTransfer( const uint8_t *tx, uint8_t *rx, uint16_t len )
{
	SpiIf_Status retVal;

    if ((tx == 0) || (rx == 0) || (len == 0U))
    {
        return ADBMS6833_ERR_PARAM;
    }


	retVal = qspi0_send_receive_iLLD( eQspiHwCs02, len, (uint8_t *)tx, rx );
	if( retVal == SpiIf_Status_ok )
	{
		if (qspimstr_waitForRxDone_iLLD() == true)
		{
			return ADBMS6833_OK;
		}

		return ADBMS6833_ERR_TIMEOUT;
	}

	return ADBMS6833_ERR_COMM;
}

static void adbms_DelayUs( uint32_t us )
{
	delay_us_stm2( us );
}

static void adbms_DelayMs( uint32_t us )
{
	delay_ms_stm2( us );
}

static void App_FillDefaultCfga(Adbms6833_Context_t *ctx)
{
    uint8_t ic;

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        ADBMS6833_CFGA_t cfga;

        (void)memset(&cfga, 0, sizeof(cfga));

        cfga.CFGA0.B.REFON = 1U;
        cfga.CFGA5.B.COMMBK = (ic==(ctx->icCount-1))? 1: 0;
        cfga.CFGA5.B.FC = 7U;

        ctx->cfga[ic].data[0] = cfga.CFGA0.U8;
        ctx->cfga[ic].data[1] = cfga.CFGA1.U8;
        ctx->cfga[ic].data[2] = cfga.CFGA2.U8;
        ctx->cfga[ic].data[3] = cfga.CFGA3.U8;
        ctx->cfga[ic].data[4] = cfga.CFGA4.U8;
        ctx->cfga[ic].data[5] = cfga.CFGA5.U8;
    }
}

static void App_FillDefaultCfgb(Adbms6833_Context_t *ctx)
{
    uint8_t ic;
    uint16_t uvCode = App_EncodeCellThreshold_mV(ADBMS6833_CELL_UV_THRESHOLD);
    uint16_t ovCode = App_EncodeCellThreshold_mV(ADBMS6833_CELL_OV_THRESHOLD);

    for (ic = 0U; ic < ctx->icCount; ic++)
    {
        ADBMS6833_CFGB_t cfgb;

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

    scaled = (((int32_t)threshold_mV - (int32_t)ADBMS6833_CELL_CONVERSION_FACTOR) *
              (int32_t)ADBMS6833_CELL_VOLTAGE_CONVERSION) /
             ((int32_t)ADBMS6833_NO_OF_CELLS * 1000);

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
    Adbms6833_SharedInit();
    IfxCpu_enableInterrupts();
}

static void App_PublishSharedSnapshot(void)
{
    Adbms6833_SharedSnapshot_t snapshot;
    uint8_t afeIdx;
    uint8_t usedCellIdx;

    snapshot.sample_counter = ++g_sampleCounter;
    snapshot.sample_timestamp_ms = g_bmsDrv.lastMeasureMs;
    snapshot.valid = (adbms6833_state == ECU8TR_ADBMS6830_OK);

    for (afeIdx = 0u; afeIdx < ADBMS6833_SHARED_AFE_COUNT; afeIdx++)
    {
        for (usedCellIdx = 0u; usedCellIdx < ADBMS6833_SHARED_USED_CELLS_PER_AFE; usedCellIdx++)
        {
            uint8_t driverCellIdx = (uint8_t)(ADBMS6833_SHARED_FIRST_USED_CELL_0BASED + usedCellIdx);
            uint16_t dccMask = g_bmsBal.result[afeIdx].dccMask;

            snapshot.cell_voltage_mV[afeIdx][usedCellIdx] = g_bmsDrv.cell[afeIdx].mV[driverCellIdx];
            //TODO: will implement the real code to read NTC values from ADBMS6833 CSC board
            snapshot.cell_temp_raw_0p01C[afeIdx][usedCellIdx] = (afeIdx == 0)?2500:3000;
            snapshot.balancing[afeIdx][usedCellIdx] =
                ((dccMask & ((uint16_t)1u << driverCellIdx)) != 0u) ? 1u : 0u;
        }
    }

    Adbms6833_SharedPublish(&snapshot);
}

static void App_PublishDemoSnapshot(void)
{
    Adbms6833_SharedSnapshot_t snapshot;
    uint32_t frameIdx = g_sampleCounter;
    uint8_t afeIdx;
    uint8_t cellIdx;

    snapshot.sample_counter = ++g_sampleCounter;
    snapshot.sample_timestamp_ms = g_sysTickMs;
    snapshot.valid = true;

    for (afeIdx = 0u; afeIdx < ADBMS6833_SHARED_AFE_COUNT; afeIdx++)
    {
        for (cellIdx = 0u; cellIdx < ADBMS6833_SHARED_USED_CELLS_PER_AFE; cellIdx++)
        {
            uint16_t baseVoltage_mV = (uint16_t)(3660u + ((uint16_t)afeIdx * 35u) + ((uint16_t)cellIdx * 8u));
            uint16_t waveform_mV = (uint16_t)((frameIdx + ((uint32_t)afeIdx * 3u) + cellIdx) % 9u);
            int16_t baseTempRaw = (int16_t)(2450 + ((int16_t)afeIdx * 180) + ((int16_t)cellIdx * 12));
            int16_t waveformTempRaw = (int16_t)(((int16_t)((frameIdx + cellIdx) % 7u) - 3) * 8);
            bool balancingActive = (((frameIdx + afeIdx + cellIdx) % 11u) >= 8u);

            snapshot.cell_voltage_mV[afeIdx][cellIdx] = (uint16_t)(baseVoltage_mV + waveform_mV);
            snapshot.cell_temp_raw_0p01C[afeIdx][cellIdx] = (int16_t)(baseTempRaw + waveformTempRaw);
            snapshot.balancing[afeIdx][cellIdx] = balancingActive ? 1u : 0u;
        }
    }

    adbms6833_state = ECU8TR_ADBMS6830_OK;
    g_lastPublishedMeasureMs = snapshot.sample_timestamp_ms;
    Adbms6833_SharedPublish(&snapshot);
}

static void App_PrintAuxMeasurements(void)
{
    uint8_t afeIdx;

    for (afeIdx = 0u; afeIdx < g_bmsDrv.icCount; afeIdx++)
    {
        ADBMS6833_AUX_PRINTF("ADBMS6833 AUX IC%u @%lu ms:", (uint32_t)afeIdx, (uint32_t)g_sysTickMs);
        ADBMS6833_AUX_PRINTF(" GPIO1=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[0]);
        ADBMS6833_AUX_PRINTF(" GPIO2=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[1]);
        ADBMS6833_AUX_PRINTF(" GPIO3=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[2]);
        ADBMS6833_AUX_PRINTF(" GPIO4=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[3]);
        ADBMS6833_AUX_PRINTF(" GPIO5=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[4]);
        ADBMS6833_AUX_PRINTF(" GPIO6=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[5]);
        ADBMS6833_AUX_PRINTF(" GPIO7=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[6]);
        ADBMS6833_AUX_PRINTF(" GPIO8=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[7]);
        ADBMS6833_AUX_PRINTF(" GPIO9=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[8]);
        ADBMS6833_AUX_PRINTF(" GPIO10=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[9]);
        ADBMS6833_AUX_PRINTF(" VMV=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[10]);
        ADBMS6833_AUX_PRINTF(" VPV=%lumV", (uint32_t)g_bmsDrv.aux[afeIdx].mV[11]);
        ADBMS6833_AUX_PRINTF(" VREF2=%lumV", (uint32_t)g_bmsDrv.auxStatus[afeIdx].vref2mV);
        ADBMS6833_AUX_PRINTF(" ITMP=%ldC", (int32_t)g_bmsDrv.auxStatus[afeIdx].itmpDegC);
        ADBMS6833_AUX_PRINTF(" VD=%lumV", (uint32_t)g_bmsDrv.auxStatus[afeIdx].vdmV);
        ADBMS6833_AUX_PRINTF(" VA=%lumV", (uint32_t)g_bmsDrv.auxStatus[afeIdx].vamV);
        ADBMS6833_AUX_PRINTF(" VRES=%lumV", (uint32_t)g_bmsDrv.auxStatus[afeIdx].vresmV);
        ADBMS6833_AUX_PRINTF("\r\n");
    }
}

/* =========================================================
 * Example main
 * ========================================================= */
void adbms6833_main_on_core2(void)
{
    uint32_t lastDemoPublishMs = 0u;

    App_InitCore2BmsRuntime();

#if (ADBMS6833_CORE2_DEMO_MODE == 1u)
    while (1)
    {
        if ((g_sysTickMs - lastDemoPublishMs) >= ADBMS6833_SHARED_SAMPLE_PERIOD_MS)
        {
            App_PublishDemoSnapshot();
            lastDemoPublishMs = g_sysTickMs;
        }
    }
#else
    qspi0mstr_Init_iLLD();

    /* HAL hooks */
    g_bmsHal.spiTransfer = adbms_QspiTransfer;
    g_bmsHal.delayUs     = adbms_DelayUs;
    g_bmsHal.delayMs     = adbms_DelayMs;

    /* Driver init */
    Adbms6833_Init(&g_bmsDrv, ADBMS6833_SHARED_AFE_COUNT);
    Adbms6833_SetDefaultCommands(&g_bmsCmds);     /* replace placeholder opcodes later */
    Adbms6833_BalanceInit(&g_bmsBal);

    /* Your application policy */
    g_bmsBal.cfg.chargingActive = false;
    g_bmsBal.cfg.faultActive    = false;

    /* Fill config registers before the first CONFIGURE state */
    App_FillDefaultCfga(&g_bmsDrv);
    App_FillDefaultCfgb(&g_bmsDrv);

    /* Optional: force known startup state */
    g_bmsDrv.svcState = ADBMS6833_SVC_BOOT;



    while (1)
    {
    	Adbms6833_Status_t status;
        /* keep driver time base updated */
        g_bmsDrv.tickMs = g_sysTickMs;

        /* run state machine */
        status = Adbms6833_Task(&g_bmsDrv,
                             &g_bmsHal,
                             &g_bmsCmds,
                             ADBMS6833_SHARED_SAMPLE_PERIOD_MS);
        if( status != ADBMS6833_OK )
        {
        	adbms6833_state = ECU8TR_ADBMS6830_ERROR;
        	//__debug();
        }
        else
        {
        	adbms6833_state = ECU8TR_ADBMS6830_OK;

        }
        /* After fresh measurements, evaluate balancing */
        /* Loop:
		  1. MUTE (stop balancing)
		  2. Measure cells
		  3. Calculate ΔV
		  4. Update DCC/PWM
		  5. UNMUTE (resume balancing)
         */
        if (g_bmsDrv.svcState == ADBMS6833_SVC_STANDBY)
        {
            Adbms6833_BalanceEvaluate(&g_bmsBal, &g_bmsDrv);

            /* only apply balancing if charging */
            if (g_bmsBal.cfg.chargingActive == true)
            {
                (void)Adbms6833_SendMute(&g_bmsHal, &g_bmsCmds);
                (void)Adbms6833_BalanceApplyDcc(&g_bmsBal, &g_bmsDrv, &g_bmsHal, &g_bmsCmds);
                (void)Adbms6833_SendUnmute(&g_bmsHal, &g_bmsCmds);
            }

            if ((g_bmsDrv.lastMeasureMs != 0u) && (g_bmsDrv.lastMeasureMs != g_lastPublishedMeasureMs))
            {
                App_PublishSharedSnapshot();
                g_lastPublishedMeasureMs = g_bmsDrv.lastMeasureMs;
            }

            if ((g_sysTickMs - g_lastAuxPrintMs) >= ADBMS6833_AUX_PRINT_PERIOD_MS)
            {
                if (Adbms6833_StartAuxConversion(&g_bmsHal, &g_bmsCmds) == ADBMS6833_OK)
                {
                    g_bmsHal.delayMs(8u);

                    if (Adbms6833_ReadAuxVoltagesByGroup(&g_bmsDrv, &g_bmsHal, &g_bmsCmds) == ADBMS6833_OK)
                    {
                        App_PrintAuxMeasurements();
                    }
                }

                g_lastAuxPrintMs = g_sysTickMs;
            }
        }

        /* optional: other application work */
    }
#endif
}

ECU8TR_ADBMS6830_State_t adbms6833_getState( void )
{
	return adbms6833_state;
}
