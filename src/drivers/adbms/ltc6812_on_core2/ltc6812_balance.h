#ifndef LTC6812_BALANCE_H
#define LTC6812_BALANCE_H

#include "ltc6812_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LTC6812_BALANCE_PARITY_NONE = 0,
    LTC6812_BALANCE_PARITY_ODD,
    LTC6812_BALANCE_PARITY_EVEN
} Ltc6812_BalanceParity_t;

typedef struct
{
    bool chargingActive;
    bool faultActive;
    bool balancingEnabled;
    uint16_t balanceStart_mV;
    uint16_t balanceStop_mV;
    uint16_t balanceMargin_mV;
    uint16_t minCellForBalance_mV;
} Ltc6812_BalanceConfig_t;

typedef struct
{
    uint16_t cellMin_mV;
    uint16_t cellMax_mV;
    uint16_t delta_mV;
    uint16_t avg_mV;
    uint16_t dccMask;
    Ltc6812_BalanceParity_t selectedParity;
} Ltc6812_BalanceResult_t;

typedef struct
{
    Ltc6812_BalanceConfig_t cfg;
    Ltc6812_BalanceResult_t result[LTC6812_MAX_ICS];
    uint16_t lastAppliedDcc[LTC6812_MAX_ICS];
} Ltc6812_BalanceContext_t;

void Ltc6812_BalanceInit(Ltc6812_BalanceContext_t *ctx);
void Ltc6812_BalanceEvaluate(Ltc6812_BalanceContext_t *bal, Ltc6812_Context_t *drv);
bool Ltc6812_BalanceHasAnyDccSet(const Ltc6812_BalanceContext_t *bal, uint8_t icCount);
void Ltc6812_BalanceUpdateLastAppliedDcc(Ltc6812_BalanceContext_t *bal, uint8_t icCount);
Ltc6812_Status_t Ltc6812_BalanceApplyDcc(Ltc6812_BalanceContext_t *bal,
                                         Ltc6812_Context_t *drv,
                                         const Ltc6812_Hal_t *hal,
                                         const Ltc6812_CommandSet_t *cmds);

#ifdef __cplusplus
}
#endif

#endif /* LTC6812_BALANCE_H */
