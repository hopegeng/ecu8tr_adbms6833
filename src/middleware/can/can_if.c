/*
 * can_if.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#include <freeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <string.h>
#include <Ifx_Types.h>
#include <IfxCan_Can.h>
#include "can_drv.h"
#include "can_if.h"
#include "dbc_trace_interface.h"
#include "tools.h"
#include "bmu_cell_db.h"
#include "bmu_csc_acq.h"
#include "bmu_thresholds.h"
#include "../../drivers/adbms/adbms_runtime_detect.h"

#define CANIF_TX_QUEUE_LENGTH          (128u)
#define CANIF_TX_QUEUE_SEND_TIMEOUT_MS (10u)
#define CANIF_TX_TASK_STACK_WORDS      (configMINIMAL_STACK_SIZE)
#define CANIF_TX_TASK_PRIORITY         (1u)
#define CANIF_LTC_EEPROM_SIZE_BYTES    (1024u)
#define CANIF_M001_CONFIG_CELL_TYPE_MUX    DBC_M001_EOL_MUX_CELL_TYPE
#define CANIF_M001_CONFIG_IC_TYPE_MUX      DBC_M001_EOL_MUX_IC_TYPE
#define CANIF_M001_CONFIG_MODULE_TYPE_MUX  DBC_M001_EOL_MUX_MODULE_TYPE
#define CANIF_M001_CONFIG_PCB_TYPE_MUX     DBC_M001_EOL_MUX_PCB_TYPE

SemaphoreHandle_t canTxSemaphore = NULL;
static QueueHandle_t g_canIfTxQueue = NULL;

typedef struct
{
    uint32_t bmm_control_count;
    uint32_t write_eeprom_count;
    uint32_t read_eeprom_count;
    uint32_t eeprom_request_rejected_count;
    uint32_t start_ltc_count;
    uint32_t balance_cells_count;
    uint32_t configuration_count;
    uint32_t diag_req_count;
    uint32_t threshold_settings_count;
    uint32_t scu1_hs_switch_req_count;
    uint32_t unknown_count;
    uint32_t tx_queued_count;
    uint32_t tx_queue_full_count;
    uint32_t tx_sent_count;
    uint32_t tx_error_count;

    Dbc_BmmControlDigitalOutputsType last_bmm_control;
    Dbc_M001WriteEepromDataType last_write_eeprom;
    Dbc_M001StartLtcTransmissionType last_start_ltc;
    Dbc_M001BalanceCellsType last_balance_cells;
    Dbc_BmuDiagReqType last_diag_req;
    Dbc_ThresholdSettingsType last_threshold_settings;
    Dbc_Scu1HsSwitchReqType last_scu1_hs_switch_req;
    CanIf_MsgType last_configuration;
} CanIf_TesterCommandStateType;

static CanIf_TesterCommandStateType g_canIfTesterCommandState;

static Bmu_ReturnType CanIf_TransmitBlocking(const CanIf_MsgType *msg);
static void CanIf_TxTask(void *pvParameters);
static bool CanIf_RequestLtcEepromWrite(uint16_t address, const uint8_t *data, uint8_t length);
static void CanIf_QueueEepromDataResponse(uint16_t address, uint8_t index, const uint8_t *data, uint8_t length);

static void CanIf_TxTask(void *pvParameters)
{
    CanIf_MsgType msg;

    (void)pvParameters;

    while (1)
    {
        if (xQueueReceive(g_canIfTxQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (CanIf_TransmitBlocking(&msg) == BMU_OK)
            {
                g_canIfTesterCommandState.tx_sent_count++;
            }
            else
            {
                g_canIfTesterCommandState.tx_error_count++;
            }
        }
    }
}

static void CanIf_HandleBmmControlDigitalOutputs(const Dbc_BmmControlDigitalOutputsType *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_bmm_control = *cmd;
    g_canIfTesterCommandState.bmm_control_count++;

    /* Stub: connect this to contactor, relay, and fan output control later. */
}

static void CanIf_QueueEepromDataResponse(uint16_t address, uint8_t index,
                                          const uint8_t *data, uint8_t length)
{
    CanIf_MsgType msg;

    Dbc_M001EepromData_Pack(&msg, address, index, data, length);
    (void)CanIf_Transmit(&msg);
}

