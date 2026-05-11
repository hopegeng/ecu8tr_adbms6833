/*
 * bmu_csc_acq.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_BMU_BMU_CSC_ACQ_H_
#define SRC_APP_BMU_BMU_CSC_ACQ_H_

#include <stdint.h>
#include <stdbool.h>

#include "../../drivers/adbms/adbms_family_select.h"

void Bmu_CscAcq_Init(void);
void Bmu_CscAcq_StartMeasurement(void);
bool Bmu_CscAcq_IsMeasurementActive(void);
bool Bmu_CscAcq_GetLastSnapshot(AdbmsSharedSnapshot_t *snapshot);
void Bmu_CscAcq_MainTask_20ms(uint32_t now_ms);


#endif /* SRC_APP_BMU_BMU_CSC_ACQ_H_ */
