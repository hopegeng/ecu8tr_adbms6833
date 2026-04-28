#ifndef ADBMS6830_DRIVER_H
#define ADBMS6830_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADBMS6830_MAX_ICS            (2U)			//The maximum ADBMS6830 can be daisy chained on the isoSPI. The stack may overflow on Core2( 2K user stack )
#define ADBMS6830_CELLS_PER_IC       (16U)
#define ADBMS6830_BYTES_PER_CFGR     (6U)
#define ADBMS6830_CMD_FRAME_SIZE     (4U)
#define ADBMS6830_DATA_PEC_SIZE      (2U)
#define ADBMS6830_RDCVALL_DATA_BYTES (32U)
#define ADBMS6830_CELL_GROUP_DATA_BYTES (6U)
#define ADBMS6830_AUX_CHANNELS_PER_IC (12U)

#ifndef ADBMS6830_USE_RDCVALL
#define ADBMS6830_USE_RDCVALL        (0U)
#endif

typedef enum
{
    ADBMS6830_OK = 0,
    ADBMS6830_ERR_PARAM,
    ADBMS6830_ERR_COMM,
    ADBMS6830_ERR_PEC,
    ADBMS6830_ERR_TIMEOUT,
	ADBMS6830_ERR_STATE
} Adbms6830_Status_t;

typedef enum
{
    ADBMS6830_LINK_OFF = 0,
	ADBMS6830_LINK_IDLE,
    ADBMS6830_LINK_WAKE_REQ,
    ADBMS6830_LINK_WAKE_WAIT,
    ADBMS6830_LINK_READY,
    ADBMS6830_LINK_XFER,
    ADBMS6830_LINK_ERROR
} Adbms6830_LinkState_t;

typedef enum
{
    ADBMS6830_SVC_BOOT = 0,
    ADBMS6830_SVC_WAKE_STACK,
    ADBMS6830_SVC_CONFIGURE,
    ADBMS6830_SVC_STANDBY,
    ADBMS6830_SVC_START_MEASURE,
    ADBMS6830_SVC_WAIT_MEASURE,
    ADBMS6830_SVC_READ_RESULTS,
    ADBMS6830_SVC_PROCESS_RESULTS,
    ADBMS6830_SVC_SLEEP,
    ADBMS6830_SVC_FAULT
} Adbms6830_ServiceState_t;

typedef struct
{
    uint8_t data[ADBMS6830_BYTES_PER_CFGR];
} Adbms6830_CfgReg_t;

typedef struct
{
    uint16_t raw[ADBMS6830_CELLS_PER_IC];
    uint16_t mV[ADBMS6830_CELLS_PER_IC];
} Adbms6830_CellData_t;

typedef struct
{
    uint16_t raw[ADBMS6830_AUX_CHANNELS_PER_IC];
    uint16_t mV[ADBMS6830_AUX_CHANNELS_PER_IC];
} Adbms6830_AuxData_t;

typedef struct
{
    uint8_t icCount;

    Adbms6830_LinkState_t    linkState;
    Adbms6830_ServiceState_t svcState;

    uint32_t tickMs;
    uint32_t stateEntryMs;
    uint32_t lastCommMs;
    uint32_t lastMeasureMs;

    uint32_t pecErrorCount;
    uint32_t commErrorCount;
    bool configured;

    uint32_t measureCount;

    Adbms6830_CfgReg_t cfga[ADBMS6830_MAX_ICS];
    Adbms6830_CfgReg_t cfgb[ADBMS6830_MAX_ICS];
    Adbms6830_CellData_t cell[ADBMS6830_MAX_ICS];
    Adbms6830_AuxData_t aux[ADBMS6830_MAX_ICS];

} Adbms6830_Context_t;

typedef struct
{
    Adbms6830_Status_t (*spiTransfer)(const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*delayUs)(uint32_t us);
    void (*delayMs)(uint32_t ms);
} Adbms6830_Hal_t;

typedef struct
{
    uint16_t WRCFGA;
    uint16_t WRCFGB;
    uint16_t RDCFGA;
    uint16_t RDCFGB;
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
} Adbms6830_CommandSet_t;

void Adbms6830_Init(Adbms6830_Context_t *ctx, uint8_t icCount);
void Adbms6830_SetDefaultCommands(Adbms6830_CommandSet_t *cmds);

uint16_t Adbms6830_RawTo_mV(uint16_t raw);
uint32_t Adbms6830_RawTo_uV(uint16_t raw);

Adbms6830_Status_t Adbms6830_WakeUp(const Adbms6830_Context_t *ctx, const Adbms6830_Hal_t *hal);
Adbms6830_Status_t Adbms6830_StartCellConversion(const Adbms6830_Hal_t *hal,
                                                 const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_StartAuxConversion(const Adbms6830_Hal_t *hal,
                                                const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_WriteCfga(Adbms6830_Context_t *ctx,
                                       const Adbms6830_Hal_t *hal,
                                       const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_WriteCfgb(Adbms6830_Context_t *ctx,
                                       const Adbms6830_Hal_t *hal,
                                       const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_FullInitialize(Adbms6830_Context_t *ctx,
                                            const Adbms6830_Hal_t *hal,
                                            const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_ReadCellVoltagesAll(Adbms6830_Context_t *ctx,
                                                 const Adbms6830_Hal_t *hal,
                                                 const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_ReadCellVoltagesByGroup(Adbms6830_Context_t *ctx,
                                                     const Adbms6830_Hal_t *hal,
                                                     const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_ReadAuxVoltagesByGroup(Adbms6830_Context_t *ctx,
                                                    const Adbms6830_Hal_t *hal,
                                                    const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_SendMute(const Adbms6830_Hal_t *hal,
                                      const Adbms6830_CommandSet_t *cmds);
Adbms6830_Status_t Adbms6830_SendUnmute(const Adbms6830_Hal_t *hal,
                                        const Adbms6830_CommandSet_t *cmds);

Adbms6830_Status_t Adbms6830_Task(Adbms6830_Context_t *ctx,
                                  const Adbms6830_Hal_t *hal,
                                  const Adbms6830_CommandSet_t *cmds,
                                  uint32_t measurementPeriodMs);

/*
 * IMPORTANT:
 * Replace this placeholder packing with the exact CFGB bit layout from your
 * official ADBMS6830 / ADBMS6833 documentation when using DCC balancing bits.
 */
void Adbms6830_CfgbPackDccPlaceholder(uint16_t dccMask, uint8_t cfgb[ADBMS6830_BYTES_PER_CFGR]);

#ifdef __cplusplus
}
#endif

#endif /* ADBMS6830_DRIVER_H */