static void CanIf_HandleM001WriteEepromData(const Dbc_M001WriteEepromDataType *cmd)
{
    uint8_t data[4];
    uint16_t address;

    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_write_eeprom = *cmd;
    g_canIfTesterCommandState.write_eeprom_count++;

    if (cmd->mux == DBC_M001_EEPROM_READ_MUX)
    {
        /* Read request: tester wants to read 4 bytes at this index */
        address = (uint16_t)((uint16_t)cmd->eeprom_write_index * 4u);
        if (AdbmsRuntime_RequestEepromRead(address, 4u) == false)
        {
            g_canIfTesterCommandState.eeprom_request_rejected_count++;
        }
        else
        {
            g_canIfTesterCommandState.read_eeprom_count++;
        }
        return;
    }

    /* Normal write */
    address = (uint16_t)((uint16_t)cmd->eeprom_write_index * 4u);
    data[0] = (uint8_t)(cmd->eeprom_write_data & 0xFFu);
    data[1] = (uint8_t)((cmd->eeprom_write_data >> 8u) & 0xFFu);
    data[2] = (uint8_t)((cmd->eeprom_write_data >> 16u) & 0xFFu);
    data[3] = (uint8_t)((cmd->eeprom_write_data >> 24u) & 0xFFu);

    if (CanIf_RequestLtcEepromWrite(address, data, 4u) == false)
    {
        g_canIfTesterCommandState.eeprom_request_rejected_count++;
        return;
    }

    /* Echo the written data back immediately so the tester can confirm */
    CanIf_QueueEepromDataResponse(address, cmd->eeprom_write_index, data, 4u);
}

static void CanIf_HandleM001StartLtcTransmission(const Dbc_M001StartLtcTransmissionType *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_start_ltc = *cmd;
    g_canIfTesterCommandState.start_ltc_count++;

    Bmu_CscAcq_StartMeasurement();
}

static void CanIf_HandleM001BalanceCells(const Dbc_M001BalanceCellsType *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_balance_cells = *cmd;
    g_canIfTesterCommandState.balance_cells_count++;

    Bmu_CellDb_SetManualBalanceMask(cmd->balance_mask_20);
    AdbmsRuntime_SetBalanceMask(cmd->balance_mask_20);
}

