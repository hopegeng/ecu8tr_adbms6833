/*
 * adbms6830_driver.c
 *
 * Lightweight ADBMS6830 driver for TC399 + QSPI0.
 */

#include <limits.h>
#include <string.h>

#include "adbms6830_driver.h"
#include "adbms6830_reg.h"
#include "adbmsCommon.h"
#include "qspi0mstr_illd.h"
#include "shell.h"

#define ADBMS6830_RD_WR_BUFFER_SIZE         (20u * ADBMS6830_NUMBER_OF_NODES)
#define ADBMS6830_PEC_BYTES_PER_NODE        2u

static uint8_t g_adbmsWriteBuffer[ADBMS6830_RD_WR_BUFFER_SIZE];
static uint8_t g_adbmsReadBuffer[ADBMS6830_RD_WR_BUFFER_SIZE];
static QspiCs_t g_adbms6830SpiCs = eQspiHwCs02;
static bool g_adbms6830Initialized = false;
static uint8_t g_cfgaShadow[6] = {0};
static uint8_t g_cfgbShadow[6] = {0};

static bool adbms6830_readRaw(uint16_t addr, uint8_t *pRxBuff, uint16_t rdLen);
static void adbms6830_wakeup(bool force);
static uint16_t adbms6830_ReadU16LE(const uint8_t *src);
static uint8_t adbms6830_ReadCellGroup(uint16_t cmd, ADBMS6830_CellGroup_t *cellGroup, uint8_t okMask);
static uint8_t adbms6830_ReadAvCellGroup(uint16_t cmd, ADBMS6830_AvCellGroup_t *cellGroup, uint8_t okMask);
static uint8_t adbms6830_ReadFiltCellGroup(uint16_t cmd, ADBMS6830_FiltCellGroup_t *cellGroup, uint8_t okMask);
static uint8_t adbms6830_ReadSPinGroup(uint16_t cmd, uint16_t *dst0, uint16_t *dst1, uint16_t *dst2, uint8_t okMask);
static bool adbms6830_ReadSimpleGroup(uint16_t cmd, uint8_t *dst, uint16_t len);



void adbms6830_SetSpiChipSelect(QspiCs_t cs)
{
    if (cs != eQspiHwCsNone)
    {
        g_adbms6830SpiCs = cs;
    }
}

QspiCs_t adbms6830_GetSpiChipSelect(void)
{
    return g_adbms6830SpiCs;
}

static void adbms6830_wakeup(bool force)
{
    uint8_t tx = 0u;
    uint8_t rx = 0u;

    (void)force;
    qspi0_send_receive_iLLD(g_adbms6830SpiCs, 1u, &tx, &rx);
    (void)qspimstr_waitForRxDone_iLLD();
    adbmsCommon_isoSPI_wakeup_timeoutUs_init();
}

bool adbms6830_write(uint16_t addr, uint8_t *pData, uint16_t len)
{
    uint16_t pec15;
    uint16_t pec10;
    uint16_t idx = 0u;
    uint16_t loop;

    memset(g_adbmsWriteBuffer, 0, sizeof(g_adbmsWriteBuffer));
    memset(g_adbmsReadBuffer, 0, sizeof(g_adbmsReadBuffer));

    g_adbmsWriteBuffer[idx++] = (uint8_t)(addr >> 8);
    g_adbmsWriteBuffer[idx++] = (uint8_t)addr;
    pec15 = adbmsCommon_Calc_pec15(g_adbmsWriteBuffer, 2u);
    g_adbmsWriteBuffer[idx++] = (uint8_t)(pec15 >> 8);
    g_adbmsWriteBuffer[idx++] = (uint8_t)pec15;

    if ((pData != NULL) && (len > 0u))
    {
        for (loop = 0u; loop < len; ++loop)
        {
            g_adbmsWriteBuffer[idx++] = pData[loop];
        }

        pec10 = adbmsCommon_Calc_pec10(pData, 0u, len);
        g_adbmsWriteBuffer[idx++] = (uint8_t)(pec10 >> 8);
        g_adbmsWriteBuffer[idx++] = (uint8_t)pec10;
    }

    adbms6830_wakeup(true);
    (void)qspi0_send_receive_iLLD(g_adbms6830SpiCs, idx, g_adbmsWriteBuffer, g_adbmsReadBuffer);
    return qspimstr_waitForRxDone_iLLD();
}

