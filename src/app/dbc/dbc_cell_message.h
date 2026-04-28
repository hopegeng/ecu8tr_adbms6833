/*
 * dbc_cell_message.h
 *
 *  Created on: Apr 18, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_BMU_DBC_CELL_MESSAGE_H_
#define SRC_APP_BMU_DBC_CELL_MESSAGE_H_


#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * DBC-verified Cxxx_CellMessage definition
 *
 * BO_ 2415922177 C001_CellMessage: 8 Vector__XXX
 * ...
 * BO_ 2415922536 C360_CellMessage: 8 Vector__XXX
 *
 * Note:
 * DBC stores extended-frame IDs with bit 31 set. For example:
 * - DBC stored value: 0x90000C01
 * - Actual 29-bit CAN ID on the bus: 0x10000C01
 *
 * Signals:
 *   Vxxx_act_U_CellVoltage_mV  :  0|16@1+  (0.1, 0)
 *   Txxx_act_T_CellTemp_C      : 16|16@1-  (0.01, 0)
 *   Vxxx_act_U_GPIOVoltage_mV  : 32|16@1+  (0.1, 0)
 *   Bxxx_flag_Balancing        : 48|1@1+   (1, 0)
 *
 * Cycle time:
 *   GenMsgCycleTime = 1000 ms
 * ========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

#define DBC_CELLMESSAGE_FIRST_INDEX_1BASED     (1u)
#define DBC_CELLMESSAGE_LAST_INDEX_1BASED      (360u)
#define DBC_CELLMESSAGE_COUNT                  (360u)

#define DBC_CAN_EXTENDED_ID_FLAG              (0x80000000UL)
#define DBC_CAN_EXTENDED_ID_MASK              (0x1FFFFFFFUL)

#define DBC_CELLMESSAGE_FIRST_ID_DBC          (0x90000C01UL)
#define DBC_CELLMESSAGE_LAST_ID_DBC           (0x90000D68UL)

#define DBC_CELLMESSAGE_FIRST_ID              (DBC_CELLMESSAGE_FIRST_ID_DBC & DBC_CAN_EXTENDED_ID_MASK)
#define DBC_CELLMESSAGE_LAST_ID               (DBC_CELLMESSAGE_LAST_ID_DBC & DBC_CAN_EXTENDED_ID_MASK)

#define DBC_CELLMESSAGE_DLC                   (8u)
#define DBC_CELLMESSAGE_CYCLE_MS              (1000u)
#define DBC_CELLMESSAGE_IS_EXTENDED           (1u)

/* Raw signal ranges from bit widths */
#define DBC_CELL_VOLTAGE_RAW_MIN              (0u)
#define DBC_CELL_VOLTAGE_RAW_MAX              (65535u)

#define DBC_CELL_TEMP_RAW_MIN                 (-32768)
#define DBC_CELL_TEMP_RAW_MAX                 (32767)

#define DBC_GPIO_VOLTAGE_RAW_MIN              (0u)
#define DBC_GPIO_VOLTAGE_RAW_MAX              (65535u)

#define DBC_BALANCING_RAW_MIN                 (0u)
#define DBC_BALANCING_RAW_MAX                 (1u)

/* Engineering scaling */
#define DBC_CELL_VOLTAGE_FACTOR_MV            (0.1f)
#define DBC_CELL_TEMP_FACTOR_C                (0.01f)
#define DBC_GPIO_VOLTAGE_FACTOR_MV            (0.1f)

/* Optional invalid placeholders for firmware use */
#define DBC_CELLMESSAGE_INVALID_CELL_VOLTAGE_RAW    (0u)
#define DBC_CELLMESSAGE_INVALID_CELL_TEMP_RAW       (0)
#define DBC_CELLMESSAGE_INVALID_GPIO_VOLTAGE_RAW    (0u)
#define DBC_CELLMESSAGE_INVALID_BALANCING           (0u)

