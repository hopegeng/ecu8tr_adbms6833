/*
 * bmu_main.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#include <stdio.h>
#include <stdlib.h>

/* IFX includes */
#include <Ifx_Types.h>

/* FreeRTOS includes */
#include <FreeRTOS.h>
#include <task.h>

#include "bmu_main.h"

#include "../bmu/bmu_cell_db.h"
#include "../bmu/bmu_cell_mapping.h"
#include "../bmu/bmu_cell_scheduler.h"
#include "../bmu/bmu_csc_acq.h"
#include "can_if.h"
#include "gpio_drv.h"
#include "shell.h"
#include "GETH_OS.h"
#include "eeprom.h"
#include "led.h"
#include "ecu8tr_net.h"
#include "qspi0mstr_illd.h"
//#include "qspi0mstr.h"
#include "adbms6830.h"
#include "adbmsCommon.h"

#define BMU_TASK_10MS_PERIOD_TICKS      pdMS_TO_TICKS(10u)
#define BMU_TASK_20MS_PERIOD_TICKS      pdMS_TO_TICKS(20u)
#define BMU_TASK_100MS_PERIOD_TICKS     pdMS_TO_TICKS(100u)

#ifndef pdTICKS_TO_MS
    #define pdTICKS_TO_MS( xTimeInTicks )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInTicks ) * ( uint64_t ) 1000U ) / ( uint64_t ) configTICK_RATE_HZ ) )
#endif

__private0 ADBMS6830_FUELCELL_INFO_t adbms6830Info = {0};
__private0 ADBMS6830_SM_FAULT_t adbms6830Faults;

static void Bmu_Task_10ms_FreeRTOS(void *pvParameters)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    (void)pvParameters;

    while (1)
    {
        Bmu_Task_10ms((uint32_t)pdTICKS_TO_MS(lastWakeTime));
        vTaskDelayUntil(&lastWakeTime, BMU_TASK_10MS_PERIOD_TICKS);
    }
}

static void Bmu_Task_20ms_FreeRTOS(void *pvParameters)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    (void)pvParameters;

    while (1)
    {
        Bmu_Task_20ms((uint32_t)pdTICKS_TO_MS(lastWakeTime));
        vTaskDelayUntil(&lastWakeTime, BMU_TASK_20MS_PERIOD_TICKS);
    }
}

static void Bmu_Task_100ms_FreeRTOS(void *pvParameters)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    (void)pvParameters;

    while (1)
    {
        Bmu_Task_100ms((uint32_t)pdTICKS_TO_MS(lastWakeTime));
        vTaskDelayUntil(&lastWakeTime, BMU_TASK_100MS_PERIOD_TICKS);
    }
}

void Bmu_Init(void)
{
	gpioDrv_initGPIO();
	eeprom_read_config();
	start_led();

	start_network();
	qspi0mstr_Init_iLLD();
    CanIf_Init();

    Bmu_CellMapping_InitDefault();
    Bmu_CellDb_Init();
    Bmu_CellScheduler_Init();
    Bmu_CscAcq_Init();

    xTaskCreate(Bmu_Task_10ms_FreeRTOS, "BMU_10MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
    xTaskCreate(Bmu_Task_20ms_FreeRTOS, "BMU_20MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
    xTaskCreate(Bmu_Task_100ms_FreeRTOS, "BMU_100MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
}

void Bmu_Task_10ms(uint32_t now_ms)
{
    (void)now_ms;
    Bmu_CellScheduler_10msTask();
}

void Bmu_Task_20ms(uint32_t now_ms)
{
    Bmu_CscAcq_MainTask_20ms(now_ms);
}

void Bmu_Task_100ms(uint32_t now_ms)
{
    Bmu_CellDb_StaleMonitor(now_ms);
}



