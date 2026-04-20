/*
 * bmu_csc_acq.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "bmu_csc_acq.h"
#include "bmu_cell_db.h"
#include "bmu_cfg.h"
#include "adbms6830.h"
#include <limits.h>

#define BMU_ADBMS6830_CSC_INDEX            (0u)
#define BMU_ADBMS6830_USED_CELL_COUNT      (10u)
#define BMU_ADBMS6830_STATUS_READ_OK       (0x03u)
#define BMU_ADBMS6830_CELL_READ_OK         (0x0Fu)
#define BMU_ADBMS6830_GPIO_RAW_DEFAULT     (0u)
#define BMU_ADBMS6830_BALANCING_DEFAULT    (false)

static bool g_bmuAdbms6830Ready = false;

static uint16_t Bmu_Adbms6830_CellRawToDbc0p1mV(uint16_t adc_raw)
{
    return (uint16_t)(15000u + ((adc_raw * 15u) / 10u));
}

static int16_t Bmu_Adbms6830_ItmpToRaw0p01C(uint16_t adc_raw)
{
    uint32_t temp_mV;
    int32_t temp_centi_c;

    temp_mV = (((uint32_t)adc_raw * (uint32_t)ADBMS6830_CELL_V_LSB) / (uint32_t)ADBMS6830_VOLT_SCALING) +
              (uint32_t)ADBMS6830_CELL_CONVERSION_FACTOR;
    temp_centi_c = ((int32_t)(temp_mV * (uint32_t)ADBMS6830_TEMP_SCALING) / (int32_t)ADBMS6830_MVPERDEGREEC) -
                   (int32_t)ADBMS6830_TEMP_KELVIN;

    if (temp_centi_c > INT16_MAX)
    {
        temp_centi_c = INT16_MAX;
    }
    else if (temp_centi_c < INT16_MIN)
    {
        temp_centi_c = INT16_MIN;
    }

    return (int16_t)temp_centi_c;
}

static bool Bmu_Adbms6830_EnsureReady(void)
{


    if (adbms6830_initADBMS6830() == true)
    {
        g_bmuAdbms6830Ready = true;
    }

    return g_bmuAdbms6830Ready;
}

static uint16_t Bmu_Adbms6830_GetCellAdcRaw(uint8_t cell_index)
{
    switch (cell_index)
    {
    case 0u:  return adbms6830Info.rawData[0].Cell.C1V;
    case 1u:  return adbms6830Info.rawData[0].Cell.C2V;
    case 2u:  return adbms6830Info.rawData[0].Cell.C3V;
    case 3u:  return adbms6830Info.rawData[0].Cell.C4V;
    case 4u:  return adbms6830Info.rawData[0].Cell.C5V;
    case 5u:  return adbms6830Info.rawData[0].Cell.C6V;
    case 6u:  return adbms6830Info.rawData[0].Cell.C7V;
    case 7u:  return adbms6830Info.rawData[0].Cell.C8V;
    case 8u:  return adbms6830Info.rawData[0].Cell.C9V;
    case 9u:  return adbms6830Info.rawData[0].Cell.C10V;
    default:  return 0u;
    }
}

void Bmu_CscAcq_Init(void)
{
    if (adbms6830_initADBMS6830() == false)
    {
    	g_bmuAdbms6830Ready = false;
    }
    else
    {
    	g_bmuAdbms6830Ready = true;
    }
}

void Bmu_CscAcq_MainTask_20ms(uint32_t now_ms)
{
    uint8_t readOk;
    uint8_t statOk;
    uint8_t cell;
    int16_t temp_raw_0p01C;

    if( g_bmuAdbms6830Ready == false )
    {
    	Bmu_CscAcq_Init();
    	return;
    }

    readOk  = adbms6830_ReadCVA();
    readOk |= adbms6830_ReadCVB();
    readOk |= adbms6830_ReadCVC();
    readOk |= adbms6830_ReadCVD();

    if (readOk != BMU_ADBMS6830_CELL_READ_OK)
    {
        return;
    }

    statOk  = adbms6830_ReadStatA();
    statOk |= adbms6830_ReadStatB();
    if (statOk == BMU_ADBMS6830_STATUS_READ_OK)
    {
        temp_raw_0p01C = Bmu_Adbms6830_ItmpToRaw0p01C(adbms6830Info.rawData[0].Status_Voltages.ITMP);
    }
    else
    {
        temp_raw_0p01C = BMU_INVALID_CELL_TEMP_RAW;
    }

    /* Current hardware integration publishes one ADBMS6830 node on CSC0.
     * This provides live values for C001..C010; the remaining BMU messages
     * stay invalid/stale until more CSCs are wired in.
     */
    for( csc = 0u; csc < BMU_CSC_COUNT ; csc++ )
    {
    	for (cell = 0u; cell < BMU_ADBMS6830_USED_CELL_COUNT; cell++)
    	    {
    	        (void)Bmu_CellDb_UpdateMeasurement(BMU_ADBMS6830_CSC_INDEX,
    	                                           cell,
    	                                           Bmu_Adbms6830_CellRawToDbc0p1mV(Bmu_Adbms6830_GetCellAdcRaw(cell)),
    	                                           temp_raw_0p01C,
    	                                           BMU_ADBMS6830_GPIO_RAW_DEFAULT,
    	                                           BMU_ADBMS6830_BALANCING_DEFAULT,
    	                                           now_ms);
    	    }
    }

}

