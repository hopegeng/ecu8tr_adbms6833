/*
 * adbms6833_platform.h
 *
 *  Created on: Apr 19, 2026
 *      Author: rgeng
 */

#ifndef SRC_DRIVERS_ADBMS_PLATFORM_ADBMS6833_PLATFORM_H_
#define SRC_DRIVERS_ADBMS_PLATFORM_ADBMS6833_PLATFORM_H_


#include <stdint.h>
#include <stdbool.h>
#include "bmu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Platform init / timing
 * ========================= */
void AdbmsPlatform_Init(void);
void AdbmsPlatform_DelayUs(uint32_t delay_us);
uint32_t AdbmsPlatform_GetTimeMs(void);

/* =========================
 * SPI / chip-select
 * Stubbed for now
 * ========================= */
void AdbmsPlatform_CsAssert(void);
void AdbmsPlatform_CsDeassert(void);

Bmu_ReturnType AdbmsPlatform_SpiTransfer(const uint8_t *tx,
                                         uint8_t *rx,
                                         uint16_t len);

/* =========================
 * Debug / stub helpers
 * ========================= */
void AdbmsPlatform_SetStubEchoMode(bool enable);
void AdbmsPlatform_SetStubTimeMs(uint32_t time_ms);

/* =========================
 * PEC15 helper
 * Polynomial 0x4599
 * ========================= */
uint16_t AdbmsPlatform_Pec15(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif


#endif /* SRC_DRIVERS_ADBMS_PLATFORM_ADBMS6833_PLATFORM_H_ */
