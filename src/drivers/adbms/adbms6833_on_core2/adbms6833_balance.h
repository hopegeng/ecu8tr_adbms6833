#ifndef ADBMS6833_BALANCE_H
#define ADBMS6833_BALANCE_H

#include "adbms6833_driver.h"

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
} Adbms6833_BalanceConfig_t;

typedef struct
{
    uint16_t cellMin_mV;
    uint16_t cellMax_mV;
    uint16_t delta_mV;
    uint16_t avg_mV;
    uint16_t dccMask;
} Adbms6833_BalanceResult_t;

typedef struct
{
    Adbms6833_BalanceConfig_t cfg;
    Adbms6833_BalanceResult_t result[ADBMS6833_MAX_ICS];
} Adbms6833_BalanceContext_t;

void Adbms6833_BalanceInit(Adbms6833_BalanceContext_t *ctx);
void Adbms6833_BalanceEvaluate(Adbms6833_BalanceContext_t *bal,
                               Adbms6833_Context_t *drv);
Adbms6833_Status_t Adbms6833_BalanceApplyDcc(Adbms6833_BalanceContext_t *bal,
                                             Adbms6833_Context_t *drv,
                                             const Adbms6833_Hal_t *hal,
                                             const Adbms6833_CommandSet_t *cmds);

#ifdef __cplusplus
}
#endif

#endif /* ADBMS6833_BALANCE_H */