static bool adbms6830_readRaw(uint16_t addr, uint8_t *pRxBuff, uint16_t rdLen)
{
    uint16_t pec15;
    uint16_t txLen = 0u;
    uint8_t bytesToRead;
    uint8_t *payload;
    uint16_t receivedPec10;
    uint16_t calculatedPec10;
    uint8_t commandCounter;

    if ((pRxBuff == NULL) || (rdLen == 0u))
    {
        return false;
    }

    memset(g_adbmsWriteBuffer, 0, sizeof(g_adbmsWriteBuffer));
    memset(g_adbmsReadBuffer, 0, sizeof(g_adbmsReadBuffer));
    memset(pRxBuff, 0, rdLen * ADBMS6830_NUMBER_OF_NODES);

    g_adbmsWriteBuffer[txLen++] = (uint8_t)(addr >> 8);
    g_adbmsWriteBuffer[txLen++] = (uint8_t)addr;
    pec15 = adbmsCommon_Calc_pec15(g_adbmsWriteBuffer, 2u);
    g_adbmsWriteBuffer[txLen++] = (uint8_t)(pec15 >> 8);
    g_adbmsWriteBuffer[txLen++] = (uint8_t)pec15;

    bytesToRead = (uint8_t)((rdLen + ADBMS6830_PEC_BYTES_PER_NODE) * ADBMS6830_NUMBER_OF_NODES);

    adbms6830_wakeup(true);
    adbmsCommon_delayMillseconds(1u);

    (void)qspi0_send_receive_iLLD(g_adbms6830SpiCs, (uint16_t)(txLen + bytesToRead), g_adbmsWriteBuffer, g_adbmsReadBuffer);
    if (qspimstr_waitForRxDone_iLLD() == false)
    {
        return false;
    }

    payload = &g_adbmsReadBuffer[txLen];
    receivedPec10 = (((uint16_t)payload[rdLen] & 0x03u) << 8) | (uint16_t)payload[rdLen + 1u];
    commandCounter = payload[rdLen] >> 2;
    calculatedPec10 = adbmsCommon_Calc_pec10(payload, commandCounter, rdLen);

    if (calculatedPec10 != receivedPec10)
    {
        dbgPRINT("ADBMS6830 PEC10 mismatch");
        return false;
    }

    memcpy(pRxBuff, payload, rdLen);
    adbms6830Info.commandCounter = commandCounter;
    return true;
}

static uint16_t adbms6830_ReadU16LE(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[1] << 8) | (uint16_t)src[0]);
}

