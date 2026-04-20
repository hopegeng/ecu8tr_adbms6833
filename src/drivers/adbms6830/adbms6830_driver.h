/*
 * adbms6830_driver.h
 *
 * Simplified ADBMS6830 driver interface for the TC399/QSPI0 integration.
 */

#ifndef SRC_DRIVERS_ADBMS6830_ADBMS6830_DRIVER_H_
#define SRC_DRIVERS_ADBMS6830_ADBMS6830_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#include "qspi.h"

#define ADBMS6830_NUMBER_OF_NODES            1u
#define ADBMS6830_NO_OF_CELLS                16u

#define ADBMS6830_CELL_OV_THRESHOLD          1400u
#define ADBMS6830_CELL_UV_THRESHOLD          600u
#define ADBMS6830_CELL_CONVERSION_FACTOR     1500u
#define ADBMS6830_CELL_LOW_RANGE             500u
#define ADBMS6830_CELL_HIGH_RANGE            2000u

#define ADBMS6830_CELL_VOLTAGE_CONVERSION    6666u
#define ADBMS6830_CELL_V_LSB                 150u
#define ADBMS6830_VOLT_SCALING               1000u
#define ADBMS6830_TEMP_KELVIN                2730u
#define ADBMS6830_MVPERDEGREEC               75u
#define ADBMS6830_TEMP_SCALING               100u

#define ADBMS6830_NOFAULTS                   0u
#define ADBMS6830_FAULT_OCCURED              1u
#define ADBMS6830_CxOV_FAULT                 0xAAu
#define ADBMS6830_CxUV_FAULT                 0x55u
#define ADBMS6830_STATC_BITMASK_FAULT        0x7FFu

typedef struct
{
    uint16_t Vref2;
    uint16_t Itmp;
    uint16_t Vd;
    uint16_t Va;
    uint16_t Vres;
} STATUSV_t;

typedef struct
{
    int16_t packV[ADBMS6830_NUMBER_OF_NODES];
    int16_t cellV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];
    int16_t avgCellV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];
    int16_t filtCellV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];
    int16_t sPinV[ADBMS6830_NUMBER_OF_NODES][ADBMS6830_NO_OF_CELLS];
    STATUSV_t statusV[ADBMS6830_NUMBER_OF_NODES];
} CELL_INFO_t;

typedef struct
{
    uint16_t C1V;
    uint16_t C2V;
    uint16_t C3V;
    uint16_t C4V;
    uint16_t C5V;
    uint16_t C6V;
    uint16_t C7V;
    uint16_t C8V;
    uint16_t C9V;
    uint16_t C10V;
    uint16_t C11V;
    uint16_t C12V;
    uint16_t C13V;
    uint16_t C14V;
    uint16_t C15V;
    uint16_t C16V;
} ADBMS6830_CellGroup_t;

typedef struct
{
    uint16_t AC1V;
    uint16_t AC2V;
    uint16_t AC3V;
    uint16_t AC4V;
    uint16_t AC5V;
    uint16_t AC6V;
    uint16_t AC7V;
    uint16_t AC8V;
    uint16_t AC9V;
    uint16_t AC10V;
    uint16_t AC11V;
    uint16_t AC12V;
    uint16_t AC13V;
    uint16_t AC14V;
    uint16_t AC15V;
    uint16_t AC16V;
} ADBMS6830_AvCellGroup_t;

typedef struct
{
    uint16_t FC1V;
    uint16_t FC2V;
    uint16_t FC3V;
    uint16_t FC4V;
    uint16_t FC5V;
    uint16_t FC6V;
    uint16_t FC7V;
    uint16_t FC8V;
    uint16_t FC9V;
    uint16_t FC10V;
    uint16_t FC11V;
    uint16_t FC12V;
    uint16_t FC13V;
    uint16_t FC14V;
    uint16_t FC15V;
    uint16_t FC16V;
} ADBMS6830_FiltCellGroup_t;

typedef struct
{
    uint16_t S1V;
    uint16_t S2V;
    uint16_t S3V;
    uint16_t S4V;
    uint16_t S5V;
    uint16_t S6V;
    uint16_t S7V;
    uint16_t S8V;
    uint16_t S9V;
    uint16_t S10V;
    uint16_t S11V;
    uint16_t S12V;
    uint16_t S13V;
    uint16_t S14V;
    uint16_t S15V;
    uint16_t S16V;
} ADBMS6830_SPinGroup_t;

typedef struct
{
    uint16_t VREF2;
    uint16_t ITMP;
    uint16_t VD;
    uint16_t VA;
    uint16_t VRES;
} ADBMS6830_StatusVoltages_t;

typedef struct
{
    ADBMS6830_CellGroup_t Cell;
    ADBMS6830_AvCellGroup_t AvCell;
    ADBMS6830_FiltCellGroup_t FiltCell;
    ADBMS6830_SPinGroup_t S_Volt;
    ADBMS6830_StatusVoltages_t Status_Voltages;
} ADBMS6830_NodeRawData_t;

