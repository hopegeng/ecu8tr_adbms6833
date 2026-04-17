/**
 ******************************************************************************
 * @file 	bms.h
 * @version V0.1.0
 * @date    Jan2024
 * @brief	Low level BMS driver header file
 ******************************************************************************
 * @attention
 *
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2019 Neutron Controls</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
 ******************************************************************************
 */

#ifndef TLE9012_TLE9012_ALT_H_
#define TLE9012_TLE9012_ALT_H_

#include <stdint.h>
#include <stdbool.h>
#include "_Reg/IfxAsclin_reg.h"
#include <Ifx_Types.h>
#include "ecu8tr_cmd.h"


#define ISOUART_COMM_SUCCESS (0x00u)
#define ISOUART_COMM_FAILURE (0x01u)

typedef enum
{
	TLE9012_BITWIDTH_16 = 0,	/**< MoT Tx or MoB ring Rx */
	TLE9012_BITWIDTH_10 = 1,	/**< MoB Tx or MoT ring Rx */
	/**************/
	ISOUART_BITWIDTH_NONE
} TLE9012_BITWIDTH_t;


/**
 * @brief UARTs connected to a ring network of TLE9012 battery monitor ICs
 *
 * Note that the mapping of UART channel to network topology is verified by
 * inspection of the MOT_MOB_N bit of the TLE9012 GEN_DIAG register.
 */




#ifdef __cplusplus
extern "C" {
#endif

extern uint8 tle9012_rdReg( uint8_t device, uint8_t reg, uint8_t *pRet );
extern boolean tle9012_wrReg( uint8_t device, uint8_t reg, uint16_t val, uint8_t *pRet );
extern uint8 tle9012_enumerateNetwork( uint8_t adc_flag, uint8_t nodeNr, uint8_t *pRet );
extern void tle9012_tle9015Wakeup( uint8 level );
extern void tle9012_doPulseWakeup( void);
extern void tle9012_emmInit( void );
extern boolean tle9012_doFullWakeup( void );
extern boolean tle9012_sleep( void );
extern uint8 tle9012_getNodeNr( void );
extern void tle9012_readMetrics( uint8 nodeNr, ECU8TR_DATA_t *pData );
extern void tle9012_appyBitWidth( TLE9012_BITWIDTH_t bitWidth );
extern boolean tle9012_setBitWidth( int bitWidth );
extern TLE9012_BITWIDTH_t tle9012_getBitWidth( void );
extern boolean tle9012_resetWDT(void);
extern boolean tle9012_isWakeup( void );

#ifdef __cplusplus
}
#endif

#endif /* TLE9012_TLE9012_ALT_H_ */
