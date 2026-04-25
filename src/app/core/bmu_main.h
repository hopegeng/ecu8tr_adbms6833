/*
 * bmu_main.h
 *
 *  Created on: Apr 17, 2026
 *      Author: rgeng
 */

#ifndef SRC_APP_DBC_BMU_MAIN_H_
#define SRC_APP_DBC_BMU_MAIN_H_


#include <stdint.h>

void Bmu_Init(void);
void Bmu_Task_10ms(uint32_t now_ms);
void Bmu_Task_20ms(uint32_t now_ms);
void Bmu_Task_100ms(uint32_t now_ms);
void Bmu_Task_1000ms(uint32_t now_ms);


#endif /* SRC_APP_DBC_BMU_MAIN_H_ */