static uint8_t adbms6830_ReadCellGroup(uint16_t cmd, ADBMS6830_CellGroup_t *cellGroup, uint8_t okMask)
{
    uint8_t data[6];

    if ((cellGroup == NULL) || (adbms6830_readRaw(cmd, data, sizeof(data)) == false))
    {
        return 0u;
    }

    switch (cmd)
    {
    case ADBMS6830_RDCVA_REG:
    case ADBMS6830_RDACA_REG:
    case ADBMS6830_RDFCA_REG:
        cellGroup->C1V = adbms6830_ReadU16LE(&data[0]);
        cellGroup->C2V = adbms6830_ReadU16LE(&data[2]);
        cellGroup->C3V = adbms6830_ReadU16LE(&data[4]);
        break;
    case ADBMS6830_RDCVB_REG:
    case ADBMS6830_RDACB_REG:
    case ADBMS6830_RDFCB_REG:
        cellGroup->C4V = adbms6830_ReadU16LE(&data[0]);
        cellGroup->C5V = adbms6830_ReadU16LE(&data[2]);
        cellGroup->C6V = adbms6830_ReadU16LE(&data[4]);
        break;
    case ADBMS6830_RDCVC_REG:
    case ADBMS6830_RDACC_REG:
    case ADBMS6830_RDFCC_REG:
        cellGroup->C7V = adbms6830_ReadU16LE(&data[0]);
        cellGroup->C8V = adbms6830_ReadU16LE(&data[2]);
        cellGroup->C9V = adbms6830_ReadU16LE(&data[4]);
        break;
    case ADBMS6830_RDCVD_REG:
    case ADBMS6830_RDACD_REG:
    case ADBMS6830_RDFCD_REG:
        cellGroup->C10V = adbms6830_ReadU16LE(&data[0]);
        cellGroup->C11V = adbms6830_ReadU16LE(&data[2]);
        cellGroup->C12V = adbms6830_ReadU16LE(&data[4]);
        break;
    case ADBMS6830_RDCVE_REG:
    case ADBMS6830_RDACE_REG:
    case ADBMS6830_RDFCE_REG:
        cellGroup->C13V = adbms6830_ReadU16LE(&data[0]);
        cellGroup->C14V = adbms6830_ReadU16LE(&data[2]);
        cellGroup->C15V = adbms6830_ReadU16LE(&data[4]);
        break;
    case ADBMS6830_RDCVF_REG:
    case ADBMS6830_RDACF_REG:
    case ADBMS6830_RDFCF_REG:
        cellGroup->C16V = adbms6830_ReadU16LE(&data[0]);
        break;
    default:
        return 0u;
    }

    return okMask;
}

static uint8_t adbms6830_ReadAvCellGroup(uint16_t cmd, ADBMS6830_AvCellGroup_t *cellGroup, uint8_t okMask)
{
    return adbms6830_ReadCellGroup(cmd, (ADBMS6830_CellGroup_t *)cellGroup, okMask);
}

static uint8_t adbms6830_ReadFiltCellGroup(uint16_t cmd, ADBMS6830_FiltCellGroup_t *cellGroup, uint8_t okMask)
{
    return adbms6830_ReadCellGroup(cmd, (ADBMS6830_CellGroup_t *)cellGroup, okMask);
}

static uint8_t adbms6830_ReadSPinGroup(uint16_t cmd, uint16_t *dst0, uint16_t *dst1, uint16_t *dst2, uint8_t okMask)
{
    uint8_t data[6];

    if ((dst0 == NULL) || (dst1 == NULL) || (dst2 == NULL) || (adbms6830_readRaw(cmd, data, sizeof(data)) == false))
    {
        return 0u;
    }

    *dst0 = adbms6830_ReadU16LE(&data[0]);
    *dst1 = adbms6830_ReadU16LE(&data[2]);
    *dst2 = adbms6830_ReadU16LE(&data[4]);
    return okMask;
}

static bool adbms6830_ReadSimpleGroup(uint16_t cmd, uint8_t *dst, uint16_t len)
{
    return adbms6830_readRaw(cmd, dst, len);
}

bool adbms6830_initADBMS6830( void )
{
    static const uint8_t startContinuousCellCmd[1] = { 0u };
    static const uint8_t startContinuousSPinCmd[1] = { 0u };

    if (adbms6830_write(ADBMS6830_SRST_REG, NULL, 0u) == false)
    {
        return false;
    }

    adbmsCommon_delayMillseconds(2u);

    if ((adbms6830_WriteCFGA() == false) || (adbms6830_WriteCFGB() == false))
    {
        return false;
    }

    if (adbms6830_write((uint16_t)(ADBMS6830_ADCV_BASE_REG | ADBMS6830_ADCV_CONT_OPTION),
                        (uint8_t *)startContinuousCellCmd,
                        0u) == false)
    {
        return false;
    }

    if (adbms6830_write((uint16_t)(ADBMS6830_ADSV_BASE_REG | ADBMS6830_ADCV_CONT_OPTION),
                        (uint8_t *)startContinuousSPinCmd,
                        0u) == false)
    {
        return false;
    }

    adbms6830Info.isInitialized = true;
    return true;
}

