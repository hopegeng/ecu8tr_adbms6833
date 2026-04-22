#include "adbms6833_proto.h"
#include "adbms6822_if.h"
#include "adbms6833_platform.h"
#include <string.h>

/*
ReadSnapshot()
 ├─ StartCellConversion()
 ├─ StartAuxConversion()
 ├─ ReadCellGroups()
 │   ├─ ReadRegisterGroup(RDCVA)
 │   ├─ Verify PEC
 │   └─ Decode cells 1..N
 ├─ ReadAuxGroups()
 │   ├─ ReadRegisterGroup(RDAUXA)
 │   ├─ Verify PEC
 │   └─ Decode GPIOs
 ├─ ReadBalanceGroups()
 │   ├─ ReadRegisterGroup(RDBAL / config/status)
 │   ├─ Verify PEC
 │   └─ Decode balancing flags
 └─ Fill Adbms6833_StackSnapshotType
 */

#define ADBMS6833_CMD_FRAME_LEN           (4u)
#define ADBMS6833_PEC_LEN                 (2u)

/* Placeholder group sizes for decode template.
 * Replace with exact per-group byte counts from datasheet.
 */
#define ADBMS6833_CELL_GROUP_DATA_BYTES   (6u)   /* typically 3 channels x 2 bytes */
#define ADBMS6833_AUX_GROUP_DATA_BYTES    (6u)   /* example */
#define ADBMS6833_BAL_GROUP_DATA_BYTES    (2u)   /* example */

static uint8_t g_totalAfes = 36u;
static Adbms6833_ProtoStatsType g_protoStats;

static bool g_skipPecCheckInStub = true;

/* ============================================================
 * Helpers
 * ============================================================ */

static uint16_t Adbms6833_BuildCommandFrame(uint16_t cmd, uint8_t frame[ADBMS6833_CMD_FRAME_LENGTH])
{
    uint16_t pec;

    frame[0] = (uint8_t)((cmd >> 8) & 0xFFu);
    frame[1] = (uint8_t)(cmd & 0xFFu);

    pec = AdbmsPlatform_Calc_PEC15(frame, 2u);
    frame[2] = (uint8_t)((pec >> 8) & 0xFFu);
    frame[3] = (uint8_t)(pec & 0xFFu);

    return 4;
}

static uint16_t Adbms6833_ReadU16Le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static bool Adbms6833_VerifyBlockPec(const uint8_t *block_data, uint8_t data_len)
{
    uint16_t calc_pec;
    uint16_t rx_pec;

    if (block_data == (const uint8_t *)0)
    {
        return false;
    }

    calc_pec = AdbmsPlatform_Calc_PEC15(block_data, data_len);
    rx_pec   = (uint16_t)(((uint16_t)block_data[data_len] << 8) |
                          ((uint16_t)block_data[data_len + 1u]));

    return (calc_pec == rx_pec);
}

static void Adbms6833_ClearSnapshot(Adbms6833_StackSnapshotType *snapshot)
{
    if (snapshot != (Adbms6833_StackSnapshotType *)0)
    {
        (void)memset(snapshot, 0, sizeof(*snapshot));
        snapshot->afe_count = g_totalAfes;
    }
}

/* ============================================================
 * Public state
 * ============================================================ */

