#include "adbms6830_balance.h"

#include <string.h>

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

void Adbms6830_BalanceInit(Adbms6830_BalanceContext_t *ctx)
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

void Adbms6830_BalanceEvaluate(Adbms6830_BalanceContext_t *bal,
                               Adbms6830_Context_t *drv)
{
    uint8_t ic;
    uint16_t globalDelta = 0U;

    if ((bal == NULL) || (drv == NULL))
    {
        return;
    }

    for (ic = 0U; ic < drv->icCount; ic++)
    {
        Adbms6830_BalanceResult_t *res = &bal->result[ic];
        res->cellMin_mV = Adbms_Min(drv->cell[ic].mV, ADBMS6830_CELLS_PER_IC);
        res->cellMax_mV = Adbms_Max(drv->cell[ic].mV, ADBMS6830_CELLS_PER_IC);
        res->avg_mV = Adbms_Avg(drv->cell[ic].mV, ADBMS6830_CELLS_PER_IC);
        res->delta_mV = (uint16_t)(res->cellMax_mV - res->cellMin_mV);
        res->dccMask = 0U;

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

        if (bal->result[ic].cellMax_mV < bal->cfg.minCellForBalance_mV)
        {
            continue;
        }

        for (cell = 0U; cell < ADBMS6830_CELLS_PER_IC; cell++)
        {
            if (drv->cell[ic].mV[cell] > threshold)
            {
                bal->result[ic].dccMask |= (uint16_t)(1U << cell);
            }
        }
    }
}

Adbms6830_Status_t Adbms6830_BalanceApplyDcc(Adbms6830_BalanceContext_t *bal,
                                             Adbms6830_Context_t *drv,
                                             const Adbms6830_Hal_t *hal,
                                             const Adbms6830_CommandSet_t *cmds)
{
    uint8_t ic;

    if ((bal == NULL) || (drv == NULL) || (hal == NULL) || (cmds == NULL))
    {
        return ADBMS6830_ERR_PARAM;
    }

    for (ic = 0U; ic < drv->icCount; ic++)
    {
        Adbms6830_CfgbPackDccPlaceholder(bal->result[ic].dccMask, drv->cfgb[ic].data);
    }

    return Adbms6830_WriteCfgb(drv, hal, cmds);
}
