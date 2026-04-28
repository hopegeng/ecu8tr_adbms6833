#include "adbms6833_balance.h"

#include <string.h>

static uint16_t Adbms6833_SelectParityConstrainedMask(const uint16_t *cellMilliVolt,
                                                      uint16_t requestedMask,
                                                      uint16_t thresholdMilliVolt,
                                                      Adbms6833_BalanceParity_t *selectedParity)
{
    uint16_t oddMask = 0U;
    uint16_t evenMask = 0U;
    uint8_t oddCount = 0U;
    uint8_t evenCount = 0U;
    uint32_t oddScore = 0U;
    uint32_t evenScore = 0U;
    uint8_t cellIdx;

    if (cellMilliVolt == NULL)
    {
        if (selectedParity != NULL)
        {
            *selectedParity = ADBMS6833_BALANCE_PARITY_NONE;
        }
        return 0U;
    }

    for (cellIdx = 0U; cellIdx < ADBMS6833_CELLS_PER_IC; cellIdx++)
    {
        uint16_t cellBit = (uint16_t)(1U << cellIdx);

        if ((requestedMask & cellBit) == 0U)
        {
            continue;
        }

        /* 0-based index 0 maps to cell 1, so even index means odd-numbered cell. */
        if ((cellIdx & 1U) == 0U)
        {
            oddMask |= cellBit;
            oddCount++;
            oddScore += (uint32_t)(cellMilliVolt[cellIdx] - thresholdMilliVolt);
        }
        else
        {
            evenMask |= cellBit;
            evenCount++;
            evenScore += (uint32_t)(cellMilliVolt[cellIdx] - thresholdMilliVolt);
        }
    }

    if (oddCount > evenCount)
    {
        if (selectedParity != NULL)
        {
            *selectedParity = ADBMS6833_BALANCE_PARITY_ODD;
        }
        return oddMask;
    }

    if (evenCount > oddCount)
    {
        if (selectedParity != NULL)
        {
            *selectedParity = ADBMS6833_BALANCE_PARITY_EVEN;
        }
        return evenMask;
    }

    if (oddScore >= evenScore)
    {
        if (selectedParity != NULL)
        {
            *selectedParity = ADBMS6833_BALANCE_PARITY_ODD;
        }
        return oddMask;
    }

    if (selectedParity != NULL)
    {
        *selectedParity = ADBMS6833_BALANCE_PARITY_EVEN;
    }
    return evenMask;
}

static uint16_t Adbms_Min(const uint16_t *data, uint8_t len)
{
    uint8_t i;
    uint16_t v = data[0];
    for (i = 1U; i < len; i++)
    {
        if (data[i] < v)
        {
            v = data[i];
        }
    }
    return v;
}

static uint16_t Adbms_Max(const uint16_t *data, uint8_t len)
{
    uint8_t i;
    uint16_t v = data[0];
    for (i = 1U; i < len; i++)
    {
        if (data[i] > v)
        {
            v = data[i];
        }
    }
    return v;
}

static uint16_t Adbms_Avg(const uint16_t *data, uint8_t len)
{
    uint8_t i;
    uint32_t sum = 0U;
    for (i = 0U; i < len; i++)
    {
        sum += data[i];
    }
    return (uint16_t)(sum / len);
}

void Adbms6833_BalanceInit(Adbms6833_BalanceContext_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg.balanceStart_mV = 15U;
    ctx->cfg.balanceStop_mV = 5U;
    ctx->cfg.balanceMargin_mV = 10U;
    ctx->cfg.minCellForBalance_mV = 3700U;
}

void Adbms6833_BalanceEvaluate(Adbms6833_BalanceContext_t *bal,
                               Adbms6833_Context_t *drv)
{
    uint8_t ic;
    uint16_t globalDelta = 0U;

    if ((bal == NULL) || (drv == NULL))
    {
        return;
    }

    for (ic = 0U; ic < drv->icCount; ic++)
    {
        Adbms6833_BalanceResult_t *res = &bal->result[ic];
        res->cellMin_mV = Adbms_Min(drv->cell[ic].mV, ADBMS6833_CELLS_PER_IC);
        res->cellMax_mV = Adbms_Max(drv->cell[ic].mV, ADBMS6833_CELLS_PER_IC);
        res->avg_mV = Adbms_Avg(drv->cell[ic].mV, ADBMS6833_CELLS_PER_IC);
        res->delta_mV = (uint16_t)(res->cellMax_mV - res->cellMin_mV);
        res->dccMask = 0U;
        res->selectedParity = ADBMS6833_BALANCE_PARITY_NONE;

        if (res->delta_mV > globalDelta)
        {
            globalDelta = res->delta_mV;
        }
    }

    if (bal->cfg.balancingEnabled == false)
    {
        if (globalDelta >= bal->cfg.balanceStart_mV)
        {
            bal->cfg.balancingEnabled = true;
        }
    }
    else
    {
        if (globalDelta <= bal->cfg.balanceStop_mV)
        {
            bal->cfg.balancingEnabled = false;
        }
    }

    if ((bal->cfg.chargingActive == false) ||
        (bal->cfg.faultActive == true) ||
        (bal->cfg.balancingEnabled == false))
    {
        return;
    }

    for (ic = 0U; ic < drv->icCount; ic++)
    {
        uint8_t cell;
        uint16_t threshold = (uint16_t)(bal->result[ic].cellMin_mV + bal->cfg.balanceMargin_mV);
        uint16_t requestedMask = 0U;

        if (bal->result[ic].cellMax_mV < bal->cfg.minCellForBalance_mV)
        {
            continue;
        }

        for (cell = 0U; cell < ADBMS6833_CELLS_PER_IC; cell++)
        {
            if (drv->cell[ic].mV[cell] > threshold)
            {
                requestedMask |= (uint16_t)(1U << cell);
            }
        }

        bal->result[ic].dccMask = Adbms6833_SelectParityConstrainedMask(drv->cell[ic].mV,
                                                                        requestedMask,
                                                                        threshold,
                                                                        &bal->result[ic].selectedParity);

        if (bal->result[ic].dccMask == 0U)
        {
            bal->result[ic].selectedParity = ADBMS6833_BALANCE_PARITY_NONE;
        }
    }
}

Adbms6833_Status_t Adbms6833_BalanceApplyDcc(Adbms6833_BalanceContext_t *bal,
                                             Adbms6833_Context_t *drv,
                                             const Adbms6833_Hal_t *hal,
                                             const Adbms6833_CommandSet_t *cmds)
{
    uint8_t ic;

    if ((bal == NULL) || (drv == NULL) || (hal == NULL) || (cmds == NULL))
    {
        return ADBMS6833_ERR_PARAM;
    }

    for (ic = 0U; ic < drv->icCount; ic++)
    {
        Adbms6833_CfgbPackDccPlaceholder(bal->result[ic].dccMask, drv->cfgb[ic].data);
    }

    return Adbms6833_WriteCfgb(drv, hal, cmds);
}