static void CanIf_HandleM001ConfigurationMessage(const CanIf_MsgType *msg)
{
    uint32_t mux;
    uint8_t major;
    uint8_t minor;
    bool accepted = false;

    if (msg == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_configuration = *msg;
    g_canIfTesterCommandState.configuration_count++;

    mux = Dbc_ReadLe32(&msg->data[0]);
    major = msg->data[4];
    minor = msg->data[5];

    if (mux == CANIF_M001_CONFIG_CELL_TYPE_MUX)
    {
        accepted = AdbmsRuntime_RequestLtcConfigWrite(ADBMS_RUNTIME_LTC_CONFIG_CELL_TYPE, major, minor);
    }
    else if (mux == CANIF_M001_CONFIG_IC_TYPE_MUX)
    {
        accepted = AdbmsRuntime_RequestLtcConfigWrite(ADBMS_RUNTIME_LTC_CONFIG_IC_TYPE, major, minor);
    }
    else if (mux == CANIF_M001_CONFIG_MODULE_TYPE_MUX)
    {
        accepted = AdbmsRuntime_RequestLtcConfigWrite(ADBMS_RUNTIME_LTC_CONFIG_MODULE_TYPE, major, minor);
    }
    else if (mux == CANIF_M001_CONFIG_PCB_TYPE_MUX)
    {
        accepted = AdbmsRuntime_RequestLtcConfigWrite(ADBMS_RUNTIME_LTC_CONFIG_PCB_TYPE, major, minor);
    }

    if (accepted == false)
    {
        g_canIfTesterCommandState.eeprom_request_rejected_count++;
    }
}

static void CanIf_HandleBmuDiagReq(const Dbc_BmuDiagReqType *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_diag_req = *cmd;
    g_canIfTesterCommandState.diag_req_count++;

    /* Stub: connect this to BMU/BMM diagnostic command execution later. */
}

static void CanIf_HandleThresholdSettings(const Dbc_ThresholdSettingsType *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_threshold_settings = *cmd;
    g_canIfTesterCommandState.threshold_settings_count++;

    Bmu_Thresholds_Set(cmd);
}

static void CanIf_HandleScu1HsSwitchReq(const Dbc_Scu1HsSwitchReqType *cmd)
{
    if (cmd == 0)
    {
        return;
    }

    g_canIfTesterCommandState.last_scu1_hs_switch_req = *cmd;
    g_canIfTesterCommandState.scu1_hs_switch_req_count++;

    /* Stub: connect this to high-side switch output request handling later. */
}

void CanIf_PollEepromReadResult(void)
{
    AdbmsRuntime_EepromReadResult_t result;

    if (AdbmsRuntime_TakeEepromReadResult(&result) == false)
    {
        return;
    }

    if (result.valid == false)
    {
        return;
    }

    CanIf_QueueEepromDataResponse(result.address,
                                  (uint8_t)(result.address / 4u),
                                  result.data,
                                  result.length);
}

static bool CanIf_RequestLtcEepromWrite(uint16_t address, const uint8_t *data, uint8_t length)
{
    uint32_t endAddress;

    if ((data == 0) || (length == 0u) || (length > 4u))
    {
        return false;
    }

    endAddress = (uint32_t)address + (uint32_t)length;
    if (endAddress > CANIF_LTC_EEPROM_SIZE_BYTES)
    {
        return false;
    }

    return AdbmsRuntime_RequestEepromWrite(address, data, length);
}


void CanIf_Init(void)
{
    memset((void *)&g_canIfTesterCommandState, 0, sizeof(g_canIfTesterCommandState));

	canTxSemaphore = xSemaphoreCreateBinary();
	if (canTxSemaphore == NULL)
	{
	   // Handle semaphore creation failure
		__debug();
    }

    g_canIfTxQueue = xQueueCreate(CANIF_TX_QUEUE_LENGTH, sizeof(CanIf_MsgType));
    if (g_canIfTxQueue == NULL)
    {
        __debug();
    }

	CanDrv_initCan();

    if (xTaskCreate(CanIf_TxTask,
                    "CAN_TX",
                    CANIF_TX_TASK_STACK_WORDS,
                    NULL,
                    CANIF_TX_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
        __debug();
    }
}

static Bmu_ReturnType CanIf_TransmitBlocking(const CanIf_MsgType *msg)
{
	uint32 low;
	uint32 high;
    TickType_t startTicks;
    BaseType_t semResult;
    IfxCan_Status txStatus;

    if( msg == 0 )
    {
        return BMU_E_PARAM;
    }

    low = bytes_to_u32_le( msg->data );
    high = bytes_to_u32_le( msg->data+4 );

    startTicks = xTaskGetTickCount();

    do
    {
        txStatus = can_transmitCanMessage( msg->id, low, high );

        if( txStatus == IfxCan_Status_ok )
        {
            break;
        }

        if( txStatus != IfxCan_Status_notSentBusy )
        {
            return BMU_E_BUSY;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    } while( (xTaskGetTickCount() - startTicks) < pdMS_TO_TICKS(50) );

    if( txStatus != IfxCan_Status_ok )
    {
        return BMU_E_BUSY;
    }

    semResult = xSemaphoreTake( canTxSemaphore, pdMS_TO_TICKS(400) );
    if( semResult == pdFALSE )
    {
    	return BMU_E_BUSY;
    }


    return BMU_OK;

}

Bmu_ReturnType CanIf_Transmit(const CanIf_MsgType *msg)
{
    if (msg == 0)
    {
        return BMU_E_PARAM;
    }

    if (g_canIfTxQueue == NULL)
    {
        return BMU_E_NOT_OK;
    }

    if (xQueueSendToBack(g_canIfTxQueue,
                         msg,
                         pdMS_TO_TICKS(CANIF_TX_QUEUE_SEND_TIMEOUT_MS)) != pdPASS)
    {
        g_canIfTesterCommandState.tx_queue_full_count++;
        return BMU_E_BUSY;
    }

    g_canIfTesterCommandState.tx_queued_count++;
    return BMU_OK;
}

void CanIf_RxIndication(const CanIf_MsgType *msg)
{
    Dbc_BmmControlDigitalOutputsType bmmControl;
    Dbc_M001WriteEepromDataType writeEeprom;
    Dbc_M001StartLtcTransmissionType startLtc;
    Dbc_M001BalanceCellsType balanceCells;
    Dbc_BmuDiagReqType diagReq;
    Dbc_ThresholdSettingsType thresholdSettings;
    Dbc_Scu1HsSwitchReqType hsSwitchReq;

    if (msg == 0)
    {
        return;
    }

    if (Dbc_BmmControlDigitalOutputs_Unpack(msg, &bmmControl))
    {
        CanIf_HandleBmmControlDigitalOutputs(&bmmControl);
        return;
    }

    if (Dbc_M001WriteEepromData_Unpack(msg, &writeEeprom))
    {
        CanIf_HandleM001WriteEepromData(&writeEeprom);
        return;
    }

    if (Dbc_M001StartLtcTransmission_Unpack(msg, &startLtc))
    {
        CanIf_HandleM001StartLtcTransmission(&startLtc);
        return;
    }

    if (Dbc_M001BalanceCells_Unpack(msg, &balanceCells))
    {
        CanIf_HandleM001BalanceCells(&balanceCells);
        return;
    }

    if (Dbc_IsExpectedExtended8(msg, DBC_M001_CONFIGURATION_MESSAGE_ID))
    {
        CanIf_HandleM001ConfigurationMessage(msg);
        return;
    }

    if (Dbc_BmuDiagReq_Unpack(msg, &diagReq))
    {
        CanIf_HandleBmuDiagReq(&diagReq);
        return;
    }

    if (Dbc_ThresholdSettings_Unpack(msg, &thresholdSettings))
    {
        CanIf_HandleThresholdSettings(&thresholdSettings);
        return;
    }

    if (Dbc_Scu1HsSwitchReq_Unpack(msg, &hsSwitchReq))
    {
        CanIf_HandleScu1HsSwitchReq(&hsSwitchReq);
        return;
    }

    g_canIfTesterCommandState.unknown_count++;
}

