/*
 * dbc_trace_interface.h
 *
 * Lightweight CAN/DBC interface derived from John Evans' testCAN log.xlsx
 * trace. IDs are actual 29-bit bus IDs; Vector DBC extended-ID storage flag
 * is not included here.
 */

#ifndef SRC_APP_DBC_DBC_TRACE_INTERFACE_H_
#define SRC_APP_DBC_DBC_TRACE_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>
#include "can_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DBC_TRACE_MESSAGE_COUNT                      (100u)
#define DBC_TRACE_EXTENDED_FRAME                     (1u)

#define DBC_BMM_CONTROL_DIGITAL_OUTPUTS_ID           (0x0A181400UL)
#define DBC_M001_EEPROM_DATA_ID                      (0x10007801UL)
#define DBC_M001_WRITE_EEPROM_DATA_ID                (0x10087801UL)
#define DBC_M001_START_LTC_TRANSMISSION_ID           (0x10088801UL)
#define DBC_M001_BALANCE_CELLS_ID                    (0x12181801UL)
#define DBC_M001_CONFIGURATION_MESSAGE_ID            (0x1808D801UL)
#define DBC_SCU1_HS_SWITCH_REQ_ID                    (0x12182401UL)
#define DBC_BMU_DIAG_COMMAND_ID                      (0x1E05D401UL)
#define DBC_SCU1_LIMIT_CONFIG_INFO_ID                (0x12013401UL)
#define DBC_BMM_DIAG_REQ_ID                          DBC_BMU_DIAG_COMMAND_ID
#define DBC_THRESHOLD_SETTINGS_ID                    DBC_SCU1_LIMIT_CONFIG_INFO_ID
/* Mux value in M001_WriteEEPROMData that triggers an EEPROM read-back response */
#define DBC_M001_EEPROM_READ_MUX                     (0xEE5200UL)

typedef enum
{
    DBC_TRACE_CATEGORY_UNKNOWN = 0,
    DBC_TRACE_CATEGORY_BMM,
    DBC_TRACE_CATEGORY_BMMP,
    DBC_TRACE_CATEGORY_CELL,
    DBC_TRACE_CATEGORY_M001,
    DBC_TRACE_CATEGORY_SCU1
} Dbc_TraceCategoryType;

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    Dbc_TraceCategoryType category;
    const char *name;
    uint16_t observed_count;
} Dbc_TraceMessageDefType;

typedef struct
{
    bool contactor_pos;
    bool contactor_neg;
    bool contactor_pre;
    bool contactor_aux;
    bool vsense_relay_bat_pos;
    bool vsense_relay_bat_neg;
    bool vsense_relay_sys_pos;
    bool vsense_relay_sys_neg;
    bool fan_state;
} Dbc_BmmControlDigitalOutputsType;

typedef struct
{
    uint32_t mux;
    uint8_t eeprom_write_index;
    uint32_t eeprom_write_data;
} Dbc_M001WriteEepromDataType;

typedef struct
{
    uint32_t magic;
    uint8_t action_index;
} Dbc_M001StartLtcTransmissionType;

typedef struct
{
    uint32_t balance_mask_20;
} Dbc_M001BalanceCellsType;

typedef struct
{
    uint16_t diag_cmd;
    uint16_t param1;
    int32_t param2;
} Dbc_BmuDiagReqType;

typedef struct
{
    uint16_t under_voltage_raw_0p001V;
    uint16_t over_voltage_raw_0p001V;
    int16_t min_temp_raw_0p1C;
    int16_t max_temp_raw_0p1C;
} Dbc_ThresholdSettingsType;

typedef struct
{
    uint8_t hs_output1_pct;
    uint8_t hs_output2_pct;
    uint8_t hs_output3_pct;
    uint8_t hs_output4_pct;
} Dbc_Scu1HsSwitchReqType;

