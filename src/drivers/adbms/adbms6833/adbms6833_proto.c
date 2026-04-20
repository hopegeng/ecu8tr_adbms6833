/*
 * adbms6833_proto.c
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */


#include "adbms6833_proto.h"
#include "adbms6822_if.h"
#include "adbms6833_platform.h"
#include <string.h>

#define ADBMS6833_CMD_FRAME_LEN           (4u)
#define ADBMS6833_PEC_LEN                 (2u)

/* Skeleton register sizing assumptions for stub/demo only */
#define ADBMS6833_CELL_BYTES_PER_AFE      (6u)   /* example */
#define ADBMS6833_GPIO_BYTES_PER_AFE      (6u)   /* example */
#define ADBMS6833_BAL_BYTES_PER_AFE       (2u)   /* example */

static uint8_t g_totalAfes = 36u;

static void Adbms6833_BuildCommandFrame(uint16_t cmd, uint8_t frame[ADBMS6833_CMD_FRAME_LEN])
{
    uint16_t pec;

    frame[0] = (uint8_t)((cmd >> 8) & 0xFFu);
    frame[1] = (uint8_t)(cmd & 0xFFu);

    pec = AdbmsPlatform_Pec15(frame, 2u);
    frame[2] = (uint8_t)((pec >> 8) & 0xFFu);
    frame[3] = (uint8_t)(pec & 0xFFu);
}

void Adbms6833Proto_Init(void)
{
    if (g_totalAfes == 0u)
    {
        g_totalAfes = 1u;
    }
}

void Adbms6833Proto_SetConfiguredAfeCount(uint8_t afe_count)
{
    if ((afe_count > 0u) && (afe_count <= ADBMS6833_MAX_AFES_PER_STACK))
    {
        g_totalAfes = afe_count;
    }
}

Bmu_ReturnType Adbms6833_SendCommand(uint16_t cmd)
{
    uint8_t tx[ADBMS6833_CMD_FRAME_LEN];
    Bmu_ReturnType ret;

    Adbms6833_BuildCommandFrame(cmd, tx);
    ret = Adbms6822_SendRawFrame(tx, (uint8_t *)0, ADBMS6833_CMD_FRAME_LEN);

    return ret;
}

Bmu_ReturnType Adbms6833_StartCellConversion(void)
{
    return Adbms6833_SendCommand(ADBMS6833_CMD_ADCV);
}

Bmu_ReturnType Adbms6833_StartAuxConversion(void)
{
    return Adbms6833_SendCommand(ADBMS6833_CMD_ADAX);
}

