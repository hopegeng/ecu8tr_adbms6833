#ifndef ADBMS6833_PEC_H
#define ADBMS6833_PEC_H

#define ADBMS_COMMON_ENABLE_PEC10
#include "../adbms_common_pec.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint16_t Adbms_Pec15_Calc(const uint8_t *data, uint16_t len)
{
    return AdbmsCommon_Pec15Calc(data, len);
}

static uint16_t Adbms_Pec10_Calc(const uint8_t *data, uint8_t cmdCounter, uint16_t len)
{
    return AdbmsCommon_Pec10Calc(data, cmdCounter, len);
}

#ifdef __cplusplus
}
#endif

#endif /* ADBMS6833_PEC_H */
