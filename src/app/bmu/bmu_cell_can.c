/*
 * bmu_cell_can.c
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */


#include "../bmu/bmu_cell_can.h"

#include "../../middleware/can/can_if.h"
#include "../bmu/bmu_cell_db.h"
#include "../bmu/bmu_cell_mapping.h"
#include "../bmu/bmu_csc_acq.h"
#include "../bmu/bmu_thresholds.h"
#include "../dbc/dbc_trace_interface.h"
#include "bmu_cfg.h"

#define BMU_CAN_ID_BMMP_CELL_VOLTAGES           (0x1E000001UL)
#define BMU_CAN_ID_BMMP_CELL_TEMPERATURES       (0x1E001001UL)
#define BMU_CAN_ID_BMMP_TERMINAL_TEMPERATURES   (0x1E002001UL)
#define BMU_CAN_ID_M001_TERMINAL_TEMPERATURES   (0x10001801UL)
#define BMU_CAN_ID_M001_MODULE_STATUS           (0x10002801UL)
#define BMU_CAN_ID_M001_MAX_MIN_VOLTAGES        (0x10003801UL)
#define BMU_CAN_ID_M001_IC_INTERNAL_TEMP        (0x10008801UL)
#define BMU_CAN_ID_M001_IC_CELL_SUM_VOLTAGES    (0x1000C801UL)
#define BMU_CAN_ID_M001_OVER_VOLTAGE_FLAGS      (0x1000D801UL)
#define BMU_CAN_ID_M001_UNDER_VOLTAGE_FLAGS     (0x1000E801UL)

static uint16_t g_bmuCellCanNextCellToTx = 0u;
static uint32_t g_bmuCellCanNextCellTxMs = 0u;
static uint32_t g_bmuCellCanNextSummaryTxMs = 0u;

static void Bmu_CellCan_PutLe16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static uint16_t Bmu_CellCan_CellVoltageRawTo_mV(uint16_t raw_0p1mV)
{
    return (uint16_t)(raw_0p1mV / 10u);
}

static int16_t Bmu_CellCan_TempRaw0p01_ToRaw0p1(int16_t raw_0p01C)
{
    return (int16_t)(raw_0p01C / 10);
}

static void Bmu_CellCan_InitExt8(CanIf_MsgType *msg, uint32_t id)
{
    uint8_t i;

    msg->id = id;
    msg->dlc = 8u;
    msg->is_extended = true;
    for (i = 0u; i < 8u; i++)
    {
        msg->data[i] = 0u;
    }
}

static void Bmu_CellCan_PackPayload(const Bmu_CellRecordType *rec, CanIf_MsgType *msg)
{
    uint16_t v_raw;
    uint16_t g_raw;
    uint16_t t_raw_u16;
    bool bal;

    if ((rec == 0) || (msg == 0))
    {
        return;
    }

    if (rec->valid == false)
    {
        v_raw = BMU_INVALID_CELL_VOLTAGE_RAW;
        g_raw = BMU_INVALID_GPIO_VOLTAGE_RAW;
        t_raw_u16 = (uint16_t)BMU_INVALID_CELL_TEMP_RAW;
        bal = (BMU_INVALID_BALANCING != 0u) ? true : false;
    }
    else
    {
        /* Keep publishing the last sampled payload at the CAN cycle time
         * even when the acquisition period is slower than the 1 s bus period.
         */
        v_raw = rec->dbc_cell_sig.cell_voltage_raw_0p1mV;
        g_raw = rec->dbc_cell_sig.gpio_voltage_raw_0p1mV;
        t_raw_u16 = (uint16_t)rec->dbc_cell_sig.cell_temp_raw_0p01C;
        bal = rec->dbc_cell_sig.balancing;
    }

    msg->data[0] = (uint8_t)(v_raw & 0xFFu);
    msg->data[1] = (uint8_t)((v_raw >> 8) & 0xFFu);

    msg->data[2] = (uint8_t)(t_raw_u16 & 0xFFu);
    msg->data[3] = (uint8_t)((t_raw_u16 >> 8) & 0xFFu);

    msg->data[4] = (uint8_t)(g_raw & 0xFFu);
    msg->data[5] = (uint8_t)((g_raw >> 8) & 0xFFu);

    msg->data[6] = bal ? 0x01u : 0x00u;
    msg->data[7] = 0x00u;
}