Bmu_ReturnType Adbms6833_ReadRegisterGroup(uint16_t cmd,
                                           uint8_t total_afes,
                                           uint8_t bytes_per_afe,
                                           uint8_t *rx_buf,
                                           uint16_t rx_len)
{
    uint8_t tx[ADBMS6833_MAX_FRAME_BYTES];
    uint16_t tx_len;
    uint16_t expected_rx_len;
    uint16_t i;
    Bmu_ReturnType ret;

    if ((rx_buf == (uint8_t *)0) || (total_afes == 0u) || (bytes_per_afe == 0u))
    {
        return BMU_E_PARAM;
    }

    expected_rx_len = (uint16_t)((uint16_t)total_afes * ((uint16_t)bytes_per_afe + ADBMS6833_PEC_LEN));

    if (rx_len < expected_rx_len)
    {
        return BMU_E_RANGE;
    }

    if (expected_rx_len > ADBMS6833_MAX_FRAME_BYTES)
    {
        return BMU_E_RANGE;
    }

    /* For skeleton:
     * Build command frame and pad remaining TX bytes with 0
     * so stub SPI can echo/fill an RX buffer of the same length.
     */
    (void)memset(tx, 0, sizeof(tx));
    Adbms6833_BuildCommandFrame(cmd, tx);
    tx_len = expected_rx_len;

    ret = Adbms6822_SendRawFrame(tx, rx_buf, tx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* When using stub echo mode, rx_buf currently contains the TX pattern.
     * Replace with deterministic fake measurement payload so upper layers work.
     */
    for (i = 0u; i < expected_rx_len; i++)
    {
        rx_buf[i] = (uint8_t)(i & 0xFFu);
    }

    return BMU_OK;
}

/* ----------------------------
 * Stub/demo decode helpers
 * ---------------------------- */
static void Adbms6833_FillDemoCells(Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t afe;
    uint8_t ch;

    for (afe = 0u; afe < snapshot->afe_count; afe++)
    {
        snapshot->afe[afe].valid = true;

        for (ch = 0u; ch < ADBMS6833_CELL_CH_PER_AFE; ch++)
        {
            snapshot->afe[afe].cell_raw_0p1mV[ch] =
                (uint16_t)(37000u + ((uint16_t)afe * 20u) + ch);

            snapshot->afe[afe].balancing[ch] =
                ((((uint16_t)afe + ch) & 0x01u) != 0u) ? true : false;
        }

        for (ch = 0u; ch < ADBMS6833_GPIO_CH_PER_AFE; ch++)
        {
            snapshot->afe[afe].gpio_raw_0p1mV[ch] =
                (uint16_t)(25000u + ((uint16_t)afe * 10u) + ch);
        }
    }
}

Bmu_ReturnType Adbms6833_ReadSnapshot(Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t rxCell[ADBMS6833_MAX_FRAME_BYTES];
    uint8_t rxAux[ADBMS6833_MAX_FRAME_BYTES];
    uint8_t rxBal[ADBMS6833_MAX_FRAME_BYTES];
    Bmu_ReturnType ret;
    uint16_t need_len_cell;
    uint16_t need_len_aux;
    uint16_t need_len_bal;

    if (snapshot == (Adbms6833_StackSnapshotType *)0)
    {
        return BMU_E_PARAM;
    }

    if ((g_totalAfes == 0u) || (g_totalAfes > ADBMS6833_MAX_AFES_PER_STACK))
    {
        return BMU_E_RANGE;
    }

    snapshot->afe_count = g_totalAfes;
    (void)memset(snapshot->afe, 0, sizeof(snapshot->afe));

    ret = Adbms6833_StartCellConversion();
    if (ret != BMU_OK)
    {
        return ret;
    }

    ret = Adbms6833_StartAuxConversion();
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* In real implementation, use actual conversion delay from datasheet/config */
    AdbmsPlatform_DelayUs(300u);

    need_len_cell = (uint16_t)(g_totalAfes * (ADBMS6833_CELL_BYTES_PER_AFE + ADBMS6833_PEC_LEN));
    need_len_aux  = (uint16_t)(g_totalAfes * (ADBMS6833_GPIO_BYTES_PER_AFE + ADBMS6833_PEC_LEN));
    need_len_bal  = (uint16_t)(g_totalAfes * (ADBMS6833_BAL_BYTES_PER_AFE  + ADBMS6833_PEC_LEN));

    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDCVA,
                                      g_totalAfes,
                                      ADBMS6833_CELL_BYTES_PER_AFE,
                                      rxCell,
                                      need_len_cell);
    if (ret != BMU_OK)
    {
        return ret;
    }

    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDAUXA,
                                      g_totalAfes,
                                      ADBMS6833_GPIO_BYTES_PER_AFE,
                                      rxAux,
                                      need_len_aux);
    if (ret != BMU_OK)
    {
        return ret;
    }

    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDBAL,
                                      g_totalAfes,
                                      ADBMS6833_BAL_BYTES_PER_AFE,
                                      rxBal,
                                      need_len_bal);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* For now we ignore stub rx buffers and fill deterministic demo values.
     * Later replace with real decode:
     *  - parse per-AFE registers
     *  - check PEC per AFE block
     *  - convert register words into raw database units
     */
    Adbms6833_FillDemoCells(snapshot);

    (void)rxCell;
    (void)rxAux;
    (void)rxBal;

    return BMU_OK;
}
