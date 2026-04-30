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

#include "../bmu/bmu_cell_can.h"
#include "../../middleware/can/can_if.h"
#include "../dbc/dbc_trace_interface.h"
#include "bmu_cell_db.h"
#include "bmu_cell_mapping.h"
#include "bmu_csc_acq.h"

#define BMU_TASK_10MS_PERIOD_TICKS      pdMS_TO_TICKS(10u)
#define BMU_TASK_20MS_PERIOD_TICKS      pdMS_TO_TICKS(20u)
#define BMU_TASK_100MS_PERIOD_TICKS     pdMS_TO_TICKS(100u)
#define BMU_TASK_1000MS_PERIOD_TICKS    pdMS_TO_TICKS(1000u)

#ifndef pdTICKS_TO_MS
    #define pdTICKS_TO_MS( xTimeInTicks )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInTicks ) * ( uint64_t ) 1000U ) / ( uint64_t ) configTICK_RATE_HZ ) )
#endif

static void Bmu_SendDemoTraceMessages(uint32_t now_ms);

static void Bmu_SendDemoTraceMessages(uint32_t now_ms)
{
    static uint32_t demoSequence = 0u;
    uint16_t i;

    demoSequence++;

    for (i = 0u; i < DBC_TRACE_MESSAGE_COUNT; i++)
    {
        CanIf_MsgType msg;

        if (!Dbc_TraceMessage_ShouldDemoTransmit(&g_dbcTraceMessages[i]))
        {
            continue;
        }

        Dbc_TraceMessage_BuildDemoPayload(&g_dbcTraceMessages[i],
                                          (demoSequence ^ now_ms),
                                          &msg);
        (void)CanIf_Transmit(&msg);
    }
}

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

static void Bmu_Task_1000ms_FreeRTOS(void *pvParameters)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    (void)pvParameters;

    while (1)
    {
        Bmu_Task_1000ms((uint32_t)pdTICKS_TO_MS(lastWakeTime));
        vTaskDelayUntil(&lastWakeTime, BMU_TASK_1000MS_PERIOD_TICKS);
    }
}

void Bmu_Init(void)
{
    Bmu_CellMapping_InitDefault();
    Bmu_CellDb_Init();
    Bmu_CscAcq_Init();


    xTaskCreate(Bmu_Task_10ms_FreeRTOS, "BMU_10MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
    xTaskCreate(Bmu_Task_20ms_FreeRTOS, "BMU_20MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
    xTaskCreate(Bmu_Task_100ms_FreeRTOS, "BMU_100MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
    xTaskCreate(Bmu_Task_1000ms_FreeRTOS, "BMU_1000MS", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
}

void Bmu_Task_10ms(uint32_t now_ms)
{
    (void)now_ms;
}

void Bmu_Task_20ms(uint32_t now_ms)
{
    Bmu_CscAcq_MainTask_20ms(now_ms);
}

void Bmu_Task_100ms(uint32_t now_ms)
{
    Bmu_CellDb_StaleMonitor(now_ms);
}

void Bmu_Task_1000ms(uint32_t now_ms)
{
#if 0
    (void)Bmu_CellCan_SendAll();
    Bmu_SendDemoTraceMessages(now_ms);
#else
    void run_can_simulation(void);
    run_can_simulation();
#endif
}


