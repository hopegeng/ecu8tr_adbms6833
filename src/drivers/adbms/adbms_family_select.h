#ifndef SRC_DRIVERS_ADBMS_ADBMS_FAMILY_SELECT_H_
#define SRC_DRIVERS_ADBMS_ADBMS_FAMILY_SELECT_H_

#include "ecu8tr_cmd.h"

#define ADBMS_DEVICE_FAMILY_6830    (6830u)
#define ADBMS_DEVICE_FAMILY_6833    (6833u)

#ifndef ADBMS_DEVICE_FAMILY
#define ADBMS_DEVICE_FAMILY         ADBMS_DEVICE_FAMILY_6830
#endif

#if (ADBMS_DEVICE_FAMILY == ADBMS_DEVICE_FAMILY_6830)

#include "adbms6830_on_core2/adbms6830_shared.h"

typedef Adbms6830_SharedSnapshot_t AdbmsSharedSnapshot_t;

#define AdbmsSharedRead                     Adbms6830_SharedRead
#define ADBMS_SHARED_SAMPLE_PERIOD_MS       ADBMS6830_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_CORE2_MAIN_FN                 adbms6830_main_on_core2
#define ADBMS_GET_STATE_FN                  adbms6830_getState

#elif (ADBMS_DEVICE_FAMILY == ADBMS_DEVICE_FAMILY_6833)

#include "adbms6833_on_core2/adbms6833_shared.h"

typedef Adbms6833_SharedSnapshot_t AdbmsSharedSnapshot_t;

#define AdbmsSharedRead                     Adbms6833_SharedRead
#define ADBMS_SHARED_SAMPLE_PERIOD_MS       ADBMS6833_SHARED_SAMPLE_PERIOD_MS
#define ADBMS_CORE2_MAIN_FN                 adbms6833_main_on_core2
#define ADBMS_GET_STATE_FN                  adbms6833_getState

#else
#error "Unsupported ADBMS_DEVICE_FAMILY selection."
#endif

extern void ADBMS_CORE2_MAIN_FN(void);
extern ECU8TR_ADBMS6830_State_t ADBMS_GET_STATE_FN(void);

#endif /* SRC_DRIVERS_ADBMS_ADBMS_FAMILY_SELECT_H_ */