/* Compile-time sanity */
#if ((DBC_CELLMESSAGE_FIRST_ID_DBC & DBC_CAN_EXTENDED_ID_FLAG) == 0u)
# error "Cxxx_CellMessage is expected to be an extended-frame DBC ID."
#endif

#if ((DBC_CELLMESSAGE_FIRST_ID + (DBC_CELLMESSAGE_COUNT - 1u)) != DBC_CELLMESSAGE_LAST_ID)
# error "Cxxx_CellMessage ID range is inconsistent."
#endif


typedef struct
{
    uint16_t cell_voltage_raw_0p1mV;   /* unsigned, 0.1 mV/bit */
    int16_t  cell_temp_raw_0p01C;      /* signed,   0.01 C/bit */
    uint16_t gpio_voltage_raw_0p1mV;   /* unsigned, 0.1 mV/bit */
    bool     balancing;                /* 1 bit */
} Dbc_CellMessageSignalsType;

/* ---------- Index / ID helpers ---------- */

static inline bool Dbc_CellMessage_IsValidIndex1(uint16_t cell_index_1based)
{
    return (cell_index_1based >= DBC_CELLMESSAGE_FIRST_INDEX_1BASED) &&
           (cell_index_1based <= DBC_CELLMESSAGE_LAST_INDEX_1BASED);
}

static inline bool Dbc_CellMessage_IsValidIndex0(uint16_t cell_index_0based)
{
    return (cell_index_0based < DBC_CELLMESSAGE_COUNT);
}

static inline uint32_t Dbc_CellMessage_IdFromIndex1(uint16_t cell_index_1based)
{
    return (DBC_CELLMESSAGE_FIRST_ID + (uint32_t)(cell_index_1based - 1u));
}

static inline uint32_t Dbc_CellMessage_IdFromIndex0(uint16_t cell_index_0based)
{
    return (DBC_CELLMESSAGE_FIRST_ID + (uint32_t)cell_index_0based);
}

static inline bool Dbc_CellMessage_IsValidId(uint32_t can_id)
{
    return (can_id >= DBC_CELLMESSAGE_FIRST_ID) &&
           (can_id <= DBC_CELLMESSAGE_LAST_ID);
}

static inline uint16_t Dbc_CellMessage_Index1FromId(uint32_t can_id)
{
    return (uint16_t)((can_id - DBC_CELLMESSAGE_FIRST_ID) + 1u);
}

static inline uint16_t Dbc_CellMessage_Index0FromId(uint32_t can_id)
{
    return (uint16_t)(can_id - DBC_CELLMESSAGE_FIRST_ID);
}

/* ---------- Engineering conversion helpers ---------- */

static inline float Dbc_CellVoltage_RawTo_mV(uint16_t raw)
{
    return ((float)raw * DBC_CELL_VOLTAGE_FACTOR_MV);
}

static inline float Dbc_CellTemp_RawTo_C(int16_t raw)
{
    return ((float)raw * DBC_CELL_TEMP_FACTOR_C);
}

static inline float Dbc_GpioVoltage_RawTo_mV(uint16_t raw)
{
    return ((float)raw * DBC_GPIO_VOLTAGE_FACTOR_MV);
}

uint16_t Dbc_CellVoltage_mV_ToRaw(float value_mV);
int16_t  Dbc_CellTemp_C_ToRaw(float value_C);
uint16_t Dbc_GpioVoltage_mV_ToRaw(float value_mV);

/* ---------- Pack / unpack ---------- */

bool Dbc_CellMessage_Pack(uint16_t cell_index_1based,
                          const Dbc_CellMessageSignalsType *sig,
						  CanIf_MsgType *msg);

bool Dbc_CellMessage_Unpack(const CanIf_MsgType *msg,
                            uint16_t *cell_index_1based,
                            Dbc_CellMessageSignalsType *sig);

#ifdef __cplusplus
}
#endif

#endif /* SRC_APP_BMU_DBC_CELL_MESSAGE_H_ */