bool adbms6830_ReadCFGA(void)
{
    return adbms6830_ReadSimpleGroup(ADBMS6830_RDCFGA_REG, g_cfgaShadow, sizeof(g_cfgaShadow));
}

bool adbms6830_ReadCFGB(void)
{
    return adbms6830_ReadSimpleGroup(ADBMS6830_RDCFGB_REG, g_cfgbShadow, sizeof(g_cfgbShadow));
}

uint8_t adbms6830_ReadStatA(void)
{
    uint8_t data[6];

    if (adbms6830_readRaw(ADBMS6830_RDSTATA_REG, data, sizeof(data)) == false)
    {
        return 0u;
    }

    adbms6830Info.rawData[0].Status_Voltages.VREF2 = adbms6830_ReadU16LE(&data[0]);
    adbms6830Info.rawData[0].Status_Voltages.ITMP  = adbms6830_ReadU16LE(&data[2]);
    adbms6830Info.rawData[0].Status_Voltages.VD    = adbms6830_ReadU16LE(&data[4]);
    return 0x01u;
}

uint8_t adbms6830_ReadStatB(void)
{
    uint8_t data[6];

    if (adbms6830_readRaw(ADBMS6830_RDSTATB_REG, data, sizeof(data)) == false)
    {
        return 0u;
    }

    adbms6830Info.rawData[0].Status_Voltages.VA   = adbms6830_ReadU16LE(&data[0]);
    adbms6830Info.rawData[0].Status_Voltages.VRES = adbms6830_ReadU16LE(&data[2]);
    return 0x02u;
}

uint8_t adbms6830_ReadStatC(void)
{
    uint8_t data[6];

    return (adbms6830_readRaw(ADBMS6830_RDSTATC_REG, data, sizeof(data)) == true) ? 0x04u : 0u;
}

uint8_t adbms6830_ReadStatD(void)
{
    uint8_t data[6];

    return (adbms6830_readRaw(ADBMS6830_RDSTATD_REG, data, sizeof(data)) == true) ? 0x08u : 0u;
}

uint8_t adbms6830_ReadStatE(void)
{
    uint8_t data[6];

    return (adbms6830_readRaw(ADBMS6830_RDSTATE_REG, data, sizeof(data)) == true) ? 0x10u : 0u;
}

uint8_t adbms6830_ReadCVA(void) { return adbms6830_ReadCellGroup(ADBMS6830_RDCVA_REG, &adbms6830Info.rawData[0].Cell, 0x01u); }
uint8_t adbms6830_ReadCVB(void) { return adbms6830_ReadCellGroup(ADBMS6830_RDCVB_REG, &adbms6830Info.rawData[0].Cell, 0x02u); }
uint8_t adbms6830_ReadCVC(void) { return adbms6830_ReadCellGroup(ADBMS6830_RDCVC_REG, &adbms6830Info.rawData[0].Cell, 0x04u); }
uint8_t adbms6830_ReadCVD(void) { return adbms6830_ReadCellGroup(ADBMS6830_RDCVD_REG, &adbms6830Info.rawData[0].Cell, 0x08u); }
uint8_t adbms6830_ReadCVE(void) { return adbms6830_ReadCellGroup(ADBMS6830_RDCVE_REG, &adbms6830Info.rawData[0].Cell, 0x10u); }
uint8_t adbms6830_ReadCVF(void) { return adbms6830_ReadCellGroup(ADBMS6830_RDCVF_REG, &adbms6830Info.rawData[0].Cell, 0x20u); }