Bmu_ReturnType Bmu_CellCan_BuildMessage(uint16_t global_cell_0based, void *msg_out)
{
    const Bmu_CellRecordType *rec;
    CanIf_MsgType *msg = (CanIf_MsgType *)msg_out;

    if ((msg == 0) || (!Bmu_IsValidGlobalCell0(global_cell_0based)))
    {
        return BMU_E_PARAM;
    }

    rec = Bmu_CellDb_GetByGlobal0(global_cell_0based);
    if (rec == 0)
    {
        return BMU_E_NOT_OK;
    }

    msg->id = Bmu_CellMessageIdFromGlobal0(global_cell_0based);
    msg->dlc = 8u;
    msg->is_extended = true;

    Bmu_CellCan_PackPayload(rec, msg);

    return BMU_OK;
}

Bmu_ReturnType Bmu_CellCan_SendOne(uint16_t global_cell_0based)
{
    CanIf_MsgType msg;
    Bmu_ReturnType ret;

    ret = Bmu_CellCan_BuildMessage(global_cell_0based, &msg);
    if (ret != BMU_OK)
    {
        return ret;
    }

    return CanIf_Transmit(&msg);
}

Bmu_ReturnType Bmu_CellCan_SendAll(void)
{
    uint16_t cellIdx;
    Bmu_ReturnType ret = BMU_OK;

    for (cellIdx = 0u; cellIdx < BMU_TOTAL_CELLS; cellIdx++)
    {
        ret = Bmu_CellCan_SendOne(cellIdx);
        if (ret != BMU_OK)
        {
            return ret;
        }
    }

    return ret;
}

void Bmu_CellCan_MainTask_10ms(uint32_t now_ms, bool measurementActive)
{
    if (measurementActive == false)
    {
        g_bmuCellCanNextCellToTx = 0u;
        g_bmuCellCanNextCellTxMs = now_ms;
        g_bmuCellCanNextSummaryTxMs = now_ms;
        return;
    }

    if ((uint32_t)(now_ms - g_bmuCellCanNextCellTxMs) >= BMU_CELLMESSAGE_TX_PERIOD_MS)
    {
        (void)Bmu_CellCan_SendOne(g_bmuCellCanNextCellToTx);

        g_bmuCellCanNextCellToTx++;
        if (g_bmuCellCanNextCellToTx >= BMU_TOTAL_CELLS)
        {
            g_bmuCellCanNextCellToTx = 0u;
        }
        g_bmuCellCanNextCellTxMs += BMU_CELLMESSAGE_TX_PERIOD_MS;
    }

    if ((uint32_t)(now_ms - g_bmuCellCanNextSummaryTxMs) >= BMU_CELLMESSAGE_CYCLE_MS)
    {
        (void)Bmu_CellCan_SendMeasurementSummary();
        g_bmuCellCanNextSummaryTxMs += BMU_CELLMESSAGE_CYCLE_MS;
    }
}

/* M001_IC_CellSumVoltages */
/* 0x1000c801 */
static Bmu_ReturnType Bmu_CellCan_SendIcCellSumVoltages(void)
{
    CanIf_MsgType msg;
    AdbmsSharedSnapshot_t snapshot;
    bool useIcStatus = false;
    uint8_t afe;

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_IC_CELL_SUM_VOLTAGES);

    if (Bmu_CscAcq_GetLastSnapshot(&snapshot) == true)
    {
        uint8_t icIdx;

        for (icIdx = 0u; icIdx < BMU_IC_STATUS_COUNT; icIdx++)
        {
            if (snapshot.ic_status_valid[icIdx] != 0u)
            {
                useIcStatus = true;
                break;
            }
        }

        if (useIcStatus == true)
        {
            for (icIdx = 0u; icIdx < BMU_IC_STATUS_COUNT; icIdx++)
            {
                Bmu_CellCan_PutLe16(&msg.data[(uint16_t)icIdx * 2u],
                                    snapshot.ic_cell_sum_raw_0p01V[icIdx]);
            }

            return CanIf_Transmit(&msg);
        }
    }

    for (afe = 0u; afe < BMU_AFE_COUNT_PER_CSC; afe++)
    {
        uint8_t cell;
        uint32_t sum_mV = 0u;

        for (cell = 0u; cell < BMU_CELL_COUNT_PER_AFE; cell++)
        {
            const Bmu_CellRecordType *rec =
                Bmu_CellDb_GetByGlobal0(Bmu_MakeGlobalCellIndex0(afe, cell));

            if ((rec != 0) && (rec->valid == true))
            {
                sum_mV += Bmu_CellCan_CellVoltageRawTo_mV(rec->dbc_cell_sig.cell_voltage_raw_0p1mV);
            }
        }

        Bmu_CellCan_PutLe16(&msg.data[(uint16_t)afe * 2u], (uint16_t)(sum_mV / 10u));
    }

    return CanIf_Transmit(&msg);
}

