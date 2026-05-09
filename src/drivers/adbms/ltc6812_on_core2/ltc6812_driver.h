#ifndef LTC6812_DRIVER_H
#define LTC6812_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LTC6812_MAX_ICS                 (2u)
#define LTC6812_CELLS_PER_IC            (15u)
#define LTC6812_BYTES_PER_CFGR          (6u)
#define LTC6812_CMD_FRAME_SIZE          (4u)
#define LTC6812_DATA_PEC_SIZE           (2u)
#define LTC6812_REG_GROUP_DATA_LEN      (6u)
#define LTC6812_CELL_GROUP_COUNT        (5u)
#define LTC6812_CELL_GROUP_DATA_BYTES   (6u)
#define LTC6812_AUX_CHANNELS_PER_IC     (9u)
#define LTC6812_AUX_GROUP_COUNT         (4u)
#define LTC6812_COMM_BYTES_PER_IC       (6u)

typedef enum
{
    LTC6812_OK = 0,
    LTC6812_ERR_PARAM,
    LTC6812_ERR_COMM,
    LTC6812_ERR_PEC,
    LTC6812_ERR_TIMEOUT,
    LTC6812_ERR_STATE
} Ltc6812_Status_t;

typedef enum
{
    LTC6812_DIAG_NONE = 0,
    LTC6812_DIAG_WAKE,
    LTC6812_DIAG_WRCFGA,
    LTC6812_DIAG_RDCFGA,
    LTC6812_DIAG_CFGA_COMPARE,
    LTC6812_DIAG_WRCFGB,
    LTC6812_DIAG_RDCFGB,
    LTC6812_DIAG_CFGB_COMPARE,
    LTC6812_DIAG_ADCV,
    LTC6812_DIAG_ADAX,
    LTC6812_DIAG_RDCV,
    LTC6812_DIAG_RDCV_PEC,
    LTC6812_DIAG_RDAUX,
    LTC6812_DIAG_RDAUX_PEC,
    LTC6812_DIAG_WRCOMM,
    LTC6812_DIAG_RDCOMM,
    LTC6812_DIAG_STCOMM
} Ltc6812_DiagStep_t;

typedef enum
{
    LTC6812_LINK_OFF = 0,
    LTC6812_LINK_IDLE,
    LTC6812_LINK_READY,
    LTC6812_LINK_XFER,
    LTC6812_LINK_ERROR
} Ltc6812_LinkState_t;

typedef enum
{
    LTC6812_SVC_BOOT = 0,
    LTC6812_SVC_WAKE_STACK,
    LTC6812_SVC_CONFIGURE,
    LTC6812_SVC_STANDBY,
    LTC6812_SVC_START_MEASURE,
    LTC6812_SVC_WAIT_MEASURE,
    LTC6812_SVC_READ_RESULTS,
    LTC6812_SVC_PROCESS_RESULTS,
    LTC6812_SVC_FAULT
} Ltc6812_ServiceState_t;

typedef struct
{
    uint8_t data[LTC6812_BYTES_PER_CFGR];
} Ltc6812_CfgReg_t;

typedef struct
{
    uint16_t raw[LTC6812_CELLS_PER_IC];
    uint16_t mV[LTC6812_CELLS_PER_IC];
} Ltc6812_CellData_t;

typedef struct
{
    uint16_t raw[LTC6812_AUX_CHANNELS_PER_IC];
    uint16_t mV[LTC6812_AUX_CHANNELS_PER_IC];
    uint16_t vref2Raw;
    uint16_t vref2mV;
} Ltc6812_AuxData_t;