void Adbms6833Proto_Init(void)
{
    (void)memset(&g_protoStats, 0, sizeof(g_protoStats));

    if ((g_totalAfes == 0u) || (g_totalAfes > ADBMS6833_MAX_AFES_PER_STACK))
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

const Adbms6833_ProtoStatsType *Adbms6833Proto_GetStats(void)
{
    return &g_protoStats;
}

/* ============================================================
 * Command / read transport
 * ============================================================ */


Bmu_ReturnType Adbms6833_SendCommand(uint16_t cmd)
{
    uint8_t tx[ADBMS6833_CMD_FRAME_LEN];
    uint8_t rx[ADBMS6833_MAX_FRAME_BYTES];
    Bmu_ReturnType ret;

    Adbms6833_BuildCommandFrame(cmd, tx);
    ret = Adbms6822_SendRawFrame(tx, (uint8_t *)rx, ADBMS6833_CMD_FRAME_LEN);

    if (ret == BMU_OK)
    {
        g_protoStats.cmd_count++;
    }

    return ret;
}


Bmu_ReturnType Adbms6833_ReadRegisterGroup( uint16_t cmd,
                                           uint8_t total_afes,
                                           uint8_t bytes_per_afe,
                                           uint8_t *rx_buf,
                                           uint16_t rx_len )
{
    uint8_t tx[ADBMS6833_CMD_FRAME_LENGTH];
    uint8_t rx[ADBMS6833_MAX_FRAME_BYTES];
    uint16_t expected_rx_len;
    uint16_t tx_len;
    Bmu_ReturnType ret;

    if ((rx_buf == (uint8_t *)0) || (total_afes == 0u) || (bytes_per_afe == 0u))
    {
        return BMU_E_PARAM;
    }

    /* A group: Total 6 bytes payload + 2 bytes pec */
    /* Total receive bytes: the count of the AFE * (8 ) */

    expected_rx_len = (uint16_t)((uint16_t)total_afes *
                                 ((uint16_t)bytes_per_afe + ADBMS6833_PEC_LEN));

    if ((expected_rx_len > ADBMS6833_MAX_FRAME_BYTES) || (rx_len < expected_rx_len))
    {
        return BMU_E_RANGE;
    }

    (void)memset(tx, 0, sizeof(tx));
    tx_len = Adbms6833_BuildCommandFrame( cmd, tx );

    /* For current stub platform, TX and RX lengths must match.
     * Later, if your SPI driver supports separate TX cmd + RX read,
     * you can improve this.
     */
    ret = Adbms6822_SendRawFrame(tx, rx, expected_rx_len + tx_len );
    if (ret != BMU_OK)
    {
        return ret;
    }

    memcpy( rx_buf, rx+4, expected_rx_len );

    g_protoStats.read_count++;
    return BMU_OK;

}

/* ============================================================
 * Conversion commands
 * ============================================================ */

Bmu_ReturnType Adbms6833_StartCellConversion(void)
{
    return Adbms6833_SendCommand(ADBMS6833_CMD_ADCV);
}

Bmu_ReturnType Adbms6833_StartAuxConversion(void)
{
    return Adbms6833_SendCommand(ADBMS6833_CMD_ADAX);
}

/* ============================================================
 * Decode: cell groups
 * ============================================================ */

/* Template assumption:
 * Each cell register group returns 3 channel values per AFE:
 *   bytes 0..1 -> channel A
 *   bytes 2..3 -> channel B
 *   bytes 4..5 -> channel C
 *
 * Replace mapping if your part uses different grouping/order.
 */
static Bmu_ReturnType Adbms6833_DecodeCellGroup(const uint8_t *rx_buf,
                                                uint8_t afe_count,
                                                uint8_t cell_base_index,
                                                Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t afe;
    const uint8_t *p;

    if ((rx_buf == (const uint8_t *)0) || (snapshot == (Adbms6833_StackSnapshotType *)0))
    {
        return BMU_E_PARAM;
    }

    for (afe = 0u; afe < afe_count; afe++)
    {
        p = &rx_buf[(uint16_t)afe * (ADBMS6833_CELL_GROUP_DATA_BYTES + ADBMS6833_PEC_LEN)];

        if ((!g_skipPecCheckInStub) && (!Adbms6833_VerifyBlockPec(p, ADBMS6833_CELL_GROUP_DATA_BYTES)))
        {
            snapshot->afe[afe].valid = false;
            g_protoStats.pec_error_count++;
            continue;
        }

        if ((uint8_t)(cell_base_index + 0u) < ADBMS6833_CELL_CH_PER_AFE)
        {
            snapshot->afe[afe].cell_raw_0p1mV[cell_base_index + 0u] = Adbms6833_ReadU16Le(&p[0]);
        }

        if ((uint8_t)(cell_base_index + 1u) < ADBMS6833_CELL_CH_PER_AFE)
        {
            snapshot->afe[afe].cell_raw_0p1mV[cell_base_index + 1u] = Adbms6833_ReadU16Le(&p[2]);
        }

        if ((uint8_t)(cell_base_index + 2u) < ADBMS6833_CELL_CH_PER_AFE)
        {
            snapshot->afe[afe].cell_raw_0p1mV[cell_base_index + 2u] = Adbms6833_ReadU16Le(&p[4]);
        }

        snapshot->afe[afe].valid = true;
    }

    return BMU_OK;
}

static Bmu_ReturnType Adbms6833_ReadAllCellGroups(Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t rx[ADBMS6833_MAX_FRAME_BYTES];
    Bmu_ReturnType ret;
    uint16_t rx_len;

    if (snapshot == (Adbms6833_StackSnapshotType *)0)
    {
        return BMU_E_PARAM;
    }

    rx_len = (uint16_t)(g_totalAfes * (ADBMS6833_CELL_GROUP_DATA_BYTES + ADBMS6833_PEC_LEN));

    /* Group A -> cells 0..2 */
    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDCVA,
                                      g_totalAfes,
                                      ADBMS6833_CELL_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }
    ret = Adbms6833_DecodeCellGroup(rx, g_totalAfes, 0u, snapshot);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* Group B -> cells 3..5 */
    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDCVB,
                                      g_totalAfes,
                                      ADBMS6833_CELL_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }
    ret = Adbms6833_DecodeCellGroup(rx, g_totalAfes, 3u, snapshot);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* Group C -> cells 6..8 */
    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDCVC,
                                      g_totalAfes,
                                      ADBMS6833_CELL_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }
    ret = Adbms6833_DecodeCellGroup(rx, g_totalAfes, 6u, snapshot);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* Group D -> cells 9..11 (template only)
     * Add more groups if your exact part exposes more cell groups.
     */
    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDCVD,
                                      g_totalAfes,
                                      ADBMS6833_CELL_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }
    ret = Adbms6833_DecodeCellGroup(rx, g_totalAfes, 9u, snapshot);
    if (ret != BMU_OK)
    {
        return ret;
    }

    return BMU_OK;
}

