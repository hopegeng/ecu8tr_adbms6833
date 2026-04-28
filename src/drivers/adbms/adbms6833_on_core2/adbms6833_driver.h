#ifndef ADBMS6833_DRIVER_H
#define ADBMS6833_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS6833_MAX_ICS            (2U)			//The maximum ADBMS6833 can be daisy chained on the isoSPI. The stack may overflow on Core2( 2K user stack )
#define ADBMS6833_CELLS_PER_IC       (16U)
#define ADBMS6833_BYTES_PER_CFGR     (6U)
#define ADBMS6833_CMD_FRAME_SIZE     (4U)
#define ADBMS6833_DATA_PEC_SIZE      (2U)
#define ADBMS6833_RDCVALL_DATA_BYTES (32U)
#define ADBMS6833_CELL_GROUP_DATA_BYTES (6U)
#define ADBMS6833_AUX_CHANNELS_PER_IC (12U)
#define ADBMS6833_AUX_STATUS_CHANNELS_PER_IC (5U)
#define ADBMS6833_AUX_TOTAL_CHANNELS_PER_IC (17U)

#ifndef ADBMS6833_USE_RDCVALL
#define ADBMS6833_USE_RDCVALL        (0U)
#endif

typedef enum
{
    ADBMS6833_OK = 0,
    ADBMS6833_ERR_PARAM,
    ADBMS6833_ERR_COMM,
    ADBMS6833_ERR_PEC,
    ADBMS6833_ERR_TIMEOUT,
	ADBMS6833_ERR_STATE
} Adbms6833_Status_t;

typedef enum
{
    ADBMS6833_LINK_OFF = 0,
	ADBMS6833_LINK_IDLE,
    ADBMS6833_LINK_WAKE_REQ,
    ADBMS6833_LINK_WAKE_WAIT,
    ADBMS6833_LINK_READY,
    ADBMS6833_LINK_XFER,
    ADBMS6833_LINK_ERROR
} Adbms6833_LinkState_t;

typedef enum
{
    ADBMS6833_SVC_BOOT = 0,
    ADBMS6833_SVC_WAKE_STACK,
    ADBMS6833_SVC_CONFIGURE,
    ADBMS6833_SVC_STANDBY,
    ADBMS6833_SVC_START_MEASURE,
    ADBMS6833_SVC_WAIT_MEASURE,
    ADBMS6833_SVC_READ_RESULTS,
    ADBMS6833_SVC_PROCESS_RESULTS,
    ADBMS6833_SVC_SLEEP,
    ADBMS6833_SVC_FAULT
} Adbms6833_ServiceState_t;

typedef struct
{
    uint8_t data[ADBMS6833_BYTES_PER_CFGR];
} Adbms6833_CfgReg_t;

typedef struct
{
    uint16_t raw[ADBMS6833_CELLS_PER_IC];
    uint16_t mV[ADBMS6833_CELLS_PER_IC];
} Adbms6833_CellData_t;

typedef struct
{
    uint16_t raw[ADBMS6833_AUX_CHANNELS_PER_IC];
    uint32_t mV[ADBMS6833_AUX_CHANNELS_PER_IC];
} Adbms6833_AuxData_t;

typedef struct
{
    uint16_t vref2Raw;
    uint32_t vref2mV;
    uint16_t itmpRaw;
    int16_t itmpDegC;
    uint16_t vdRaw;
    uint32_t vdmV;
    uint16_t vaRaw;
    uint32_t vamV;
    uint16_t vresRaw;
    uint32_t vresmV;
} Adbms6833_AuxStatusData_t;

typedef struct
{
    uint8_t icCount;

    Adbms6833_LinkState_t    linkState;
    Adbms6833_ServiceState_t svcState;

    uint32_t tickMs;
    uint32_t stateEntryMs;
    uint32_t lastCommMs;
    uint32_t lastMeasureMs;

    uint32_t pecErrorCount;
    uint32_t commErrorCount;
    bool configured;

    uint32_t measureCount;

    Adbms6833_CfgReg_t cfga[ADBMS6833_MAX_ICS];
    Adbms6833_CfgReg_t cfgb[ADBMS6833_MAX_ICS];
    Adbms6833_CellData_t cell[ADBMS6833_MAX_ICS];
    Adbms6833_AuxData_t aux[ADBMS6833_MAX_ICS];
    Adbms6833_AuxStatusData_t auxStatus[ADBMS6833_MAX_ICS];

} Adbms6833_Context_t;

