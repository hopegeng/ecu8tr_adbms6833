/*
 * ltc6812_debug.c
 *
 *  Created on: May 4, 2026
 *      Author: rgeng
 */


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "qspi0mstr_illd.h"
#include "shell.h"
#include "ltc6812_driver.h"

/* Adjust these for your project */
#define LTC6812_MAX_IC          1u      /* set to your daisy-chain count max */
#define LTC6812_CMD_LEN         4u
#define LTC6812_REG_DATA_LEN    6u
#define LTC6812_REG_PEC_LEN     2u
#define LTC6812_RX_PER_IC       8u

#define LTC6812_CMD0_RDSTATB    0x00u
#define LTC6812_CMD1_RDSTATB    0x12u

static Ltc6812_Status_t Ltc6812_QspiTransfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    SpiIf_Status retVal;

    if ((tx == 0) || (rx == 0) || (len == 0u))
    {
        return LTC6812_ERR_PARAM;
    }

    retVal = qspi0_send_receive_iLLD(eQspiHwCs02, len, (uint8_t *)tx, rx);
    if (retVal == SpiIf_Status_ok)
    {
        if (qspimstr_waitForRxDoneTimeout_iLLD(1000000u) == true)
        {
            return LTC6812_OK;
        }

        return LTC6812_ERR_TIMEOUT;
    }

    return LTC6812_ERR_COMM;
}


/*
 * Rename these to match your existing Ltc6812_Status_t.
 * Example only:
 *
 * typedef enum {
 *     LTC6812_OK = 0,
 *     LTC6812_ERROR,
 *     LTC6812_ERROR_PARAM,
 *     LTC6812_ERROR_PEC
 * } Ltc6812_Status_t;
 */

static uint16_t Ltc6812_Pec15Calc(const uint8_t *data, uint16_t len)
{
    const uint16_t poly = 0x4599u;
    uint16_t remainder = 16u;   /* LTC681x PEC initial value */

    for (uint16_t i = 0u; i < len; i++)
    {
        remainder ^= ((uint16_t)data[i] << 7);

        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            if ((remainder & 0x4000u) != 0u)
            {
                remainder = (uint16_t)((remainder << 1) ^ poly);
            }
            else
            {
                remainder = (uint16_t)(remainder << 1);
            }
        }
    }

    /*
     * LTC681x PEC15 is transmitted left-shifted by 1.
     * Bit0 is always 0.
     */
    return (uint16_t)(remainder << 1);
}

static void Ltc6812_BuildCommand(uint8_t cmd0, uint8_t cmd1, uint8_t cmd[4])
{
    uint16_t pec;

    cmd[0] = cmd0;
    cmd[1] = cmd1;

    pec = Ltc6812_Pec15Calc(cmd, 2u);

    cmd[2] = (uint8_t)(pec >> 8);
    cmd[3] = (uint8_t)(pec & 0xFFu);
}

static bool Ltc6812_CheckRegisterPec(const uint8_t reg_rx[8])
{
    uint16_t received_pec;
    uint16_t calculated_pec;

    calculated_pec = Ltc6812_Pec15Calc(reg_rx, LTC6812_REG_DATA_LEN);
    received_pec  = ((uint16_t)reg_rx[6] << 8) | reg_rx[7];

    return (received_pec == calculated_pec);
}

/*
 * Reads Status Register Group B and extracts the revision code.
 *
 * rev_code[ic] receives REV[3:0].
 * raw_statb can be NULL, or point to total_ic * 8 bytes.
 *
 * IMPORTANT:
 * Ltc6812_QspiTransfer() must keep CS low for the entire transaction:
 * command bytes + dummy bytes used to clock out the reply.
 */
Ltc6812_Status_t Ltc6812_TestReadRevisionCode(uint8_t total_ic,
                                              uint8_t *rev_code,
                                              uint8_t *raw_statb)
{
    uint8_t tx[LTC6812_CMD_LEN + (LTC6812_RX_PER_IC * LTC6812_MAX_IC)];
    uint8_t rx[LTC6812_CMD_LEN + (LTC6812_RX_PER_IC * LTC6812_MAX_IC)];
    uint16_t transfer_len;
    Ltc6812_Status_t status;

    if ((total_ic == 0u) || (total_ic > LTC6812_MAX_IC) || (rev_code == NULL))
    {
        return LTC6812_ERR_PARAM;   /* rename to your enum */
    }

    transfer_len = (uint16_t)(LTC6812_CMD_LEN + (LTC6812_RX_PER_IC * total_ic));

    memset(tx, 0xFF, sizeof(tx));
    memset(rx, 0x00, sizeof(rx));

    /*
     * RDSTATB command.
     * Command bytes should become:
     *   tx[0] = 0x00
     *   tx[1] = 0x12
     *   tx[2] = 0x70
     *   tx[3] = 0x24
     */
    Ltc6812_BuildCommand(LTC6812_CMD0_RDSTATB, LTC6812_CMD1_RDSTATB, tx);

    /*
     * Send command + dummy 0xFF clocks in one CS-low transaction.
     * rx[0..3] are normally dummy/ignored.
     * rx[4..] contains the returned register data.
     */
    status = Ltc6812_QspiTransfer(tx, rx, transfer_len);
    if (status != LTC6812_OK)          /* rename to your enum */
    {
        return status;
    }

    for (uint8_t ic = 0u; ic < total_ic; ic++)
    {
        const uint8_t *p = &rx[LTC6812_CMD_LEN + (ic * LTC6812_RX_PER_IC)];

        if (Ltc6812_CheckRegisterPec(p) == false)
        {
            return LTC6812_ERR_PEC; /* rename to your enum */
        }

        /*
         * Status Register Group B:
         * STBR5 bit[7:4] = REV[3:0]
         */
        rev_code[ic] = (uint8_t)((p[5] >> 4) & 0x0Fu);

        if (raw_statb != NULL)
        {
            memcpy(&raw_statb[ic * LTC6812_RX_PER_IC], p, LTC6812_RX_PER_IC);
        }
    }

    return LTC6812_OK;
}


void Ltc6812_DebugTest(void)
{
    uint8_t rev = 0u;
    uint8_t raw[8];
    Ltc6812_Status_t st;

    st = Ltc6812_TestReadRevisionCode(1u, &rev, raw);

    if (st == LTC6812_OK)
    {
        /* Put breakpoint here.
         * raw[0..5] = Status Register Group B data
         * raw[6..7] = received PEC
         * rev       = LTC6812 revision code
         */
    }
    else
    {
        /* Check QSPI mode, CS timing, wakeup, isoSPI/LTC6820, and PEC. */
    }
}