/* ============================================================
 * Decode: AUX / GPIO groups
 * ============================================================ */

static Bmu_ReturnType Adbms6833_DecodeAuxGroup(const uint8_t *rx_buf,
                                               uint8_t afe_count,
                                               uint8_t gpio_base_index,
                                               Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t afe;
    const uint8_t *p;

    if ((rx_buf == (const uint8_t *)0) || (snapshot == (Adbms6833_StackSnapshotType *)0))
    {
        return BMU_E_PARAM;
    }

    for (afe = 0u; afe < afe_count; afe++)
    {
        p = &rx_buf[(uint16_t)afe * (ADBMS6833_AUX_GROUP_DATA_BYTES + ADBMS6833_PEC_LEN)];

        if ((!g_skipPecCheckInStub) &&  (!Adbms6833_VerifyBlockPec(p, ADBMS6833_AUX_GROUP_DATA_BYTES)) )
        {
            snapshot->afe[afe].valid = false;
            g_protoStats.pec_error_count++;
            continue;
        }

        if ((uint8_t)(gpio_base_index + 0u) < ADBMS6833_GPIO_CH_PER_AFE)
        {
            snapshot->afe[afe].gpio_raw_0p1mV[gpio_base_index + 0u] = Adbms6833_ReadU16Le(&p[0]);
        }

        if ((uint8_t)(gpio_base_index + 1u) < ADBMS6833_GPIO_CH_PER_AFE)
        {
            snapshot->afe[afe].gpio_raw_0p1mV[gpio_base_index + 1u] = Adbms6833_ReadU16Le(&p[2]);
        }

        if ((uint8_t)(gpio_base_index + 2u) < ADBMS6833_GPIO_CH_PER_AFE)
        {
            snapshot->afe[afe].gpio_raw_0p1mV[gpio_base_index + 2u] = Adbms6833_ReadU16Le(&p[4]);
        }

        snapshot->afe[afe].valid = true;
    }

    return BMU_OK;
}

static Bmu_ReturnType Adbms6833_ReadAllAuxGroups(Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t rx[ADBMS6833_MAX_FRAME_BYTES];
    Bmu_ReturnType ret;
    uint16_t rx_len;

    if (snapshot == (Adbms6833_StackSnapshotType *)0)
    {
        return BMU_E_PARAM;
    }

    rx_len = (uint16_t)(g_totalAfes * (ADBMS6833_AUX_GROUP_DATA_BYTES + ADBMS6833_PEC_LEN));

    /* AUXA -> GPIO 0..2 */
    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDAUXA,
                                      g_totalAfes,
                                      ADBMS6833_AUX_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }
    ret = Adbms6833_DecodeAuxGroup(rx, g_totalAfes, 0u, snapshot);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* AUXB -> GPIO 3..5 */
    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDAUXB,
                                      g_totalAfes,
                                      ADBMS6833_AUX_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }
    ret = Adbms6833_DecodeAuxGroup(rx, g_totalAfes, 3u, snapshot);
    if (ret != BMU_OK)
    {
        return ret;
    }

    /* Add AUXC/AUXD if needed for remaining GPIO channels. */
    return BMU_OK;
}

