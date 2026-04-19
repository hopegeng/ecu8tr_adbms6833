/*
 * dbc_cell_message.c
 *
 *  Created on: Apr 18, 2026
 *      Author: rgeng
 */


#include "dbc_cell_message.h"

/* ---------- local helpers ---------- */

static uint16_t Dbc_ClampU16_FromFloat(float value, float factor, uint16_t max_raw)
{
    float raw_f;

    if (value <= 0.0f)
    {
        return 0u;
    }

    raw_f = value / factor;

    if (raw_f >= (float)max_raw)
    {
        return max_raw;
    }

    return (uint16_t)(raw_f + 0.5f);
}

static int16_t Dbc_ClampS16_FromFloat(float value, float factor)
{
    float raw_f;

    raw_f = value / factor;

    if (raw_f >= 32767.0f)
    {
        return 32767;
    }

    if (raw_f <= -32768.0f)
    {
        return -32768;
    }

    if (raw_f >= 0.0f)
    {
        return (int16_t)(raw_f + 0.5f);
    }
    else
    {
        return (int16_t)(raw_f - 0.5f);
    }
}

/* ---------- conversions ---------- */

uint16_t Dbc_CellVoltage_mV_ToRaw(float value_mV)
{
    return Dbc_ClampU16_FromFloat(value_mV,
                                  DBC_CELL_VOLTAGE_FACTOR_MV,
                                  DBC_CELL_VOLTAGE_RAW_MAX);
}

int16_t Dbc_CellTemp_C_ToRaw(float value_C)
{
    return Dbc_ClampS16_FromFloat(value_C, DBC_CELL_TEMP_FACTOR_C);
}

uint16_t Dbc_GpioVoltage_mV_ToRaw(float value_mV)
{
    return Dbc_ClampU16_FromFloat(value_mV,
                                  DBC_GPIO_VOLTAGE_FACTOR_MV,
                                  DBC_GPIO_VOLTAGE_RAW_MAX);
}

/* ---------- pack ---------- */

bool Dbc_CellMessage_Pack(uint16_t cell_index_1based,
                          const Dbc_CellMessageSignalsType *sig,
                          Dbc_CanMsgType *msg)
{
    uint16_t temp_u16;

    if ((sig == 0) || (msg == 0))
    {
        return false;
    }

    if (!Dbc_CellMessage_IsValidIndex1(cell_index_1based))
    {
        return false;
    }

    msg->id = Dbc_CellMessage_IdFromIndex1(cell_index_1based);
    msg->dlc = DBC_CELLMESSAGE_DLC;
    msg->is_extended = (DBC_CELLMESSAGE_IS_EXTENDED != 0u);

    msg->data[0] = (uint8_t)(sig->cell_voltage_raw_0p1mV & 0xFFu);
    msg->data[1] = (uint8_t)((sig->cell_voltage_raw_0p1mV >> 8) & 0xFFu);

    temp_u16 = (uint16_t)sig->cell_temp_raw_0p01C;
    msg->data[2] = (uint8_t)(temp_u16 & 0xFFu);
    msg->data[3] = (uint8_t)((temp_u16 >> 8) & 0xFFu);

    msg->data[4] = (uint8_t)(sig->gpio_voltage_raw_0p1mV & 0xFFu);
    msg->data[5] = (uint8_t)((sig->gpio_voltage_raw_0p1mV >> 8) & 0xFFu);

    msg->data[6] = sig->balancing ? 0x01u : 0x00u;
    msg->data[7] = 0x00u;

    return true;
}

/* ---------- unpack ---------- */

bool Dbc_CellMessage_Unpack(const Dbc_CanMsgType *msg,
                            uint16_t *cell_index_1based,
                            Dbc_CellMessageSignalsType *sig)
{
    uint16_t temp_u16;

    if ((msg == 0) || (sig == 0))
    {
        return false;
    }

    if ((msg->dlc != DBC_CELLMESSAGE_DLC) ||
        (msg->is_extended != (DBC_CELLMESSAGE_IS_EXTENDED != 0u)) ||
        (!Dbc_CellMessage_IsValidId(msg->id)))
    {
        return false;
    }

    if (cell_index_1based != 0)
    {
        *cell_index_1based = Dbc_CellMessage_Index1FromId(msg->id);
    }

    sig->cell_voltage_raw_0p1mV =
        (uint16_t)((uint16_t)msg->data[0] |
                  ((uint16_t)msg->data[1] << 8));

    temp_u16 =
        (uint16_t)((uint16_t)msg->data[2] |
                  ((uint16_t)msg->data[3] << 8));
    sig->cell_temp_raw_0p01C = (int16_t)temp_u16;

    sig->gpio_voltage_raw_0p1mV =
        (uint16_t)((uint16_t)msg->data[4] |
                  ((uint16_t)msg->data[5] << 8));

    sig->balancing = ((msg->data[6] & 0x01u) != 0u);

    return true;
}