uint8_t adbms6830_ReadACVA(void) { return adbms6830_ReadAvCellGroup(ADBMS6830_RDACA_REG, &adbms6830Info.rawData[0].AvCell, 0x01u); }
uint8_t adbms6830_ReadACVB(void) { return adbms6830_ReadAvCellGroup(ADBMS6830_RDACB_REG, &adbms6830Info.rawData[0].AvCell, 0x02u); }
uint8_t adbms6830_ReadACVC(void) { return adbms6830_ReadAvCellGroup(ADBMS6830_RDACC_REG, &adbms6830Info.rawData[0].AvCell, 0x04u); }
uint8_t adbms6830_ReadACVD(void) { return adbms6830_ReadAvCellGroup(ADBMS6830_RDACD_REG, &adbms6830Info.rawData[0].AvCell, 0x08u); }
uint8_t adbms6830_ReadACVE(void) { return adbms6830_ReadAvCellGroup(ADBMS6830_RDACE_REG, &adbms6830Info.rawData[0].AvCell, 0x10u); }
uint8_t adbms6830_ReadACVF(void) { return adbms6830_ReadAvCellGroup(ADBMS6830_RDACF_REG, &adbms6830Info.rawData[0].AvCell, 0x20u); }

uint8_t adbms6830_ReadFCA(void) { return adbms6830_ReadFiltCellGroup(ADBMS6830_RDFCA_REG, &adbms6830Info.rawData[0].FiltCell, 0x01u); }
uint8_t adbms6830_ReadFCB(void) { return adbms6830_ReadFiltCellGroup(ADBMS6830_RDFCB_REG, &adbms6830Info.rawData[0].FiltCell, 0x02u); }
uint8_t adbms6830_ReadFCC(void) { return adbms6830_ReadFiltCellGroup(ADBMS6830_RDFCC_REG, &adbms6830Info.rawData[0].FiltCell, 0x04u); }
uint8_t adbms6830_ReadFCD(void) { return adbms6830_ReadFiltCellGroup(ADBMS6830_RDFCD_REG, &adbms6830Info.rawData[0].FiltCell, 0x08u); }
uint8_t adbms6830_ReadFCE(void) { return adbms6830_ReadFiltCellGroup(ADBMS6830_RDFCE_REG, &adbms6830Info.rawData[0].FiltCell, 0x10u); }
uint8_t adbms6830_ReadFCF(void) { return adbms6830_ReadFiltCellGroup(ADBMS6830_RDFCF_REG, &adbms6830Info.rawData[0].FiltCell, 0x20u); }

uint8_t adbms6830_ReadSVA(void) { return adbms6830_ReadSPinGroup(ADBMS6830_RDSVA_REG, &adbms6830Info.rawData[0].S_Volt.S1V,  &adbms6830Info.rawData[0].S_Volt.S2V,  &adbms6830Info.rawData[0].S_Volt.S3V,  0x01u); }
uint8_t adbms6830_ReadSVB(void) { return adbms6830_ReadSPinGroup(ADBMS6830_RDSVB_REG, &adbms6830Info.rawData[0].S_Volt.S4V,  &adbms6830Info.rawData[0].S_Volt.S5V,  &adbms6830Info.rawData[0].S_Volt.S6V,  0x02u); }
uint8_t adbms6830_ReadSVC(void) { return adbms6830_ReadSPinGroup(ADBMS6830_RDSVC_REG, &adbms6830Info.rawData[0].S_Volt.S7V,  &adbms6830Info.rawData[0].S_Volt.S8V,  &adbms6830Info.rawData[0].S_Volt.S9V,  0x04u); }
uint8_t adbms6830_ReadSVD(void) { return adbms6830_ReadSPinGroup(ADBMS6830_RDSVD_REG, &adbms6830Info.rawData[0].S_Volt.S10V, &adbms6830Info.rawData[0].S_Volt.S11V, &adbms6830Info.rawData[0].S_Volt.S12V, 0x08u); }
uint8_t adbms6830_ReadSVE(void) { return adbms6830_ReadSPinGroup(ADBMS6830_RDSVE_REG, &adbms6830Info.rawData[0].S_Volt.S13V, &adbms6830Info.rawData[0].S_Volt.S14V, &adbms6830Info.rawData[0].S_Volt.S15V, 0x10u); }
uint8_t adbms6830_ReadSVF(void)
{
    uint8_t data[6];

    if (adbms6830_readRaw(ADBMS6830_RDSVF_REG, data, sizeof(data)) == false)
    {
        return 0u;
    }

    adbms6830Info.rawData[0].S_Volt.S16V = adbms6830_ReadU16LE(&data[0]);
    return 0x20u;
}