/* M001_MaxMinVoltages */
/* 0x10003801 */
static Bmu_ReturnType Bmu_CellCan_SendMaxMinVoltages(void)
{
    CanIf_MsgType msg;
    uint16_t minRaw = 0xFFFFu;
    uint16_t maxRaw = 0u;
    uint32_t sumRaw = 0u;
    uint16_t validCount = 0u;
    uint16_t i;

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        const Bmu_CellRecordType *rec = Bmu_CellDb_GetByGlobal0(i);
        uint16_t raw;

        if ((rec == 0) || (rec->valid == false))
        {
            continue;
        }

        raw = rec->dbc_cell_sig.cell_voltage_raw_0p1mV;
        if (raw < minRaw)
        {
            minRaw = raw;
        }
        if (raw > maxRaw)
        {
            maxRaw = raw;
        }
        sumRaw += raw;
        validCount++;
    }

    if (validCount == 0u)
    {
        minRaw = 0u;
    }

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_MAX_MIN_VOLTAGES);
    Bmu_CellCan_PutLe16(&msg.data[0], minRaw);
    Bmu_CellCan_PutLe16(&msg.data[2], maxRaw);
    Bmu_CellCan_PutLe16(&msg.data[4], (validCount == 0u) ? 0u : (uint16_t)(sumRaw / validCount));

    return CanIf_Transmit(&msg);
}

/* BMMp_CellVoltages */
/* 0x0x1E000001 */
static Bmu_ReturnType Bmu_CellCan_SendBmmpCellVoltages(void)
{
    CanIf_MsgType msg;
    uint16_t min_mV = 0xFFFFu;
    uint16_t max_mV = 0u;
    uint32_t sum_mV = 0u;
    uint16_t validCount = 0u;
    uint8_t minCell = 0u;
    uint8_t maxCell = 0u;
    uint16_t i;

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        const Bmu_CellRecordType *rec = Bmu_CellDb_GetByGlobal0(i);
        uint16_t cell_mV;

        if ((rec == 0) || (rec->valid == false))
        {
            continue;
        }

        cell_mV = Bmu_CellCan_CellVoltageRawTo_mV(rec->dbc_cell_sig.cell_voltage_raw_0p1mV);
        if (cell_mV < min_mV)
        {
            min_mV = cell_mV;
            minCell = (uint8_t)(i + 1u);
        }
        if (cell_mV > max_mV)
        {
            max_mV = cell_mV;
            maxCell = (uint8_t)(i + 1u);
        }
        sum_mV += cell_mV;
        validCount++;
    }

    if (validCount == 0u)
    {
        min_mV = 0u;
        max_mV = 0u;
    }

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_BMMP_CELL_VOLTAGES);
    Bmu_CellCan_PutLe16(&msg.data[0], max_mV);
    Bmu_CellCan_PutLe16(&msg.data[2], min_mV);
    Bmu_CellCan_PutLe16(&msg.data[4], (validCount == 0u) ? 0u : (uint16_t)(sum_mV / validCount));
    msg.data[6] = maxCell;
    msg.data[7] = minCell;

    return CanIf_Transmit(&msg);
}

