/*
 * qspi0mstr_illd.h
 *
 *  Created on: Oct 23, 2024
 *      Author: Ahmed
 */

#ifndef BSP_QSPI_INC_QSPI0MSTR_ILLD_H_
#define BSP_QSPI_INC_QSPI0MSTR_ILLD_H_
#include "stdbool.h"
#include "shell.h"
#include "IfxQspi_SpiMaster.h"
#include "qspi.h"

void qspi0mstr_Init_iLLD(void);
extern SpiIf_Status qspi0_send_receive_iLLD(QspiCs_t CS, uint16 u16Length, uint8* pu8SrcBuff, uint8* pu8DstBuff);
bool qspimstr_waitForRxDone_iLLD( void );
bool qspimstr_waitForRxDoneTimeout_iLLD( uint32 timeoutLoops );
bool qspimstr_waitForRxDone( void );

#endif /* BSP_QSPI_INC_QSPI0MSTR_ILLD_H_ */