bool adbms6830_ReadAUXA(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDAUXA_REG, data, sizeof(data)); }
bool adbms6830_ReadAUXB(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDAUXB_REG, data, sizeof(data)); }
bool adbms6830_ReadAUXC(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDAUXC_REG, data, sizeof(data)); }
bool adbms6830_ReadAUXD(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDAUXD_REG, data, sizeof(data)); }
bool adbms6830_ReadRAXA(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDRAXA_REG, data, sizeof(data)); }
bool adbms6830_ReadRAXB(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDRAXB_REG, data, sizeof(data)); }
bool adbms6830_ReadRAXC(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDRAXC_REG, data, sizeof(data)); }
bool adbms6830_ReadRAXD(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDRAXD_REG, data, sizeof(data)); }

bool adbms6830_WriteCFGA(void)
{
    return adbms6830_write(ADBMS6830_WRCFGA_REG, g_cfgaShadow, sizeof(g_cfgaShadow));
}

bool adbms6830_WriteCFGB(void)
{
    return adbms6830_write(ADBMS6830_WRCFGB_REG, g_cfgbShadow, sizeof(g_cfgbShadow));
}

bool adbms6830_ReadCOMM(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDCOMM_REG, data, sizeof(data)); }
bool adbms6830_ReadPWMA(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDPWMA_REG, data, sizeof(data)); }
bool adbms6830_ReadPWMB(void) { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDPWMB_REG, data, sizeof(data)); }
bool adbms6830_ReadRR(void)   { uint8_t data[6]; return adbms6830_ReadSimpleGroup(ADBMS6830_RDRR_REG,   data, sizeof(data)); }

bool adbms6830_WriteCOMM(void)
{
    uint8_t data[6] = { 0u };
    return adbms6830_write(ADBMS6830_WRCOMM_REG, data, sizeof(data));
}

bool adbms6830_WritePWMA(void)
{
    uint8_t data[6] = { 0u };
    return adbms6830_write(ADBMS6830_WRPWMA_REG, data, sizeof(data));
}

bool adbms6830_WritePWMB(void)
{
    uint8_t data[6] = { 0u };
    return adbms6830_write(ADBMS6830_WRPWMB_REG, data, sizeof(data));
}

bool adbms6830_WriteRR(void)
{
    uint8_t data[6] = { 0u };
    if (adbms6830_write(ADBMS6830_ULRR_REG, NULL, 0u) == false)
    {
        return false;
    }

    return adbms6830_write(ADBMS6830_WRRR_REG, data, sizeof(data));
}

bool adbms6830_clearFaultFlags(void)
{
    memset(adbms6830Info.faults, 0, sizeof(adbms6830Info.faults));
    adbms6830Faults.U32 = 0u;
    return adbms6830_write(ADBMS6830_CLRFLAG_REG, NULL, 0u);
}

bool adbms6830_clearOVUV(void)
{
    return adbms6830_write(ADBMS6830_CLOVUV_REG, NULL, 0u);
}

bool adbms6830_ConfigureOVUVthresholds(void)
{
    return adbms6830_WriteCFGB();
}

bool adbms6830_TrigAuxMeasurement(void)
{
    return adbms6830_write(ADBMS6830_ADAX_BASE_REG, NULL, 0u);
}

bool adbms6830_checkStatC_Faults(void)
{
    adbms6830Info.faults[0].U32 = 0u;
    return false;
}

bool adbms6830_checkStatD_Faults(void)
{
    adbms6830Info.faults[0].U32 = 0u;
    return false;
}

bool adbms6830_SNAP(void)
{
    return adbms6830_write(ADBMS6830_SNAP_REG, NULL, 0u);
}

bool adbms6830_UNSNAP(void)
{
    return adbms6830_write(ADBMS6830_UNSNAP_REG, NULL, 0u);
}
