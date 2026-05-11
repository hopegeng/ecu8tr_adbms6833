#ifndef LTC6812_PEC_H
#define LTC6812_PEC_H

#include "../adbms_common_pec.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint16_t Ltc6812_Pec15Calc(const uint8_t *data, uint16_t len)
{
    return AdbmsCommon_Pec15Calc(data, len);
}

#ifdef __cplusplus
}
#endif

#endif /* LTC6812_PEC_H */
