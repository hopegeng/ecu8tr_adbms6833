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
#include "bmu_cfg.h"

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

    if ((rec->valid == false) || (rec->stale == true))
    {
        v_raw = BMU_INVALID_CELL_VOLTAGE_RAW;
        g_raw = BMU_INVALID_GPIO_VOLTAGE_RAW;
        t_raw_u16 = (uint16_t)BMU_INVALID_CELL_TEMP_RAW;
        bal = (BMU_INVALID_BALANCING != 0u) ? true : false;
    }
    else
    {
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