typedef struct
{
    uint8_t icCount;
    Ltc6812_LinkState_t linkState;
    Ltc6812_ServiceState_t svcState;
    uint32_t tickMs;
    uint32_t stateEntryMs;
    uint32_t lastCommMs;
    uint32_t lastMeasureMs;
    uint32_t pecErrorCount;
    uint32_t commErrorCount;
    uint32_t measureCount;
    Ltc6812_DiagStep_t lastDiagStep;
    uint16_t lastDiagCmd;
    uint8_t lastDiagIc;
    uint8_t lastDiagGroup;
    uint16_t lastRxPec;
    uint16_t lastCalcPec;
    uint16_t lastRawCell0;
    uint16_t lastRawCell1;
    uint16_t lastRawCell2;
    bool configured;
    Ltc6812_CfgReg_t cfga[LTC6812_MAX_ICS];
    Ltc6812_CfgReg_t cfgb[LTC6812_MAX_ICS];
    Ltc6812_CellData_t cell[LTC6812_MAX_ICS];
    Ltc6812_AuxData_t aux[LTC6812_MAX_ICS];
} Ltc6812_Context_t;

typedef struct
{
    Ltc6812_Status_t (*spiTransfer)(const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*delayUs)(uint32_t us);
    void (*delayMs)(uint32_t ms);
} Ltc6812_Hal_t;

typedef struct
{
    uint16_t WRCFGA;
    uint16_t WRCFGB;
    uint16_t RDCFGA;
    uint16_t RDCFGB;
    uint16_t WRCOMM;
    uint16_t RDCOMM;
    uint16_t STCOMM;
    uint16_t RDCVA;
    uint16_t RDCVB;
    uint16_t RDCVC;
    uint16_t RDCVD;
    uint16_t RDCVE;
    uint16_t ADCV;
    uint16_t ADAX;
    uint16_t RDAUXA;
    uint16_t RDAUXB;
    uint16_t RDAUXC;
    uint16_t RDAUXD;
    uint16_t CLRCELL;
    uint16_t MUTE;
    uint16_t UNMUTE;
} Ltc6812_CommandSet_t;

void Ltc6812_Init(Ltc6812_Context_t *ctx, uint8_t icCount);
void Ltc6812_SetDefaultCommands(Ltc6812_CommandSet_t *cmds);
uint16_t Ltc6812_RawTo_mV(uint16_t raw);
uint32_t Ltc6812_RawTo_uV(uint16_t raw);
Ltc6812_Status_t Ltc6812_WakeUp(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal);
Ltc6812_Status_t Ltc6812_StartCellConversion(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_StartAuxConversion(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_WriteCfga(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_WriteCfgb(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_ReadCellVoltagesByGroup(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_ReadAuxVoltagesByGroup(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_FullInitialize(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_SendMute(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_SendUnmute(const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds);
Ltc6812_Status_t Ltc6812_SetGpioPullDown(Ltc6812_Context_t *ctx,
                                         const Ltc6812_Hal_t *hal,
                                         const Ltc6812_CommandSet_t *cmds,
                                         uint8_t ic,
                                         uint8_t gpio,
                                         bool pullDownOn);
Ltc6812_Status_t Ltc6812_EepromRead(Ltc6812_Context_t *ctx,
                                    const Ltc6812_Hal_t *hal,
                                    const Ltc6812_CommandSet_t *cmds,
                                    uint16_t address,
                                    uint8_t *data,
                                    uint16_t len);
Ltc6812_Status_t Ltc6812_EepromWrite(Ltc6812_Context_t *ctx,
                                     const Ltc6812_Hal_t *hal,
                                     const Ltc6812_CommandSet_t *cmds,
                                     uint16_t address,
                                     const uint8_t *data,
                                     uint16_t len);
Ltc6812_Status_t Ltc6812_Task(Ltc6812_Context_t *ctx, const Ltc6812_Hal_t *hal, const Ltc6812_CommandSet_t *cmds, uint32_t measurementPeriodMs);
void Ltc6812_CfgaPackDcc(uint16_t dccMask, uint8_t cfga[LTC6812_BYTES_PER_CFGR]);
void Ltc6812_CfgbPackDcc(uint16_t dccMask, uint8_t cfgb[LTC6812_BYTES_PER_CFGR]);

#ifdef __cplusplus
}
#endif

#endif /* LTC6812_DRIVER_H */
