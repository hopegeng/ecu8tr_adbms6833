/**
 * \file adbmsCommon.h
 *
 * \brief ADBMSCommon header file.
 *
 * \author rlarocque
 *  Created on: May 5, 2022
 *
 */

#ifndef BMS_DRV_INC_ADBMSCOMMON_H_
#define BMS_DRV_INC_ADBMSCOMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>


//Constants
#define PEC10_LENGTH				2u


extern uint64_t adbmsCommon_isoSPI_wakeup_timeoutUs;

extern void adbmsCommon_delayMillseconds(uint32_t delay);
/***************************** Source File Types ******************************/
extern void adbmsCommon_isoSPI_wakeup_timeoutUs_init(void);
extern uint16_t adbmsCommon_Calc_pec15( uint8_t *data , uint32_t len );
extern uint16_t adbmsCommon_Calc_pec10( uint8_t *data ,uint8_t temp_cmdCnt, uint16_t len );
extern uint8_t adbmsCommon_Calc_bytes_for_pec15( uint16_t len );
extern uint8_t adbmsCommon_Calc_id_byte_for_pec15( uint8_t bytesPerPec, bool isRead );
extern void adbmsCommon_init( void );

#endif /* BMS_DRV_INC_ADBMSCOMMON_H_ */
