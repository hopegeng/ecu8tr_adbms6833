#ifndef ADBMS6830_BALANCE_H
#define ADBMS6830_BALANCE_H

#include "adbms6830_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool chargingActive;
    bool faultActive;
    bool balancingEnabled;

    uint16_t balanceStart_mV;
    uint16_t balanceStop_mV;
    uint16_t balanceMargin_mV;
    uint16_t minCellForBalance_mV;
} Adbms6830_BalanceConfig_t;

typedef struct
{
    uint16_t cellMin_mV;
    uint16_t cellMax_mV;
    uint16_t delta_mV;
    uint16_t avg_mV;
    uint16_t dccMask;
} Adbms6830_BalanceResult_t;

typedef struct
{
    Adbms6830_BalanceConfig_t cfg;
    Adbms6830_BalanceResult_t result[ADBMS6830_MAX_ICS];
} Adbms6830_BalanceContext_t;

void Adbms6830_BalanceInit(Adbms6830_BalanceContext_t *ctx);
void Adbms6830_BalanceEvaluate(Adbms6830_BalanceContext_t *bal,
                               Adbms6830_Context_t *drv);
Adbms6830_Status_t Adbms6830_BalanceApplyDcc(Adbms6830_BalanceContext_t *bal,
                                             Adbms6830_Context_t *drv,
                                             const Adbms6830_Hal_t *hal,
                                             const Adbms6830_CommandSet_t *cmds);

#ifdef __cplusplus
}
#endif

#endif /* ADBMS6830_BALANCE_H */
