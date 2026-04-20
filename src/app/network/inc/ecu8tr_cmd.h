/*
 * ecu8_cmd.h
 *
 *  Created on: Feb 2, 2024
 *      Author: rgeng
 */

#ifndef APP_NET_INC_ECU8TR_CMD_H_
#define APP_NET_INC_ECU8TR_CMD_H_

#include <Ifx_Types.h>
#include "adbms6830.h"

#define CMD_BODY_SIZE 32
#define MAX_TLE9012_DATA_ELEMENT  32
#define MAX_ADBMS6830_DATA_ELEMENT  (ADBMS6830_NUMBER_OF_NODES*(1+ADBMS6830_NO_OF_CELLS))


typedef enum
{
	UPGRADE_STATUS_IDLE = 0,
	UPGRADE_STATUS_IN_PROGRESS = 1,
	UPGRADE_STATUS_SUCCESS = 2,
	UPGRADE_STATUS_FAILURE = 3,
	UPGRADE_STATUS_NONE = 4,
} ECU8TR_UPGRADE_STATUS_e;

typedef enum
{
	CMD_NET_SET_MAC = 0,
	CMD_NET_SET_IP = 1,
	CMD_NET_SET_NETMASK = 2,
	CMD_NET_SET_GATEWAY = 3,
	CMD_NET_SHOW_NETWORK = 4,
	CMD_ADBMS6830_WAKEUP = 5,
	CMD_ADBMS6830_SLEEP = 6,
	CMD_ADBMS6830_RUNNING_MODE = 7,
	CMD_ECU8TR_VERSION = 8,
	CMD_RESET = 15,

	CMD_NONE
} 	ECU8TR_CMD_e;

typedef enum
{
	ECU8TR_TLE9012_IDLE = 0,
	ECU8TR_TLE9012_SLEEP = 1,
	ECU8TR_TLE9012_WAKEUP = 2,
	ECU8TR_TLE9012_WAKEUP_DONE = 3,
	ECU8TR_TLE9012_DATA_STREAMING = 4,
	ECU8TR_TLE9012_NONE = 6,
} ECU8TR_TLE9012_State_t;

typedef enum
{
	ECU8TR_ADBMS6830_IDLE = 0,
	ECU8TR_ADBMS6830_SLEEP = 1,
	ECU8TR_ADBMS6830_WAKEUP = 2,
	ECU8TR_ADBMS6830_WAKEUP_DONE = 3,
	ECU8TR_ADBMS6830_DATA_STREAMING = 4,
	ECU8TR_ADBMS6830_NONE = 6,
}ECU8TR_ADBMS6830_State_t;

typedef struct __packed__
{

	ECU8TR_CMD_e cmd_code;
	char cmd_body[ CMD_BODY_SIZE ];
	uint16_t delimiter;	/* "$$" */
} ECU8TR_CMD_t;


typedef struct __packed__
{
	uint8 reg;
	uint16 reg_value;
} ECU8TR_DATA_BODY_t;

typedef struct __packed__
{
	uint8 dev;
	uint8 body_cnt;
	ECU8TR_DATA_BODY_t data_body[ MAX_TLE9012_DATA_ELEMENT ];
	uint32 crc;
	uint16 delimiter;	/* "$$" */
} ECU8TR_DATA_t;


typedef struct __packed__
{
	uint8 dev[ADBMS6830_NUMBER_OF_NODES];
	uint8 body_cnt;
	ECU8TR_DATA_BODY_t data_body[ MAX_ADBMS6830_DATA_ELEMENT ];
	uint32 crc;
	uint16 delimiter;	/* "$$" */
} ECU8TR_ADBMS6830_DATA_t;

extern ECU8TR_TLE9012_State_t ecu8tr_getTLE9012State( void );

#endif /* APP_NET_INC_ECU8TR_CMD_H_ */