/* ============================================================
 * Decode: balancing
 * ============================================================ */

/* Template assumption:
 * 2 bytes per AFE, bitfield of balance channels 0..15
 * Replace with exact meaning from datasheet/register map.
 */
static Bmu_ReturnType Adbms6833_DecodeBalanceGroup(const uint8_t *rx_buf,
                                                   uint8_t afe_count,
                                                   Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t afe;
    const uint8_t *p;
    uint16_t bal_mask;
    uint8_t ch;

    if ((rx_buf == (const uint8_t *)0) || (snapshot == (Adbms6833_StackSnapshotType *)0))
    {
        return BMU_E_PARAM;
    }

    for (afe = 0u; afe < afe_count; afe++)
    {
        p = &rx_buf[(uint16_t)afe * (ADBMS6833_BAL_GROUP_DATA_BYTES + ADBMS6833_PEC_LEN)];

        if ((!g_skipPecCheckInStub) && (!Adbms6833_VerifyBlockPec(p, ADBMS6833_BAL_GROUP_DATA_BYTES)) )
        {
            snapshot->afe[afe].valid = false;
            g_protoStats.pec_error_count++;
            continue;
        }

        bal_mask = Adbms6833_ReadU16Le(&p[0]);

        for (ch = 0u; ch < ADBMS6833_CELL_CH_PER_AFE; ch++)
        {
            snapshot->afe[afe].balancing[ch] =
                ((bal_mask & ((uint16_t)1u << ch)) != 0u) ? true : false;
        }

        snapshot->afe[afe].valid = true;
    }

    return BMU_OK;
}

static Bmu_ReturnType Adbms6833_ReadAllBalanceGroups(Adbms6833_StackSnapshotType *snapshot)
{
    uint8_t rx[ADBMS6833_MAX_FRAME_BYTES];
    Bmu_ReturnType ret;
    uint16_t rx_len;

    if (snapshot == (Adbms6833_StackSnapshotType *)0)
    {
        return BMU_E_PARAM;
    }

    rx_len = (uint16_t)(g_totalAfes * (ADBMS6833_BAL_GROUP_DATA_BYTES + ADBMS6833_PEC_LEN));

    ret = Adbms6833_ReadRegisterGroup(ADBMS6833_CMD_RDBAL,
                                      g_totalAfes,
                                      ADBMS6833_BAL_GROUP_DATA_BYTES,
                                      rx,
                                      rx_len);
    if (ret != BMU_OK)
    {
        return ret;
    }

    return Adbms6833_DecodeBalanceGroup(rx, g_totalAfes, snapshot);
}

/* ============================================================
 * High-level snapshot read
 * ============================================================ */

Bmu_ReturnType Adbms6833_ReadSnapshot(Adbms6833_StackSnapshotType *snapshot)
{
    Bmu_ReturnType ret;

    if (snapshot == (Adbms6833_StackSnapshotType *)0)
    {
        return BMU_E_PARAM;
    }

    if ((g_totalAfes == 0u) || (g_totalAfes > ADBMS6833_MAX_AFES_PER_STACK))
    {
        return BMU_E_RANGE;
    }

    Adbms6833_ClearSnapshot(snapshot);

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

    /* Replace later with exact conversion timing */
    AdbmsPlatform_DelayUs(300u);

    ret = Adbms6833_ReadAllCellGroups(snapshot);
    if (ret != BMU_OK)
    {
        g_protoStats.decode_error_count++;
        return ret;
    }

    ret = Adbms6833_ReadAllAuxGroups(snapshot);
    if (ret != BMU_OK)
    {
        g_protoStats.decode_error_count++;
        return ret;
    }

    ret = Adbms6833_ReadAllBalanceGroups(snapshot);
    if (ret != BMU_OK)
    {
        g_protoStats.decode_error_count++;
        return ret;
    }

    return BMU_OK;
}
