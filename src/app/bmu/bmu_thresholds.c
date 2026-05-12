/*
 * bmu_thresholds.c
 *
 * Active OV/UV/OT threshold storage.  Defaults are set via static initialiser
 * so the module is safe before any SCU1_LimitConfigInfo frame is received.
 *
 * Thread-safety: Set() is called from the CAN RX context; IsCellOv/IsCellUv
 * are called from the 10 ms FreeRTOS task.  Missing one threshold update by a
 * single cycle is acceptable for this application, so no mutex is needed.
 */

#include "bmu_thresholds.h"

/* Static initialiser applies the safe defaults at link time. */
static Dbc_ThresholdSettingsType g_thresholds = {
    BMU_THRESHOLD_DEFAULT_UV_RAW_0P001V,   /* under_voltage_raw_0p001V */
    BMU_THRESHOLD_DEFAULT_OV_RAW_0P001V,   /* over_voltage_raw_0p001V  */
    BMU_THRESHOLD_DEFAULT_UT_RAW_0P1C,     /* min_temp_raw_0p1C        */
    BMU_THRESHOLD_DEFAULT_OT_RAW_0P1C      /* max_temp_raw_0p1C        */
};

void Bmu_Thresholds_Init(void)
{
    g_thresholds.under_voltage_raw_0p001V = BMU_THRESHOLD_DEFAULT_UV_RAW_0P001V;
    g_thresholds.over_voltage_raw_0p001V  = BMU_THRESHOLD_DEFAULT_OV_RAW_0P001V;
    g_thresholds.min_temp_raw_0p1C        = BMU_THRESHOLD_DEFAULT_UT_RAW_0P1C;
    g_thresholds.max_temp_raw_0p1C        = BMU_THRESHOLD_DEFAULT_OT_RAW_0P1C;
}

void Bmu_Thresholds_Set(const Dbc_ThresholdSettingsType *s)
{
    if (s == 0)
    {
        return;
    }
    g_thresholds = *s;
}

void Bmu_Thresholds_Get(Dbc_ThresholdSettingsType *s_out)
{
    if (s_out == 0)
    {
        return;
    }
    *s_out = g_thresholds;
}

bool Bmu_Thresholds_IsCellOv(uint16_t cell_voltage_raw_0p1mV)
{
    /* cell_voltage_raw_0p1mV : 0.1 mV per bit  (uint16)
     * over_voltage_raw_0p001V : 0.001 V = 1 mV per bit (uint16)
     *
     * Convert OV threshold to the same 0.1 mV/bit scale: multiply by 10.
     * Both operands promoted to uint32 to avoid overflow.
     */
    return ((uint32_t)cell_voltage_raw_0p1mV >
            (uint32_t)g_thresholds.over_voltage_raw_0p001V * 10u);
}

bool Bmu_Thresholds_IsCellUv(uint16_t cell_voltage_raw_0p1mV)
{
    /* UV threshold of 0 means "not configured" — never triggers (unsigned). */
    return ((uint32_t)cell_voltage_raw_0p1mV <
            (uint32_t)g_thresholds.under_voltage_raw_0p001V * 10u);
}