static const Dbc_TraceMessageDefType g_dbcTraceMessages[DBC_TRACE_MESSAGE_COUNT] =
{
    { 0x0A181400UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_ControlDigitalOutputs", 677u },
    { 0x1E013001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag0", 239u },
    { 0x1E014001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag1", 239u },
    { 0x1E015001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag2", 240u },
    { 0x1E016001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag3", 241u },
    { 0x1E017001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag4", 241u },
    { 0x1E018001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag5", 239u },
    { 0x1E019001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag6", 239u },
    { 0x1E01A001UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_Diag7", 239u },
    { 0x02080400UL, 8u, DBC_TRACE_CATEGORY_BMM, "BMM_SCUSyncMsg", 34u },
    { 0x1E007001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_AlarmFlag", 240u },
    { 0x1E001001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_CellTemperatures", 240u },
    { 0x1E000001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_CellVoltages", 240u },
    { 0x1E003001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_Electrics", 238u },
    { 0x1E006001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_ErrorFlag", 239u },
    { 0x1E009001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_Error_info", 240u },
    { 0x1E005001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_IsolationResistance", 238u },
    { 0x1E00E001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_SOH", 240u },
    { 0x1E004001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_State", 241u },
    { 0x1E002001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_TerminalTemperatures", 240u },
    { 0x1E008001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_WarningFlag", 241u },
    { 0x1E00D001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_capacity", 240u },
    { 0x1E00F001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_energy", 238u },
    { 0x1E012001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_info", 238u },
    { 0x1E00A001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_iso_Voltages", 239u },
    { 0x1E00B001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_limits1", 242u },
    { 0x1E00C001UL, 8u, DBC_TRACE_CATEGORY_BMMP, "BMMp_limits2", 241u },
    { 0x10000C01UL, 8u, DBC_TRACE_CATEGORY_CELL, "C001_CellMessage", 68u },
    { 0x10000C02UL, 8u, DBC_TRACE_CATEGORY_CELL, "C002_CellMessage", 67u },
    { 0x10000C03UL, 8u, DBC_TRACE_CATEGORY_CELL, "C003_CellMessage", 65u },
    { 0x10000C04UL, 8u, DBC_TRACE_CATEGORY_CELL, "C004_CellMessage", 66u },
    { 0x10000C05UL, 8u, DBC_TRACE_CATEGORY_CELL, "C005_CellMessage", 66u },
    { 0x10000C06UL, 8u, DBC_TRACE_CATEGORY_CELL, "C006_CellMessage", 67u },
    { 0x10000C07UL, 8u, DBC_TRACE_CATEGORY_CELL, "C007_CellMessage", 66u },
    { 0x10000C08UL, 8u, DBC_TRACE_CATEGORY_CELL, "C008_CellMessage", 64u },
    { 0x10000C09UL, 8u, DBC_TRACE_CATEGORY_CELL, "C009_CellMessage", 62u },
    { 0x10000C0AUL, 8u, DBC_TRACE_CATEGORY_CELL, "C010_CellMessage", 62u },
    { 0x10000C0BUL, 8u, DBC_TRACE_CATEGORY_CELL, "C011_CellMessage", 66u },
    { 0x10000C0CUL, 8u, DBC_TRACE_CATEGORY_CELL, "C012_CellMessage", 67u },
    { 0x10000C0DUL, 8u, DBC_TRACE_CATEGORY_CELL, "C013_CellMessage", 66u },
    { 0x10000C0EUL, 8u, DBC_TRACE_CATEGORY_CELL, "C014_CellMessage", 67u },
    { 0x10000C0FUL, 8u, DBC_TRACE_CATEGORY_CELL, "C015_CellMessage", 67u },
    { 0x10000C10UL, 8u, DBC_TRACE_CATEGORY_CELL, "C016_CellMessage", 68u },
    { 0x10000C11UL, 8u, DBC_TRACE_CATEGORY_CELL, "C017_CellMessage", 68u },
    { 0x10000C12UL, 8u, DBC_TRACE_CATEGORY_CELL, "C018_CellMessage", 68u },
    { 0x10000C13UL, 8u, DBC_TRACE_CATEGORY_CELL, "C019_CellMessage", 69u },
    { 0x10000C14UL, 8u, DBC_TRACE_CATEGORY_CELL, "C020_CellMessage", 69u },
    { 0x10005801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_BMSInfo", 67u },
    { 0x12181801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_BalanceCells", 67u },
    { 0x1808D801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_ConfigurationMessage", 6u },
    { 0x10007801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_EEPROMData", 66u },
    { 0x1000A801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_IC_AnalogPower", 68u },
    { 0x1000C801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_IC_CellSumVoltages", 69u },
    { 0x10009801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_IC_DigitalPower", 67u },
    { 0x10008801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_IC_InternalTemperature", 68u },
    { 0x1000B801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_IC_RefVoltages", 66u },
    { 0x10003801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_MaxMinVoltages", 66u },
    { 0x10004801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_ModuleInformation", 67u },
    { 0x10010801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_ModuleSafetyMechanisms", 67u },
    { 0x10002801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_ModuleStatus", 68u },
    { 0x1000D801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_OverVoltageFlags", 68u },
    { 0x1000F801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_SendEEPROMDataEOL", 563u },
    { 0x10006801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_SlaveSerials", 68u },
    { 0x10088801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_StartLTCTransmission", 8u },
    { 0x10001801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_TerminalTemperatures", 68u },
    { 0x1000E801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_UnderVoltageFlags", 67u },
    { 0x10087801UL, 8u, DBC_TRACE_CATEGORY_M001, "M001_WriteEEPROMData", 33u },
    { 0x12011401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_AkaSerialNo", 65u },
    { 0x1200D401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Alarm", 676u },
    { 0x12017401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_AmpereHourCounter", 65u },
    { 0x0A00F401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_BatteryVoltagesNeg", 678u },
    { 0x0A00E401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_BatteryVoltagesPos", 678u },
    { 0x1201B401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Board_Supply_Vtg1", 65u },
    { 0x1201C401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Board_Supply_Vtg2", 65u },
    { 0x12004401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_ContactorCoilCurrents", 675u },
    { 0x0A015401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_ContactorDmg", 674u },
    { 0x0A005401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Currents", 678u },
    { 0x1200B401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_ErrorInfo", 676u },
    { 0x12182401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_HSSwitchReq", 677u },
    { 0x12003401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_HighsideSwitchCurrents", 675u },
    { 0x12002401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_HighsideSwitchStatus", 675u },
    { 0x12019401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Histogramm", 65u },
    { 0x1201D401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_HwConf_Adr_Vtg", 65u },
    { 0x1200A401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_IsoContStuckVoltages", 676u },
    { 0x12013401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_LimitConfigInfo", 65u },
    { 0x1200F401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_LoopCurrents", 676u },
    { 0x12007401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_OperatingVoltages", 676u },
    { 0x12016401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_OperationTimeCounter", 65u },
    { 0x1E040401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_RAWErrorValues", 676u },
    { 0x1E04A401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_RAWValues10", 65u },
    { 0x1E047401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_RAWValues7", 65u },
    { 0x1201E401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_RTC", 65u },
    { 0x12010401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_SWConfigInfo", 66u },
    { 0x12006401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Sensors", 66u },
    { 0x08001401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Status", 1696u },
    { 0x12012401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_SysInfo", 65u },
    { 0x12014401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_TrayConfigInfo", 65u },
    { 0x1200C401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_Warning", 676u },
    { 0x1A008401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_battery_Voltages_diag", 676u },
    { 0x12009401UL, 8u, DBC_TRACE_CATEGORY_SCU1, "SCU1_tray_Voltages", 678u }
};

static inline const char *Dbc_TraceCategoryName(Dbc_TraceCategoryType category)
{
    switch (category)
    {
        case DBC_TRACE_CATEGORY_BMM:  return "BMM";
        case DBC_TRACE_CATEGORY_BMMP: return "BMMp";
        case DBC_TRACE_CATEGORY_CELL: return "Cell";
        case DBC_TRACE_CATEGORY_M001: return "M001";
        case DBC_TRACE_CATEGORY_SCU1: return "SCU1";
        default:                     return "Unknown";
    }
}

static inline bool Dbc_TraceMessage_IsTesterInputId(uint32_t can_id)
{
    if ((can_id == DBC_BMM_CONTROL_DIGITAL_OUTPUTS_ID) ||
        (can_id == DBC_M001_WRITE_EEPROM_DATA_ID) ||
        (can_id == DBC_M001_START_LTC_TRANSMISSION_ID) ||
        (can_id == DBC_M001_BALANCE_CELLS_ID) ||
        (can_id == DBC_M001_CONFIGURATION_MESSAGE_ID) ||
        (can_id == DBC_SCU1_HS_SWITCH_REQ_ID) ||
        (can_id == DBC_BMU_DIAG_COMMAND_ID) ||
        (can_id == DBC_SCU1_LIMIT_CONFIG_INFO_ID))
    {
        return true;
    }

    return false;
}

static inline bool Dbc_TraceMessage_ShouldDemoTransmit(const Dbc_TraceMessageDefType *def)
{
    if (def == 0)
    {
        return false;
    }

    if ((def->category == DBC_TRACE_CATEGORY_CELL) ||
        Dbc_TraceMessage_IsTesterInputId(def->id))
    {
        return false;
    }

    return true;
}

static inline void Dbc_TraceMessage_BuildDemoPayload(const Dbc_TraceMessageDefType *def,
                                                     uint32_t sequence,
                                                     CanIf_MsgType *msg)
{
    uint32_t value;

    if ((def == 0) || (msg == 0))
    {
        return;
    }

    value = (def->id ^ sequence);
    msg->id = def->id;
    msg->dlc = def->dlc;
    msg->is_extended = (DBC_TRACE_EXTENDED_FRAME != 0u);
    msg->data[0] = (uint8_t)(value & 0xFFu);
    msg->data[1] = (uint8_t)((value >> 8u) & 0xFFu);
    msg->data[2] = (uint8_t)((value >> 16u) & 0xFFu);
    msg->data[3] = (uint8_t)((value >> 24u) & 0xFFu);
    msg->data[4] = (uint8_t)(sequence & 0xFFu);
    msg->data[5] = (uint8_t)((sequence >> 8u) & 0xFFu);
    msg->data[6] = (uint8_t)def->category;
    msg->data[7] = 0u;
}

static inline bool Dbc_TraceMessage_LookupById(uint32_t can_id,
                                               const Dbc_TraceMessageDefType **def_out)
{
    uint16_t i;

    if (def_out == 0)
    {
        return false;
    }

    for (i = 0u; i < DBC_TRACE_MESSAGE_COUNT; i++)
    {
        if (g_dbcTraceMessages[i].id == can_id)
        {
            *def_out = &g_dbcTraceMessages[i];
            return true;
        }
    }

    *def_out = 0;
    return false;
}

static inline bool Dbc_TraceMessage_IsObservedId(uint32_t can_id)
{
    const Dbc_TraceMessageDefType *def;

    return Dbc_TraceMessage_LookupById(can_id, &def);
}

static inline bool Dbc_IsExpectedExtended8(const CanIf_MsgType *msg, uint32_t id)
{
    return (msg != 0) &&
           (msg->id == id) &&
           (msg->dlc == 8u) &&
           (msg->is_extended == true);
}

static inline uint32_t Dbc_ReadLe24(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u);
}

static inline uint16_t Dbc_ReadLe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0]) |
                      ((uint16_t)data[1] << 8u));
}

static inline uint32_t Dbc_ReadLe32(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static inline bool Dbc_ReadBitLe(const uint8_t *data, uint8_t bit_index)
{
    return ((data[bit_index >> 3u] & (uint8_t)(1u << (bit_index & 7u))) != 0u);
}

static inline bool Dbc_BmmControlDigitalOutputs_Unpack(
    const CanIf_MsgType *msg,
    Dbc_BmmControlDigitalOutputsType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_BMM_CONTROL_DIGITAL_OUTPUTS_ID)))
    {
        return false;
    }

    sig->contactor_pos = Dbc_ReadBitLe(msg->data, 0u);
    sig->contactor_neg = Dbc_ReadBitLe(msg->data, 1u);
    sig->contactor_pre = Dbc_ReadBitLe(msg->data, 2u);
    sig->contactor_aux = Dbc_ReadBitLe(msg->data, 3u);
    sig->vsense_relay_bat_pos = Dbc_ReadBitLe(msg->data, 8u);
    sig->vsense_relay_bat_neg = Dbc_ReadBitLe(msg->data, 9u);
    sig->vsense_relay_sys_pos = Dbc_ReadBitLe(msg->data, 10u);
    sig->vsense_relay_sys_neg = Dbc_ReadBitLe(msg->data, 11u);
    sig->fan_state = Dbc_ReadBitLe(msg->data, 17u);

    return true;
}

static inline bool Dbc_M001WriteEepromData_Unpack(
    const CanIf_MsgType *msg,
    Dbc_M001WriteEepromDataType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_M001_WRITE_EEPROM_DATA_ID)))
    {
        return false;
    }

    sig->mux = Dbc_ReadLe24(&msg->data[0]);
    sig->eeprom_write_index = msg->data[3];
    sig->eeprom_write_data = Dbc_ReadLe32(&msg->data[4]);

    return true;
}

static inline void Dbc_M001EepromData_Pack(CanIf_MsgType *msg,
                                           uint16_t address,
                                           uint8_t index,
                                           const uint8_t *data,
                                           uint8_t length)
{
    uint8_t i;

    if ((msg == 0) || (data == 0))
    {
        return;
    }

    msg->id = DBC_M001_EEPROM_DATA_ID;
    msg->dlc = 8u;
    msg->is_extended = true;
    msg->data[0] = (uint8_t)(address & 0xFFu);
    msg->data[1] = (uint8_t)((address >> 8u) & 0xFFu);
    msg->data[2] = index;
    msg->data[3] = length;
    for (i = 0u; i < 4u; i++)
    {
        msg->data[4u + i] = (i < length) ? data[i] : 0u;
    }
}

static inline bool Dbc_M001StartLtcTransmission_Unpack(
    const CanIf_MsgType *msg,
    Dbc_M001StartLtcTransmissionType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_M001_START_LTC_TRANSMISSION_ID)))
    {
        return false;
    }

    sig->magic = Dbc_ReadLe24(&msg->data[0]);
    sig->action_index = msg->data[3];

    return true;
}

static inline bool Dbc_M001BalanceCells_Unpack(
    const CanIf_MsgType *msg,
    Dbc_M001BalanceCellsType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_M001_BALANCE_CELLS_ID)))
    {
        return false;
    }

    sig->balance_mask_20 = Dbc_ReadLe32(&msg->data[0]) & 0x000FFFFFUL;
    return true;
}

static inline bool Dbc_BmuDiagReq_Unpack(const CanIf_MsgType *msg,
                                         Dbc_BmuDiagReqType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_BMU_DIAG_COMMAND_ID)))
    {
        return false;
    }

    sig->diag_cmd = Dbc_ReadLe16(&msg->data[0]);
    sig->param1 = Dbc_ReadLe16(&msg->data[2]);
    sig->param2 = (int32_t)Dbc_ReadLe32(&msg->data[4]);

    return true;
}

static inline bool Dbc_ThresholdSettings_Unpack(const CanIf_MsgType *msg,
                                                Dbc_ThresholdSettingsType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_SCU1_LIMIT_CONFIG_INFO_ID)))
    {
        return false;
    }

    sig->under_voltage_raw_0p001V = Dbc_ReadLe16(&msg->data[0]);
    sig->over_voltage_raw_0p001V = Dbc_ReadLe16(&msg->data[2]);
    sig->min_temp_raw_0p1C = (int16_t)Dbc_ReadLe16(&msg->data[4]);
    sig->max_temp_raw_0p1C = (int16_t)Dbc_ReadLe16(&msg->data[6]);

    return true;
}

static inline bool Dbc_Scu1HsSwitchReq_Unpack(const CanIf_MsgType *msg,
                                              Dbc_Scu1HsSwitchReqType *sig)
{
    if ((sig == 0) || (!Dbc_IsExpectedExtended8(msg, DBC_SCU1_HS_SWITCH_REQ_ID)))
    {
        return false;
    }

    sig->hs_output1_pct = msg->data[0];
    sig->hs_output2_pct = msg->data[1];
    sig->hs_output3_pct = msg->data[2];
    sig->hs_output4_pct = msg->data[3];

    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* SRC_APP_DBC_DBC_TRACE_INTERFACE_H_ */
