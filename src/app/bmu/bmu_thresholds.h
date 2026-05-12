/*
 * bmu_thresholds.h
 *
 * Stores the active OV/UV/OT limits received via SCU1_LimitConfigInfo and
 * exposes helpers used by the CAN transmit layer to build flag messages.
 */

#ifndef SRC_APP_BMU_BMU_THRESHOLDS_H_
#define SRC_APP_BMU_BMU_THRESHOLDS_H_

#include <stdbool.h>
#include <stdint.h>
#include "../dbc/dbc_trace_interface.h"

/* Safe power-on defaults: nothing triggers until the tester sends thresholds.
 *   OV = 0xFFFF (65.535 V) — unreachable, OV never fires
 *   UV = 0      (0 V)      — unsigned compare, UV never fires
 */
#define BMU_THRESHOLD_DEFAULT_OV_RAW_0P001V  (0xFFFFu)
#define BMU_THRESHOLD_DEFAULT_UV_RAW_0P001V  (0u)
#define BMU_THRESHOLD_DEFAULT_OT_RAW_0P1C    ((int16_t)1000)
#define BMU_THRESHOLD_DEFAULT_UT_RAW_0P1C    ((int16_t)(-1000))

void Bmu_Thresholds_Init(void);
void Bmu_Thresholds_Set(const Dbc_ThresholdSettingsType *s);
void Bmu_Thresholds_Get(Dbc_ThresholdSettingsType *s_out);

/* Return true if the supplied cell voltage (0.1 mV/bit) crosses the threshold. */
bool Bmu_Thresholds_IsCellOv(uint16_t cell_voltage_raw_0p1mV);
bool Bmu_Thresholds_IsCellUv(uint16_t cell_voltage_raw_0p1mV);

#endif /* SRC_APP_BMU_BMU_THRESHOLDS_H_ */
