#include "ltc6812_balance.h"
#include "shell.h"
#include <string.h>

static uint16_t Ltc6812_Min(const uint16_t *data, uint8_t len)
{
    uint8_t i;
    uint16_t value = data[0];

    for (i = 1u; i < len; i++)
    {
        if (data[i] < value)
        {
            value = data[i];
        }
    }

    return value;
}

static uint16_t Ltc6812_Max(const uint16_t *data, uint8_t len)
{
    uint8_t i;
    uint16_t value = data[0];

    for (i = 1u; i < len; i++)
    {
        if (data[i] > value)
        {
            value = data[i];
        }
    }

    return value;
}

static uint16_t Ltc6812_Avg(const uint16_t *data, uint8_t len)
{
    uint8_t i;
    uint32_t sum = 0u;

    for (i = 0u; i < len; i++)
    {
        sum += data[i];
    }

    return (uint16_t)(sum / len);
}

static uint16_t Ltc6812_SelectParityConstrainedMask(const uint16_t *cellMilliVolt,
                                                    uint16_t requestedMask,
                                                    uint16_t thresholdMilliVolt,
                                                    Ltc6812_BalanceParity_t *selectedParity)
{
    uint16_t oddMask = 0u;
    uint16_t evenMask = 0u;
    uint8_t oddCount = 0u;
    uint8_t evenCount = 0u;
    uint32_t oddScore = 0u;
    uint32_t evenScore = 0u;
    uint8_t cellIdx;

    if (cellMilliVolt == 0)
    {
        if (selectedParity != 0)
        {
            *selectedParity = LTC6812_BALANCE_PARITY_NONE;
        }
        return 0u;
    }

    for (cellIdx = 0u; cellIdx < LTC6812_CELLS_PER_IC; cellIdx++)
    {
        uint16_t cellBit = (uint16_t)(1u << cellIdx);

        if ((requestedMask & cellBit) == 0u)
        {
            continue;
        }

        if ((cellIdx & 1u) == 0u)
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

    if ((oddCount > evenCount) || ((oddCount == evenCount) && (oddScore >= evenScore)))
    {
        if (selectedParity != 0)
        {
            *selectedParity = (oddMask == 0u) ? LTC6812_BALANCE_PARITY_NONE : LTC6812_BALANCE_PARITY_ODD;
        }
        return oddMask;
    }

    if (selectedParity != 0)
    {
        *selectedParity = (evenMask == 0u) ? LTC6812_BALANCE_PARITY_NONE : LTC6812_BALANCE_PARITY_EVEN;
    }
    return evenMask;
}

void Ltc6812_BalanceInit(Ltc6812_BalanceContext_t *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->cfg.balanceStart_mV = 15u;			//For testing, changed to 13u from 15U
    ctx->cfg.balanceStop_mV = 5u;
    ctx->cfg.balanceMargin_mV = 10u;
    ctx->cfg.minCellForBalance_mV = 3700u; 	//For testing, changed to 2000U from 3700u;
}

void Ltc6812_BalanceEvaluate(Ltc6812_BalanceContext_t *bal, Ltc6812_Context_t *drv)
{
    uint8_t ic;
    uint16_t globalDelta = 0u;

    if ((bal == 0) || (drv == 0))
    {
        return;
    }

    for (ic = 0u; ic < drv->icCount; ic++)
    {
        Ltc6812_BalanceResult_t *res = &bal->result[ic];
        res->cellMin_mV = Ltc6812_Min(drv->cell[ic].mV, LTC6812_CELLS_PER_IC);
        res->cellMax_mV = Ltc6812_Max(drv->cell[ic].mV, LTC6812_CELLS_PER_IC);
        res->avg_mV = Ltc6812_Avg(drv->cell[ic].mV, LTC6812_CELLS_PER_IC);
        res->delta_mV = (uint16_t)(res->cellMax_mV - res->cellMin_mV);
        res->dccMask = 0u;
        res->selectedParity = LTC6812_BALANCE_PARITY_NONE;

        if (res->delta_mV > globalDelta)
        {
            globalDelta = res->delta_mV;
        }
    }

    if (bal->cfg.balancingEnabled == false)
    {
        bal->cfg.balancingEnabled = (globalDelta >= bal->cfg.balanceStart_mV);
    }
    else if (globalDelta <= bal->cfg.balanceStop_mV)
    {
        bal->cfg.balancingEnabled = false;
    }

    if ((bal->cfg.chargingActive == false) ||
        (bal->cfg.faultActive == true) ||
        (bal->cfg.balancingEnabled == false))
    {
        return;
    }

    for (ic = 0u; ic < drv->icCount; ic++)
    {
        uint8_t cell;
        uint16_t requestedMask = 0u;
        uint16_t threshold = (uint16_t)(bal->result[ic].cellMin_mV + bal->cfg.balanceMargin_mV);

        if (bal->result[ic].cellMax_mV < bal->cfg.minCellForBalance_mV)
        {
            continue;
        }

        for (cell = 0u; cell < LTC6812_CELLS_PER_IC; cell++)
        {
            if (drv->cell[ic].mV[cell] > threshold)
            {
                requestedMask |= (uint16_t)(1u << cell);
            }
        }

        bal->result[ic].dccMask = Ltc6812_SelectParityConstrainedMask(drv->cell[ic].mV,
                                                                      requestedMask,
                                                                      threshold,
                                                                      &bal->result[ic].selectedParity);
    }
}

Ltc6812_Status_t Ltc6812_BalanceApplyDcc(Ltc6812_BalanceContext_t *bal,
                                         Ltc6812_Context_t *drv,
                                         const Ltc6812_Hal_t *hal,
                                         const Ltc6812_CommandSet_t *cmds)
{
    uint8_t ic;

    if ((bal == 0) || (drv == 0) || (hal == 0) || (cmds == 0))
    {
        return LTC6812_ERR_PARAM;
    }

    for (ic = 0u; ic < drv->icCount; ic++)
    {
        Ltc6812_CfgaPackDcc(bal->result[ic].dccMask, drv->cfga[ic].data);
        Ltc6812_CfgbPackDcc(bal->result[ic].dccMask, drv->cfgb[ic].data);
    }

    {
        Ltc6812_Status_t st = Ltc6812_WriteCfga(drv, hal, cmds);
        if (st != LTC6812_OK)
        {
            return st;
        }
    }

    return Ltc6812_WriteCfgb(drv, hal, cmds);
}