typedef struct
{
    Adbms6833_Status_t (*spiTransfer)(const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*delayUs)(uint32_t us);
    void (*delayMs)(uint32_t ms);
} Adbms6833_Hal_t;

typedef struct
{
    uint16_t WRCFGA;
    uint16_t WRCFGB;
    uint16_t RDCFGA;
    uint16_t RDCFGB;
    uint16_t RDSTATA;
    uint16_t RDSTATB;
    uint16_t RDSTATC;
    uint16_t RDSID;
    uint16_t SRST;
    uint16_t CLRFLAG;
    uint16_t RDCVA;
    uint16_t RDCVB;
    uint16_t RDCVC;
    uint16_t RDCVD;
    uint16_t RDCVE;
    uint16_t RDCVF;
    uint16_t RDCVALL;
    uint16_t ADCV;
    uint16_t ADAX;
    uint16_t RDAUXA;
    uint16_t RDAUXB;
    uint16_t RDAUXC;
    uint16_t RDAUXD;
    uint16_t MUTE;
    uint16_t UNMUTE;
} Adbms6833_CommandSet_t;

void Adbms6833_Init(Adbms6833_Context_t *ctx, uint8_t icCount);
void Adbms6833_SetDefaultCommands(Adbms6833_CommandSet_t *cmds);

uint16_t Adbms6833_RawTo_mV(uint16_t raw);
uint32_t Adbms6833_RawTo_uV(uint16_t raw);

Adbms6833_Status_t Adbms6833_WakeUp(const Adbms6833_Context_t *ctx, const Adbms6833_Hal_t *hal);
Adbms6833_Status_t Adbms6833_StartCellConversion(const Adbms6833_Hal_t *hal,
                                                 const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_StartAuxConversion(const Adbms6833_Hal_t *hal,
                                                const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_WriteCfga(Adbms6833_Context_t *ctx,
                                       const Adbms6833_Hal_t *hal,
                                       const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_WriteCfgb(Adbms6833_Context_t *ctx,
                                       const Adbms6833_Hal_t *hal,
                                       const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_FullInitialize(Adbms6833_Context_t *ctx,
                                            const Adbms6833_Hal_t *hal,
                                            const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_ReadCellVoltagesAll(Adbms6833_Context_t *ctx,
                                                 const Adbms6833_Hal_t *hal,
                                                 const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_ReadCellVoltagesByGroup(Adbms6833_Context_t *ctx,
                                                     const Adbms6833_Hal_t *hal,
                                                     const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_ReadAuxVoltagesByGroup(Adbms6833_Context_t *ctx,
                                                    const Adbms6833_Hal_t *hal,
                                                    const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_SendMute(const Adbms6833_Hal_t *hal,
                                      const Adbms6833_CommandSet_t *cmds);
Adbms6833_Status_t Adbms6833_SendUnmute(const Adbms6833_Hal_t *hal,
                                        const Adbms6833_CommandSet_t *cmds);

Adbms6833_Status_t Adbms6833_Task(Adbms6833_Context_t *ctx,
                                  const Adbms6833_Hal_t *hal,
                                  const Adbms6833_CommandSet_t *cmds,
                                  uint32_t measurementPeriodMs);

/*
 * IMPORTANT:
 * Replace this placeholder packing with the exact CFGB bit layout from your
 * official ADBMS6833 / ADBMS6833 documentation when using DCC balancing bits.
 */
void Adbms6833_CfgbPackDccPlaceholder(uint16_t dccMask, uint8_t cfgb[ADBMS6833_BYTES_PER_CFGR]);

#ifdef __cplusplus
}
#endif

#endif /* ADBMS6833_DRIVER_H */