/* M001_ModuleStatus */
/* 0x10002801 */
static Bmu_ReturnType Bmu_CellCan_SendModuleStatus(void)
{
    CanIf_MsgType msg;
    AdbmsSharedSnapshot_t snapshot;
    int16_t termNeg = 0;
    int16_t termPos = 0;
    uint32_t module_mV = 0u;
    uint16_t i;

    if (Bmu_CscAcq_GetLastSnapshot(&snapshot) == true)
    {
        termNeg = snapshot.external_temp_raw_0p01C[0];
        termPos = snapshot.external_temp_raw_0p01C[1];
    }

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        const Bmu_CellRecordType *rec = Bmu_CellDb_GetByGlobal0(i);
        if ((rec != 0) && (rec->valid == true))
        {
            module_mV += Bmu_CellCan_CellVoltageRawTo_mV(rec->dbc_cell_sig.cell_voltage_raw_0p1mV);
        }
    }

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_MODULE_STATUS);
    Bmu_CellCan_PutLe16(&msg.data[0], (uint16_t)termNeg);
    Bmu_CellCan_PutLe16(&msg.data[2], (uint16_t)termPos);
    Bmu_CellCan_PutLe16(&msg.data[6], (uint16_t)(module_mV / 10u));

    return CanIf_Transmit(&msg);
}

/* M001_TerminalTemperatures and BMMp_TerminalTemperatures */
/* 0x10001801 and 0x1e002001 */
static Bmu_ReturnType Bmu_CellCan_SendTerminalTemperatures(void)
{
    CanIf_MsgType msg;
    AdbmsSharedSnapshot_t snapshot;
    int16_t termNeg = 0;
    int16_t termPos = 0;
    uint16_t dewRaw_0p1mV = 0u;

    if (Bmu_CscAcq_GetLastSnapshot(&snapshot) == true)
    {
        termNeg = snapshot.external_temp_raw_0p01C[0];
        termPos = snapshot.external_temp_raw_0p01C[1];
        dewRaw_0p1mV = (uint16_t)(snapshot.dew_sensor_mV * 10u);
    }

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_TERMINAL_TEMPERATURES);
    Bmu_CellCan_PutLe16(&msg.data[2], dewRaw_0p1mV);
    Bmu_CellCan_PutLe16(&msg.data[4], (uint16_t)termNeg);
    Bmu_CellCan_PutLe16(&msg.data[6], (uint16_t)termPos);
    if (CanIf_Transmit(&msg) != BMU_OK)
    {
        return BMU_E_BUSY;
    }

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_BMMP_TERMINAL_TEMPERATURES);
    Bmu_CellCan_PutLe16(&msg.data[0], (uint16_t)Bmu_CellCan_TempRaw0p01_ToRaw0p1((termNeg > termPos) ? termNeg : termPos));
    Bmu_CellCan_PutLe16(&msg.data[2], (uint16_t)Bmu_CellCan_TempRaw0p01_ToRaw0p1((termNeg < termPos) ? termNeg : termPos));
    Bmu_CellCan_PutLe16(&msg.data[4], (uint16_t)Bmu_CellCan_TempRaw0p01_ToRaw0p1((int16_t)((termNeg + termPos) / 2)));
    msg.data[6] = (termNeg > termPos) ? 1u : 2u;
    msg.data[7] = (termNeg < termPos) ? 1u : 2u;

    return CanIf_Transmit(&msg);
}

/* BMMp_CellTemperatures */
/* 0x1e001001 */
static Bmu_ReturnType Bmu_CellCan_SendCellTemperatures(void)
{
    CanIf_MsgType msg;
    int16_t minTemp = 32767;
    int16_t maxTemp = -32768;
    int32_t sumTemp = 0;
    uint16_t validCount = 0u;
    uint8_t minCell = 0u;
    uint8_t maxCell = 0u;
    uint16_t i;

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        const Bmu_CellRecordType *rec = Bmu_CellDb_GetByGlobal0(i);
        int16_t temp;

        if ((rec == 0) || (rec->valid == false))
        {
            continue;
        }

        temp = Bmu_CellCan_TempRaw0p01_ToRaw0p1(rec->dbc_cell_sig.cell_temp_raw_0p01C);
        if (temp < minTemp)
        {
            minTemp = temp;
            minCell = (uint8_t)(i + 1u);
        }
        if (temp > maxTemp)
        {
            maxTemp = temp;
            maxCell = (uint8_t)(i + 1u);
        }
        sumTemp += temp;
        validCount++;
    }

    if (validCount == 0u)
    {
        minTemp = 0;
        maxTemp = 0;
    }

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_BMMP_CELL_TEMPERATURES);
    Bmu_CellCan_PutLe16(&msg.data[0], (uint16_t)maxTemp);
    Bmu_CellCan_PutLe16(&msg.data[2], (uint16_t)minTemp);
    Bmu_CellCan_PutLe16(&msg.data[4], (validCount == 0u) ? 0u : (uint16_t)(sumTemp / (int32_t)validCount));
    msg.data[6] = maxCell;
    msg.data[7] = minCell;

    return CanIf_Transmit(&msg);
}

