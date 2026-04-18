/*
 * bmu_csc_acq.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_DBC_BMU_CSC_ACQ_H_
#define SRC_APP_DBC_BMU_CSC_ACQ_H_

#include <stdint.h>

void Bmu_CscAcq_Init(void);
void Bmu_CscAcq_MainTask_20ms(uint32_t now_ms);


#endif /* SRC_APP_DBC_BMU_CSC_ACQ_H_ */