typedef union
{
    uint32_t U32;
    struct
    {
        uint32_t CvsS_Mismatch : 1;
        uint32_t VAOV          : 1;
        uint32_t VAUV          : 1;
        uint32_t VDOV          : 1;
        uint32_t VDUV          : 1;
        uint32_t CED           : 1;
        uint32_t CMED          : 1;
        uint32_t SED           : 1;
        uint32_t SMED          : 1;
        uint32_t VDEL          : 1;
        uint32_t VDE           : 1;
        uint32_t COMP          : 1;
        uint32_t SPIFLT        : 1;
        uint32_t THSD          : 1;
        uint32_t OSSCHK        : 1;
        uint32_t CxOV          : 1;
        uint32_t CxUV          : 1;
        uint32_t reserved      : 15;
    } B;
} ADBMS6830_NodeFault_t;

typedef struct
{
    bool isInitialized;
    uint8_t commandCounter;
    ADBMS6830_NodeRawData_t rawData[ADBMS6830_NUMBER_OF_NODES];
    ADBMS6830_NodeFault_t faults[ADBMS6830_NUMBER_OF_NODES];
} ADBMS6830_FUELCELL_INFO_t;

typedef union
{
    uint32_t U32;
    struct
    {
        uint32_t SM_STATC_FAULT : 1;
        uint32_t SM_TRIGGER_ADSV: 1;
        uint32_t reserved       : 30;
    } B;
} ADBMS6830_SM_FAULT_t;


extern ADBMS6830_FUELCELL_INFO_t adbms6830Info;
extern ADBMS6830_SM_FAULT_t adbms6830Faults;

bool adbms6830_write(uint16_t addr, uint8_t *pData, uint16_t len);
void adbms6830_SetSpiChipSelect(QspiCs_t cs);
QspiCs_t adbms6830_GetSpiChipSelect(void);
bool adbms6830_Config(void);
bool adbms6830_ReadCFGA(void);
bool adbms6830_ReadCFGB(void);
uint8_t adbms6830_ReadStatA(void);
uint8_t adbms6830_ReadStatB(void);
uint8_t adbms6830_ReadStatC(void);
uint8_t adbms6830_ReadStatD(void);
uint8_t adbms6830_ReadStatE(void);
uint8_t adbms6830_ReadCVA(void);
uint8_t adbms6830_ReadCVB(void);
uint8_t adbms6830_ReadCVC(void);
uint8_t adbms6830_ReadCVD(void);
uint8_t adbms6830_ReadCVE(void);
uint8_t adbms6830_ReadCVF(void);
uint8_t adbms6830_ReadACVA(void);
uint8_t adbms6830_ReadACVB(void);
uint8_t adbms6830_ReadACVC(void);
uint8_t adbms6830_ReadACVD(void);
uint8_t adbms6830_ReadACVE(void);
uint8_t adbms6830_ReadACVF(void);
uint8_t adbms6830_ReadFCA(void);
uint8_t adbms6830_ReadFCB(void);
uint8_t adbms6830_ReadFCC(void);
uint8_t adbms6830_ReadFCD(void);
uint8_t adbms6830_ReadFCE(void);
uint8_t adbms6830_ReadFCF(void);
uint8_t adbms6830_ReadSVA(void);
uint8_t adbms6830_ReadSVB(void);
uint8_t adbms6830_ReadSVC(void);
uint8_t adbms6830_ReadSVD(void);
uint8_t adbms6830_ReadSVE(void);
uint8_t adbms6830_ReadSVF(void);
bool adbms6830_ReadAUXA(void);
bool adbms6830_ReadAUXB(void);
bool adbms6830_ReadAUXC(void);
bool adbms6830_ReadAUXD(void);
bool adbms6830_ReadRAXA(void);
bool adbms6830_ReadRAXB(void);
bool adbms6830_ReadRAXC(void);
bool adbms6830_ReadRAXD(void);
bool adbms6830_WriteCFGA(void);
bool adbms6830_WriteCFGB(void);
bool adbms6830_ReadCOMM(void);
bool adbms6830_ReadPWMA(void);
bool adbms6830_ReadPWMB(void);
bool adbms6830_ReadRR(void);
bool adbms6830_WriteCOMM(void);
bool adbms6830_WritePWMA(void);
bool adbms6830_WritePWMB(void);
bool adbms6830_WriteRR(void);
bool adbms6830_clearFaultFlags(void);
bool adbms6830_clearOVUV(void);
bool adbms6830_ConfigureOVUVthresholds(void);
bool adbms6830_TrigAuxMeasurement(void);
bool adbms6830_checkStatC_Faults(void);
bool adbms6830_checkStatD_Faults(void);
bool adbms6830_SNAP(void);
bool adbms6830_UNSNAP(void);

#endif /* SRC_DRIVERS_ADBMS6830_ADBMS6830_DRIVER_H_ */
