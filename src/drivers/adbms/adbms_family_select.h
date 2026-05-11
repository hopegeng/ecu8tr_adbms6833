#ifndef SRC_DRIVERS_ADBMS_ADBMS_FAMILY_SELECT_H_
#define SRC_DRIVERS_ADBMS_ADBMS_FAMILY_SELECT_H_

#include "ecu8tr_cmd.h"

#define ADBMS_DEVICE_FAMILY_6830    (6830u)
#define ADBMS_DEVICE_FAMILY_6833    (6833u)
#define ADBMS_DEVICE_FAMILY_LTC6812 (6812u)
#define ADBMS_DEVICE_FAMILY_AUTO    (0u)

#ifndef ADBMS_DEVICE_FAMILY
#define ADBMS_DEVICE_FAMILY         ADBMS_DEVICE_FAMILY_AUTO
#endif

#if (ADBMS_DEVICE_FAMILY == ADBMS_DEVICE_FAMILY_AUTO)

#include "adbms_runtime_detect.h"

typedef AdbmsRuntime_SharedSnapshot_t AdbmsSharedSnapshot_t;

#define AdbmsSharedRead                     AdbmsRuntime_SharedRead
#define ADBMS_SHARED_SAMPLE_PERIOD_MS       ADBMS_RUNTIME_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_CORE2_MAIN_FN                 adbms_runtime_main_on_core2
#define ADBMS_GET_STATE_FN                  AdbmsRuntime_GetState
#define ADBMS_REQUEST_START_FN              AdbmsRuntime_RequestStart
#define ADBMS_GET_DETECTED_DEVICE_FN        AdbmsRuntime_GetDetectedDevice

#elif (ADBMS_DEVICE_FAMILY == ADBMS_DEVICE_FAMILY_6830)

#include "adbms6830_on_core2/adbms6830_shared.h"

typedef Adbms6830_SharedSnapshot_t AdbmsSharedSnapshot_t;

#define AdbmsSharedRead                     Adbms6830_SharedRead
#define ADBMS_SHARED_SAMPLE_PERIOD_MS       ADBMS6830_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_CORE2_MAIN_FN                 adbms6830_main_on_core2
#define ADBMS_GET_STATE_FN                  adbms6830_getState
#define ADBMS_REQUEST_START_FN              AdbmsFixedFamily_RequestStart
#define ADBMS_GET_DETECTED_DEVICE_FN        AdbmsFixedFamily_GetDetectedDevice

#elif (ADBMS_DEVICE_FAMILY == ADBMS_DEVICE_FAMILY_6833)

#include "adbms6833_on_core2/adbms6833_shared.h"

typedef Adbms6833_SharedSnapshot_t AdbmsSharedSnapshot_t;

#define AdbmsSharedRead                     Adbms6833_SharedRead
#define ADBMS_SHARED_SAMPLE_PERIOD_MS       ADBMS6833_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_CORE2_MAIN_FN                 adbms6833_main_on_core2
#define ADBMS_GET_STATE_FN                  adbms6833_getState
#define ADBMS_REQUEST_START_FN              AdbmsFixedFamily_RequestStart
#define ADBMS_GET_DETECTED_DEVICE_FN        AdbmsFixedFamily_GetDetectedDevice

#elif (ADBMS_DEVICE_FAMILY == ADBMS_DEVICE_FAMILY_LTC6812)

#include "ltc6812_on_core2/ltc6812_shared.h"

typedef Ltc6812_SharedSnapshot_t AdbmsSharedSnapshot_t;

#define AdbmsSharedRead                     Ltc6812_SharedRead
#define ADBMS_SHARED_SAMPLE_PERIOD_MS       LTC6812_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_CORE2_MAIN_FN                 ltc6812_main_on_core2
#define ADBMS_GET_STATE_FN                  ltc6812_getState
#define ADBMS_REQUEST_START_FN              AdbmsFixedFamily_RequestStart
#define ADBMS_GET_DETECTED_DEVICE_FN        AdbmsFixedFamily_GetDetectedDevice

#else
#error "Unsupported ADBMS_DEVICE_FAMILY selection."
#endif

extern void ADBMS_CORE2_MAIN_FN(void);
extern ECU8TR_ADBMS6830_State_t ADBMS_GET_STATE_FN(void);

static inline void AdbmsFixedFamily_RequestStart(void)
{
}

static inline uint16_t AdbmsFixedFamily_GetDetectedDevice(void)
{
    return (uint16_t)ADBMS_DEVICE_FAMILY;
}

#endif /* SRC_DRIVERS_ADBMS_ADBMS_FAMILY_SELECT_H_ */