/* M001_IC_InternalTemperature: The command ADSTAT */
/* 0x10008801UL */
static Bmu_ReturnType Bmu_CellCan_SendIcInternalTemperature(void)
{
    CanIf_MsgType msg;
    AdbmsSharedSnapshot_t snapshot;
    uint8_t icIdx;

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_IC_INTERNAL_TEMP);

    if (Bmu_CscAcq_GetLastSnapshot(&snapshot) == true)
    {
        for (icIdx = 0u; icIdx < BMU_IC_STATUS_COUNT; icIdx++)
        {
            if (snapshot.ic_status_valid[icIdx] != 0u)
            {
                Bmu_CellCan_PutLe16(&msg.data[(uint16_t)icIdx * 2u],
                                    (uint16_t)snapshot.ic_internal_temp_raw_0p01C[icIdx]);
            }
        }
    }

    return CanIf_Transmit(&msg);
}

/* M001_OverVoltageFlags */
/* 0x1000D801 */
static Bmu_ReturnType Bmu_CellCan_SendOverVoltageFlags(void)
{
    CanIf_MsgType msg;
    uint16_t i;

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_OVER_VOLTAGE_FLAGS);

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        const Bmu_CellRecordType *rec = Bmu_CellDb_GetByGlobal0(i);

        if ((rec != 0) && (rec->valid == true) &&
            Bmu_Thresholds_IsCellOv(rec->dbc_cell_sig.cell_voltage_raw_0p1mV))
        {
            msg.data[i >> 3u] |= (uint8_t)(1u << (i & 7u));
        }
    }

    return CanIf_Transmit(&msg);
}

/* M001_UnderVoltageFlags */
/* 0x1000E801 */
static Bmu_ReturnType Bmu_CellCan_SendUnderVoltageFlags(void)
{
    CanIf_MsgType msg;
    uint16_t i;

    Bmu_CellCan_InitExt8(&msg, BMU_CAN_ID_M001_UNDER_VOLTAGE_FLAGS);

    for (i = 0u; i < BMU_TOTAL_CELLS; i++)
    {
        const Bmu_CellRecordType *rec = Bmu_CellDb_GetByGlobal0(i);

        if ((rec != 0) && (rec->valid == true) &&
            Bmu_Thresholds_IsCellUv(rec->dbc_cell_sig.cell_voltage_raw_0p1mV))
        {
            msg.data[i >> 3u] |= (uint8_t)(1u << (i & 7u));
        }
    }

    return CanIf_Transmit(&msg);
}

Bmu_ReturnType Bmu_CellCan_SendMeasurementSummary(void)
{
    Bmu_ReturnType ret;

    ret = Bmu_CellCan_SendIcCellSumVoltages();
    if (ret != BMU_OK) { return ret; }
    ret = Bmu_CellCan_SendModuleStatus();
    if (ret != BMU_OK) { return ret; }
    ret = Bmu_CellCan_SendMaxMinVoltages();
    if (ret != BMU_OK) { return ret; }
    ret = Bmu_CellCan_SendBmmpCellVoltages();
    if (ret != BMU_OK) { return ret; }
    ret = Bmu_CellCan_SendTerminalTemperatures();
    if (ret != BMU_OK) { return ret; }
    ret = Bmu_CellCan_SendCellTemperatures();
    if (ret != BMU_OK) { return ret; }

    ret = Bmu_CellCan_SendIcInternalTemperature();
    if (ret != BMU_OK) { return ret; }
    ret = Bmu_CellCan_SendOverVoltageFlags();
    if (ret != BMU_OK) { return ret; }
    return Bmu_CellCan_SendUnderVoltageFlags();
}
