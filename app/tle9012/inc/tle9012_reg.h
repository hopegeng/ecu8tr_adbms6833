/*
 * tle9012_reg.h
 *
 *  Created on: Jul 21, 2020
 *      Author: robertkearney
 */

#include <stdint.h>

#ifndef TLE9012_TLE9012_REG_H_
#define TLE9012_TLE9012_REG_H_

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_PART_CONFIG_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_EN_CELL_0  =  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_1  =  1, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_2  =  2, // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_3  =  3, // @emem [ 3] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_4  =  4, // @emem [ 4] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_5  =  5, // @emem [ 5] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_6  =  6, // @emem [ 6] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_7  =  7, // @emem [ 7] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_8  =  8, // @emem [ 8] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_9  =  9, // @emem [ 9] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_10 = 10, // @emem [10] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_EN_CELL_11 = 11, // @emem [11] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_PART_CONFIG_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_PART_CONFIG_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_0  =  1, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_1  =  1, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_2  =  1, // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_3  =  1, // @emem [ 3] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_4  =  1, // @emem [ 4] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_5  =  1, // @emem [ 5] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_6  =  1, // @emem [ 6] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_7  =  1, // @emem [ 7] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_8  =  1, // @emem [ 8] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_9  =  1, // @emem [ 9] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_10 =  1, // @emem [10] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_EN_CELL_11 =  1, // @emem [11] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_PART_CONFIG_Configuration_Width_t;

typedef union TLE9012_PART_CONFIG_Register_u
{
	struct TLE9012_PART_CONFIG_Register_s
	{
		uint16_t  EN_CELL0:  1; /* Enable cell monitoring for cell 0 */
		uint16_t  EN_CELL1:  1; /* Enable cell monitoring for cell 1 */
		uint16_t  EN_CELL2:  1; /* Enable cell monitoring for cell 2 */
		uint16_t  EN_CELL3:  1; /* Enable cell monitoring for cell 3 */
		uint16_t  EN_CELL4:  1; /* Enable cell monitoring for cell 4 */
		uint16_t  EN_CELL5:  1; /* Enable cell monitoring for cell 5 */
		uint16_t  EN_CELL6:  1; /* Enable cell monitoring for cell 6 */
		uint16_t  EN_CELL7:  1; /* Enable cell monitoring for cell 7 */
		uint16_t  EN_CELL8:  1; /* Enable cell monitoring for cell 8 */
		uint16_t  EN_CELL9:  1; /* Enable cell monitoring for cell 9 */
		uint16_t  EN_CELL10: 1; /* Enable cell monitoring for cell 10 */
		uint16_t  EN_CELL11: 1; /* Enable cell monitoring for cell 11 */
		uint16_t  RES:       3; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_PART_CONFIG_Register_t;


/* Cell voltage thresholds (supplied in sleep) */

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_OL_OV_THR_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_OV_THR  		=  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_OL_THR_MAX  	= 10  // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_OL_OV_THR_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_OL_OV_THR_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_OV_THR  	= 10, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_OL_THR_MAX 	=  6  // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_OL_OV_THR_Configuration_Width_t;


typedef union TLE9012_OL_OV_THR_Register_u
{
	struct TLE9012_OL_OV_THR_Register_s
	{
		uint16_t  OV_THR:     10; /* Overvoltage fault threshold (LSB10) */
		uint16_t  OL_THR_MAX:  6; /* Openload maximum voltage drop threshold (LSB6) */
	} B;
	uint16_t U16;

} TLE9012_OL_OV_THR_Register_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_OL_UV_THR_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_UV_THR  		=  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_OL_THR_MIN  	= 10  // @emem [11] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_OL_UV_THR_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_OL_UV_THR_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_UV_THR  	= 10, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_OL_THR_MIN  =  6  // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_OL_UV_THR_Configuration_Width_t;


/* Cell voltage thresholds (supplied in sleep) */
typedef union TLE9012_OL_UV_THR_Register_u
{
	struct TLE9012_OL_UV_THR_Register_s
	{
		uint16_t  UV_THR:     10; /* Undervoltage fault threshold (LSB10) */
		uint16_t  OL_THR_MIN:  6; /* Openload minimum voltage drop threshold (LSB6) */
	} B;
	uint16_t U16;

} TLE9012_OL_UV_THR_Register_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_TEMP_CONF_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_EXT_OT_THR  		=  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_I_NTC  			= 10, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_NR_TEMP_SENSE  	= 12  // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_TEMP_CONF_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_TEMP_CONF_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_EXT_OT_THR  	= 10, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_I_NTC  			=  2, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_NR_TEMP_SENSE  	=  3  // @emem [11] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_TEMP_CONF_Configuration_Width_t;


/* Temperature measurement configuration (supplied in sleep) */
typedef union TLE9012_TEMP_CONF_Register_u
{
	struct TLE9012_TEMP_CONF_Register_s
	{
		uint16_t  EXT_OT_THR:    10; /* External temperature overtemperature threshold */
		uint16_t  I_NTC:          2; /* Current source used for OT fault */
		uint16_t  NR_TEMP_SENSE:  3; /* Number of external temperature sensors */
		uint16_t  RES:            1; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_TEMP_CONF_Register_t;

// @enum TLE9012_INT_OT_THR_Configuration_Width_t | Defines Internal temperature overtemperature threshold position
typedef enum TLE9012_INT_OT_THR_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_INT_OT_THR  =  0  // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_INT_OT_THR_Configuration_Position_t;

// @enum TLE9012_INT_OT_THR_Configuration_Width_t | Defines Internal temperature overtemperature threshold width
typedef enum TLE9012_INT_OT_THR_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_INT_OT_THR  =  10  // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_INT_OT_THR_Configuration_Width_t;


/* Internal temperature measurement configuration (supplied in sleep)*/
typedef union TLE9012_INT_OT_WARN_CONF_Register_u
{
	struct TLE9012_INT_OT_WARN_CONF_Register_s
	{
		uint16_t  INT_OT_THR: 10; /* Internal temperature overtemperature threshold */
		uint16_t  RES:         6; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_INT_OT_WARN_CONF_Register_t;
/* Round Robin ERR counters (supplied in sleep) */
typedef union TLE9012_RR_ERR_CNT_Register_u
{
	struct TLE9012_RR_ERR_CNT_Register_s
	{
		uint16_t  NR_ERR:             3; /* Number of errors */
		uint16_t  NR_EXT_TEMP_START:  3; /* External temperature triggering in round robin */
		uint16_t  RR_SLEEP_CNT:      10; /* Round Robin timing in sleep mode */
	} B;
	uint16_t U16;

} TLE9012_RR_ERR_CNT_Register_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_RR_CONFIG_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_RR_CNT            =  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_RR_SYNC           =  7, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_ADC_ERR  =  8, // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_OL_ERR   =  9, // @emem [ 3] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_EXT_OT   = 10, // @emem [ 4] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_INT_OT   = 11, // @emem [ 5] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_CELL_UV  = 12, // @emem [ 6] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_CELL_OV  = 13, // @emem [ 7] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_BAL_UC   = 14, // @emem [ 8] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_M_NR_ERR_BAL_OC   = 15  // @emem [ 9] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_RR_CONFIG_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_RR_CONFIG_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_RR_CNT            =  7, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_RR_SYNC           =  1, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_ADC_ERR  =  1, // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_OL_ERR   =  1, // @emem [ 3] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_EXT_OT   =  1, // @emem [ 4] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_INT_OT   =  1, // @emem [ 5] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_CELL_UV  =  1, // @emem [ 6] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_CELL_OV  =  1, // @emem [ 7] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_BAL_UC   =  1, // @emem [ 8] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_M_NR_ERR_BAL_OC   =  1  // @emem [ 9] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_RR_CONFIG_Configuration_Width_t;


/* Round Robin configuration (supplied in sleep) */
typedef union TLE9012_RR_CONFIG_Register_u
{
	struct TLE9012_RR_CONFIG_Register_s
	{
		uint16_t  RR_CNT:           7; /* Round Robin counter */
		uint16_t  RR_SYNC:          1; /* Round Robin synchronization */
		uint16_t  M_NR_ERR_ADC_ERR: 1; /* mask NR_ERR counter for ADC error */
		uint16_t  M_NR_ERR_OL_ERR:  1; /* mask NR_ERR counter for open load error */
		uint16_t  M_NR_ERR_EXT_OT:  1; /* mask NR_ERR counter for external temperature error */
		uint16_t  M_NR_ERR_INT_OT:  1; /* mask NR_ERR counter for internal temperature error */
		uint16_t  M_NR_ERR_CELL_UV: 1; /* mask NR_ERR counter for undervoltage error */
		uint16_t  M_NR_ERR_CELL_OV: 1; /* mask NR_ERR counter for overvoltage error */
		uint16_t  M_NR_ERR_BAL_UC:  1; /* mask NR_ERR counter for balancing error undercurrent */
		uint16_t  M_NR_ERR_BAL_OC:  1; /* mask NR_ERR counter for balancing error overcurrent */
	} B;
	uint16_t U16;

} TLE9012_RR_CONFIG_Register_t;
/* ERR pin / EMM mask (supplied in sleep) */
typedef union TLE9012_FAULT_MASK_Register_u
{
	struct TLE9012_FAULT_MASK_Register_s
	{
		uint16_t  RES:           5; /* Reserved for future use */
		uint16_t  ERR_PIN:       1; /* Enable Error PIN functionality */
		uint16_t  M_ADC_ERR:     1; /* EMM/ERR mask for ADC error */
		uint16_t  M_OL_ERR:      1; /* EMM/ERR mask for openload error */
		uint16_t  M_INT_IC_ERR:  1; /* EMM/ERR mask for internal IC error */
		uint16_t  M_REG_CRC_ERR: 1; /* EMM/ERR mask for register CRC error */
		uint16_t  M_EXT_OT:      1; /* EMM/ERR mask for external temperature error */
		uint16_t  M_INT_OT:      1; /* EMM/ERR mask for internal temperature error */
		uint16_t  M_CELL_UV:     1; /* EMM/ERR mask for cell undervoltage error */
		uint16_t  M_CELL_OV:     1; /* EMM/ERR mask for cell overvoltage error */
		uint16_t  M_BAL_ERR_UC:  1; /* EMM/ERR mask for balancing error undercurrent */
		uint16_t  M_BAL_ERR_OC:  1; /* EMM/ERR mask for balancing error overcurrent */
	} B;
	uint16_t U16;

} TLE9012_FAULT_MASK_Register_t;
/* General diagnosis (supplied in sleep) */
typedef union TLE9012_GEN_DIAG_Register_u
{
	struct TLE9012_GEN_DIAG_Register_s
	{
		uint16_t  GPIO_WAKEUP:  1; /* Wake-up via GPIO */
		uint16_t  MOT_MOB_N:    1; /* Master on Top/Bottom configuration */
		uint16_t  BAL_ACTIVE:   1; /* Balancing active */
		uint16_t  LOCK_MEAS:    1; /* Lock measurement */
		uint16_t  RR_ACTIVE:    1; /* Round Robin active */
		uint16_t  UV_SLEEP:     1; /* Undervoltage induced sleep */
		uint16_t  ADC_ERR:      1; /* ADC Error */
		uint16_t  OL_ERR:       1; /* Openload error */
		uint16_t  INT_IC_ERR:   1; /* Internal IC error */
		uint16_t  REG_CRC_ERR:  1; /* Register CRC error */
		uint16_t  EXT_OT:       1; /* External temperature (OT) error */
		uint16_t  INT_OT:       1; /* Internal temperature (OT) error */
		uint16_t  CELL_UV:      1; /* Cell undervoltage (UV) error */
		uint16_t  CELL_OV:      1; /* Cell overvoltage (OV) error */
		uint16_t  BAL_ERR_UC:   1; /* Balancing error undercurrent (not supplied in sleep mode) */
		uint16_t  BAL_ERR_OC:   1; /* Balancing error overcurrent (not supplied in sleep mode) */
	} B;
	uint16_t U16;

} TLE9012_GEN_DIAG_Register_t;
/* Cell voltage supervision warning flags UV (supplied in sleep)*/
typedef union TLE9012_CELL_UV_Register_u
{
	struct TLE9012_CELL_UV_Register_s
	{
		uint16_t  UV_0:  1; /* Undervoltage in cell 0 */
		uint16_t  UV_1:  1; /* Undervoltage in cell 1 */
		uint16_t  UV_2:  1; /* Undervoltage in cell 2 */
		uint16_t  UV_3:  1; /* Undervoltage in cell 3 */
		uint16_t  UV_4:  1; /* Undervoltage in cell 4 */
		uint16_t  UV_5:  1; /* Undervoltage in cell 5 */
		uint16_t  UV_6:  1; /* Undervoltage in cell 6 */
		uint16_t  UV_7:  1; /* Undervoltage in cell 7 */
		uint16_t  UV_8:  1; /* Undervoltage in cell 8 */
		uint16_t  UV_9:  1; /* Undervoltage in cell 9 */
		uint16_t  UV_10: 1; /* Undervoltage in cell 10 */
		uint16_t  UV_11: 1; /* Undervoltage in cell 11 */
		uint16_t  RES:   4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_CELL_UV_Register_t;
/* Cell voltage supervision warning flags OV(supplied in sleep) */
typedef union TLE9012_CELL_OV_Register_u
{
	struct TLE9012_CELL_OV_Register_s
	{
		uint16_t  OV_0:  1; /* Overvoltage in cell 0 */
		uint16_t  OV_1:  1; /* Overvoltage in cell 1 */
		uint16_t  OV_2:  1; /* Overvoltage in cell 2 */
		uint16_t  OV_3:  1; /* Overvoltage in cell 3 */
		uint16_t  OV_4:  1; /* Overvoltage in cell 4 */
		uint16_t  OV_5:  1; /* Overvoltage in cell 5 */
		uint16_t  OV_6:  1; /* Overvoltage in cell 6 */
		uint16_t  OV_7:  1; /* Overvoltage in cell 7 */
		uint16_t  OV_8:  1; /* Overvoltage in cell 8 */
		uint16_t  OV_9:  1; /* Overvoltage in cell 9 */
		uint16_t  OV_10: 1; /* Overvoltage in cell 10 */
		uint16_t  OV_11: 1; /* Overvoltage in cell 11 */
		uint16_t  RES:   4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_CELL_OV_Register_t;
/* External overtemperature warning flags (supplied in sleep) */
typedef union TLE9012_EXT_TEMP_DIAG_Register_u
{
	struct TLE9012_EXT_TEMP_DIAG_Register_s
	{
		uint16_t  OPEN_0:   1; /* Openload in ext. temp 0 */
		uint16_t  SHORT_0:  1; /* Short in ext. temp 0 */
		uint16_t  OT_0:     1; /* Overtemperature in ext. temp 0 */
		uint16_t  OPEN_1:   1; /* Openload in ext. temp 1 */
		uint16_t  SHORT_1:  1; /* Short in ext. temp 1 */
		uint16_t  OT_1:     1; /* Overtemperature in ext. temp 1 */
		uint16_t  OPEN_2:   1; /* Openload in ext. temp 2 */
		uint16_t  SHORT_2:  1; /* Short in ext. temp 2 */
		uint16_t  OT_2:     1; /* Overtemperature in ext. temp 2 */
		uint16_t  OPEN_3:   1; /* Openload in ext. temp 3 */
		uint16_t  SHORT_3:  1; /* Short in ext. temp 3 */
		uint16_t  OT_3:     1; /* Overtemperature in ext. temp 3 */
		uint16_t  OPEN_4:   1; /* Openload in ext. temp 4 */
		uint16_t  SHORT_4:  1; /* Short in ext. temp 4 */
		uint16_t  OT_4:     1; /* Overtemperature in ext. temp 4 */
		uint16_t  RES:      1; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_EXT_TEMP_DIAG_Register_t;
/* Diagnosis OPENLOAD (supplied in sleep) */
typedef union TLE9012_DIAG_OL_Register_u
{
	struct TLE9012_DIAG_OL_Register_s
	{
		uint16_t  OL_0:  1; /* Openload in cell 0 */
		uint16_t  OL_1:  1; /* Openload in cell 1 */
		uint16_t  OL_2:  1; /* Openload in cell 2 */
		uint16_t  OL_3:  1; /* Openload in cell 3 */
		uint16_t  OL_4:  1; /* Openload in cell 4 */
		uint16_t  OL_5:  1; /* Openload in cell 5 */
		uint16_t  OL_6:  1; /* Openload in cell 6 */
		uint16_t  OL_7:  1; /* Openload in cell 7 */
		uint16_t  OL_8:  1; /* Openload in cell 8 */
		uint16_t  OL_9:  1; /* Openload in cell 9 */
		uint16_t  OL_10: 1; /* Openload in cell 10 */
		uint16_t  OL_11: 1; /* Openload in cell 11 */
		uint16_t  RES:   4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_DIAG_OL_Register_t;
/* REG_CRC_ERR (supplied in sleep) */
typedef union TLE9012_REG_CRC_ERR_Register_u
{
	struct TLE9012_REG_CRC_ERR_Register_s
	{
		uint16_t  ADDR:  7; /* Register Address of register where CRC check failed */
		uint16_t  RES:   9; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_REG_CRC_ERR_Register_t;
/* Operation mode */
typedef union TLE9012_OP_MODE_Register_u
{
	struct TLE9012_OP_MODE_Register_s
	{
		uint16_t  PD:       1; /* Activate sleep mode */
		uint16_t  EXT_WD:   1; /* Extended watchdog */
		uint16_t  RES:     14; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_OP_MODE_Register_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_BAL_CURR_THR_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_OC_THR  =  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_UC_THR  =  8  // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_BAL_CURR_THR_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_BAL_CURR_THR_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_OC_THR    =  8, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_UC_THR    =  8  // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_BAL_CURR_THR_Configuration_Width_t;


/* Balancing current thresholds */
typedef union TLE9012_BAL_CURR_THR_Register_u
{
	struct TLE9012_BAL_CURR_THR_Register_s
	{
		uint16_t  OC_THR:  8; /* Overcurrent fault threshold */
		uint16_t  UC_THR:  8; /* Undercurrent fault threshold */
	} B;
	uint16_t U16;

} TLE9012_BAL_CURR_THR_Register_t;

/* Balance settings */
typedef union TLE9012_BAL_SETTINGS_Register_u
{
	struct TLE9012_BAL_SETTINGS_Register_s
	{
		uint16_t  OL_0:  1; /* Openload in cell 0 */
		uint16_t  OL_1:  1; /* Openload in cell 1 */
		uint16_t  OL_2:  1; /* Openload in cell 2 */
		uint16_t  OL_3:  1; /* Openload in cell 3 */
		uint16_t  OL_4:  1; /* Openload in cell 4 */
		uint16_t  OL_5:  1; /* Openload in cell 5 */
		uint16_t  OL_6:  1; /* Openload in cell 6 */
		uint16_t  OL_7:  1; /* Openload in cell 7 */
		uint16_t  OL_8:  1; /* Openload in cell 8 */
		uint16_t  OL_9:  1; /* Openload in cell 9 */
		uint16_t  OL_10: 1; /* Openload in cell 10 */
		uint16_t  OL_11: 1; /* Openload in cell 11 */
		uint16_t  RES:   4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_BAL_SETTINGS_Register_t;
// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_AVM_CONFIG_Configuration_Position_tag
{
	TLE9012_CONFIGURATION_POSITION_TEMP_MUX_DIAG_SEL  =  0, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_AVM_TMP0_MASK      =  3, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_AVM_TMP1_MASK      =  4, // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_AVM_AUX0_MASK      =  6, // @emem [ 3] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_AVM_AUX1_MASK      =  7, // @emem [ 4] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_POSITION_R_DIAG             =  8  // @emem [ 5] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_AVM_CONFIG_Configuration_Position_t;

// @enum TLE9012_PART_CONFIG_Configuration_Position_t | Defines Cell Enable
typedef enum TLE9012_AVM_CONFIG_Configuration_Width_tag
{
	TLE9012_CONFIGURATION_WIDTH_TEMP_MUX_DIAG_SEL  	=  3, // @emem [ 0] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_AVM_TMP0_MASK  		=  1, // @emem [ 1] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_AVM_TMP1_MASK  		=  1, // @emem [ 2] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_AVM_AUX0_MASK  		=  1, // @emem [ 3] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_AVM_AUX1_MASK  		=  1, // @emem [ 4] STATIC INITIALIZATION ONLY Cell # from initial define
	TLE9012_CONFIGURATION_WIDTH_R_DIAG         		=  1  // @emem [ 5] STATIC INITIALIZATION ONLY Cell # from initial define
}TLE9012_AVM_CONFIG_Configuration_Width_t;


/* Auxiliary Voltage Measurement Configuration */
typedef union TLE9012_AVM_CONFIG_Register_u
{
	struct TLE9012_AVM_CONFIG_Register_s
	{
		uint16_t  TEMP_MUX_DIAG_SEL:  3; /* Selector for ext. temp diagnose */
		uint16_t  AVM_TMP0_MASK:      1; /* Masking ext. temp 0 measurement as part of AVM */
		uint16_t  AVM_TMP1_MASK:      1; /* Masking ext. temp 1 measurement as part of AVM */
		uint16_t  RES_0:              1; /* Reserved for future use */
		uint16_t  AVM_AUX0_MASK:      1; /* Masking auxiliary measurement 0 as part of AVM */
		uint16_t  AVM_AUX1_MASK:      1; /* Masking auxiliary measurement 1 as part of AVM */
		uint16_t  RES_1:              1; /* Reserved for future use */
		uint16_t  R_DIAG:             1; /* Masking diagnosis resistor as part of AVM */
		uint16_t  RES_2:              6; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_AVM_CONFIG_Register_t;
/* Measurement control */
typedef union TLE9012_MEAS_CTRL_Register_u
{
	struct TLE9012_MEAS_CTRL_Register_s
	{
		uint16_t  CVM_DEL:        5; /* CVM delay timer */
		uint16_t  PBOFF:          1; /* Enable PBOFF time */
		uint16_t  RES:            1; /* Reserved for future use */
		uint16_t  AVM_START:      1; /* Start auxiliary voltage measurement */
		uint16_t  BVM_BIT_WIDTH:  3; /* Bit width of block voltage measurement */
		uint16_t  BVM_START:      1; /* Start block voltage measurement */
		uint16_t  CVM_BIT_WIDTH:  3; /* Bit width of cell voltage measurement */
		uint16_t  CVM_START:      1; /* Start cell voltage measurement */
	} B;
	uint16_t U16;

} TLE9012_MEAS_CTRL_Register_t;
/* Cell voltage measurement 0 */
typedef union TLE9012_CVM_Register_u
{
	struct TLE9012_CVM_Register_s
	{
		uint16_t  RESULT: 16; /* Result of cell voltage measurement */
	} B;
	uint16_t U16;

} TLE9012_CVM_Register_t;
/* Block voltage measurement */
typedef union TLE9012_BVM_Register_u
{
	struct TLE9012_BVM_Register_s
	{
		uint16_t  RESULT: 16; /* Result of block voltage measurement */
	} B;
	uint16_t U16;

} TLE9012_BVM_Register_t;
/* Temp result 0 */
typedef union TLE9012_EXT_TEMP_Register_u
{
	struct TLE9012_EXT_TEMP_Register_s
	{
		uint16_t  RESULT:   10; /* Result of block voltage measurement */
		uint16_t  INTC:      2; /* Indicates which current source was used */
		uint16_t  PULLDOWN:  1; /* Indicating pulldown switch state */
		uint16_t  VALID:     1; /* Indicating a valid result */
		uint16_t  RES:       2; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_EXT_TEMP_Register_t;
/* Temp result R Diagnose */
typedef union TLE9012_EXT_TEMP_R_DIAG_Register_u
{
	struct TLE9012_EXT_TEMP_R_DIAG_Register_s
	{
		uint16_t  RESULT:   10; /* Result of block voltage measurement */
		uint16_t  INTC:      2; /* Indicates which current source was used */
		uint16_t  RES_0:     1; /* Reserved for future use */
		uint16_t  VALID:     1; /* Indicating a valid result */
		uint16_t  RES_1:     2; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_EXT_TEMP_R_DIAG_Register_t;
/* Chip temperature */
typedef union TLE9012_INT_TEMP_Register_u
{
	struct TLE9012_INT_TEMP_Register_s
	{
		uint16_t  RESULT:   10; /* Result of block voltage measurement */
		uint16_t  RES_0:     3; /* Reserved for future use */
		uint16_t  VALID:     1; /* Indicating a valid result */
		uint16_t  RES_1:     2; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_INT_TEMP_Register_t;
/* Multiread command */
typedef union TLE9012_MULTI_READ_Register_u
{
	struct TLE9012_MULTI_READ_Register_s
	{
		uint16_t  RES:      16; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_MULTI_READ_Register_t;
/* Multiread Configuration */
typedef union TLE9012_MULTI_READ_CFG_Register_u
{
	struct TLE9012_MULTI_READ_CFG_Register_s
	{
		uint16_t  CVM_SEL:         4; /* Selects which CVM results are part of the multiread */
		uint16_t  BVM_SEL:         1; /* Selects if BVM result is part of the multiread */
		uint16_t  EXT_TEMP_SEL:    3; /* Selects which TEMP result is part of the multiread */
		uint16_t  EXT_TEMP_R_SEL:  1; /* Selects if R_DIAG result is part of the multiread */
		uint16_t  INT_TEMP_SEL:    1; /* Selects if INT_TEMP result is part of the multiread */
		uint16_t  RES:             6; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_MULTI_READ_CFG_Register_t;
/* Passive bal. diagnosis OVERCURRENT */
typedef union TLE9012_BAL_DIAG_OC_Register_u
{
	struct TLE9012_BAL_DIAG_OC_Register_s
	{
		uint16_t  OC_0:  1; /* Balancing overcurrent in cell 0 */
		uint16_t  OC_1:  1; /* Balancing overcurrent in cell 1 */
		uint16_t  OC_2:  1; /* Balancing overcurrent in cell 2 */
		uint16_t  OC_3:  1; /* Balancing overcurrent in cell 3 */
		uint16_t  OC_4:  1; /* Balancing overcurrent in cell 4 */
		uint16_t  OC_5:  1; /* Balancing overcurrent in cell 5 */
		uint16_t  OC_6:  1; /* Balancing overcurrent in cell 6 */
		uint16_t  OC_7:  1; /* Balancing overcurrent in cell 7 */
		uint16_t  OC_8:  1; /* Balancing overcurrent in cell 8 */
		uint16_t  OC_9:  1; /* Balancing overcurrent in cell 9 */
		uint16_t  OC_10: 1; /* Balancing overcurrent in cell 10 */
		uint16_t  OC_11: 1; /* Balancing overcurrent in cell 11 */
		uint16_t  RES:   4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_BAL_DIAG_OC_Register_t;
/* Passive bal. diagnosis UNDERCURRENT */
typedef union TLE9012_BAL_DIAG_UC_Register_u
{
	struct TLE9012_BAL_DIAG_UC_Register_s
	{
		uint16_t  UC_0:  1; /* Balancing undercurrent in cell 0 */
		uint16_t  UC_1:  1; /* Balancing undercurrent in cell 1 */
		uint16_t  UC_2:  1; /* Balancing undercurrent in cell 2 */
		uint16_t  UC_3:  1; /* Balancing undercurrent in cell 3 */
		uint16_t  UC_4:  1; /* Balancing undercurrent in cell 4 */
		uint16_t  UC_5:  1; /* Balancing undercurrent in cell 5 */
		uint16_t  UC_6:  1; /* Balancing undercurrent in cell 6 */
		uint16_t  UC_7:  1; /* Balancing undercurrent in cell 7 */
		uint16_t  UC_8:  1; /* Balancing undercurrent in cell 8 */
		uint16_t  UC_9:  1; /* Balancing undercurrent in cell 9 */
		uint16_t  UC_10: 1; /* Balancing undercurrent in cell 10 */
		uint16_t  UC_11: 1; /* Balancing undercurrent in cell 11 */
		uint16_t  RES:   4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_BAL_DIAG_UC_Register_t;
/* Configuration   */
typedef union TLE9012_CONFIG_Register_u
{
	struct TLE9012_CONFIG_Register_s
	{
		uint16_t  IBCB_NODE_ID:  1; /* Address (ID) of the node, distributed during enumeration */
		uint16_t  RES_0:         3; /* Reserved for future use */
		uint16_t  EN_ALL_ADC:    1; /* Enable all ADCs */
		uint16_t  RES_1:         1; /* Reserved for future use */
		uint16_t  FN:            1; /* Final Node */
		uint16_t  RES_2:         4; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_CONFIG_Register_t;
/* General purpose input / output */
typedef union TLE9012_GPIO_Register_u
{
	struct TLE9012_GPIO_Register_s
	{
		uint16_t  IN_GPIO0:   1; /* GPIO 0 input state (ignored if communication over GPIO pins) */
		uint16_t  OUT_GPIO0:  1; /* GPIO 0 output setting (ignored if communication over GPIO pins) */
		uint16_t  DIR_GPIO0:  1; /* GPIO 0 direction (ignored if communication over GPIO pins) */
		uint16_t  IN_GPIO1:   1; /* GPIO 1 input state (ignored if communication over GPIO pins) */
		uint16_t  OUT_GPIO1:  1; /* GPIO 1 output setting (ignored if communication over GPIO pins) */
		uint16_t  DIR_GPIO1:  1; /* GPIO 1 direction (ignored if communication over GPIO pins) */
		uint16_t  IN_PWM0:    1; /* PWM 0 input state */
		uint16_t  PWM_PWM0:   1; /* PWM 0 enable PWM function */
		uint16_t  OUT_PWM0:   1; /* PWM 0 output setting */
		uint16_t  DIR_PWM0:   1; /* PWM 0 direction */
		uint16_t  IN_PWM1:    1; /* PWM 1 input state */
		uint16_t  PWM_PWM1:   1; /* PWM 1 enable PWM function */
		uint16_t  OUT_PWM1:   1; /* PWM 1 output setting */
		uint16_t  DIR_PWM1:   1; /* PWM 1 direction */
		uint16_t  RES:        1; /* Reserved for future use */
		uint16_t  VIO_UV:     1; /* VIO undervoltage error (supplied in sleep) */
	} B;
	uint16_t U16;

} TLE9012_GPIO_Register_t;

/* PWM settings */
typedef union TLE9012_GPIO_PWM_Register_u
{
	struct TLE9012_GPIO_PWM_Register_s
	{
		uint16_t  PWM_DUTY_CYCLE:  5; /* Address (ID) of the node, distributed during enumeration */
		uint16_t  RES_0:           3; /* Reserved for future use */
		uint16_t  PWM_PERIOD:      5; /* Enable all ADCs */
		uint16_t  RES_1:           3; /* Reserved for future use */
	} B;
	uint16_t U16;

} TLE9012_GPIO_PWM_Register_t;

/* IC version and manufacturing ID */
typedef union TLE9012_ICVID_Register_u
{
	struct TLE9012_ICVID_Register_s
	{
		uint16_t  VERSION_ID:       8; /* Version ID */
		uint16_t  MANUFACTURER_ID:  8; /* Manufacturer ID */
	} B;
	uint16_t U16;

} TLE9012_ICVID_Register_t;

/* Mailbox register */
typedef union TLE9012_MAILBOX_Register_u
{
	struct TLE9012_MAILBOX_Register_s
	{
		uint16_t  DATA:  16; /* Data storage register */
	} B;
	uint16_t U16;

} TLE9012_MAILBOX_Register_t;

/* Customer ID */
typedef union TLE9012_CUSTOMER_ID_Register_u
{
	struct TLE9012_CUSTOMER_ID_Register_s
	{
		uint16_t  DATA:  16; /* Reserved for potential unique ID */
	} B;
	uint16_t U16;

} TLE9012_CUSTOMER_ID_Register_t;

/* Watchdog counter */
typedef union TLE9012_WDOG_CNT_Register_u
{
	struct TLE9012_WDOG_CNT_Register_s
	{
		uint16_t  WD_CNT:  1; /* Enable cell monitoring for cell 0 */
		uint16_t  MAIN_CNT:  1; /* Enable cell monitoring for cell 0 */
	} B;
	uint16_t U16;

} TLE9012_WDOG_CNT_Register_t;


typedef enum TLE9012_Register_Index_tag
{
	RESERVED_0          = 0x00,
	PART_CONFIG 		= 0x01, /* Partitioning config (supplied in sleep) */
	OL_OV_THR 			= 0x02, /* Cell voltage thresholds (supplied in sleep) */
	OL_UV_THR 			= 0x03, /* Cell voltage thresholds (supplied in sleep) */
	TEMP_CONF 			= 0x04, /* Temperature measurement configuration (supplied in sleep) */
	INT_OT_WARN_CONF 	= 0x05, /* Internal temperature measurement configuration (supplied in sleep)*/
	RR_ERR_CNT 			= 0x08, /* Round Robin ERR counters (supplied in sleep) */
	RR_CONFIG 			= 0x09, /* Round Robin configuration (supplied in sleep) */
	FAULT_MASK 			= 0x0A, /* ERR pin / EMM mask (supplied in sleep) */
	GEN_DIAG 			= 0x0B, /* General diagnosis (supplied in sleep) */
	CELL_UV 			= 0x0C, /* Cell voltage supervision warning flags UV (supplied in sleep)*/
	CELL_OV 			= 0x0D, /* Cell voltage supervision warning flags OV(supplied in sleep) */
	EXT_TEMP_DIAG 		= 0x0E, /* External overtemperature warning flags (supplied in sleep) */
	DIAG_OL 			= 0x10, /* Diagnosis OPENLOAD (supplied in sleep) */
	REG_CRC_ERR 		= 0x11, /* REG_CRC_ERR (supplied in sleep) */
	OP_MODE 			= 0x14, /* Operation mode */
	BAL_CURR_THR 		= 0x15, /* Balancing current thresholds */
	BAL_SETTINGS 		= 0x16, /* Balance settings */
	AVM_CONFIG 			= 0x17, /* Auxiliary Voltage Measurement Configuration */
	MEAS_CTRL 			= 0x18, /* Measurement control */
	CVM_0 				= 0x19, /* Cell voltage measurement 0 */
	CVM_1 				= 0x1A, /* Cell voltage measurement 1 */
	CVM_2 				= 0x1B, /* Cell voltage measurement 2 */
	CVM_3 				= 0x1C, /* Cell voltage measurement 3 */
	CVM_4 				= 0x1D, /* Cell voltage measurement 4 */
	CVM_5 				= 0x1E, /* Cell voltage measurement 5 */
	CVM_6 				= 0x1F, /* Cell voltage measurement 6 */
	CVM_7 				= 0x20, /* Cell voltage measurement 7 */
	CVM_8 				= 0x21, /* Cell voltage measurement 8 */
	CVM_9 				= 0x22, /* Cell voltage measurement 9 */
	CVM_10 				= 0x23, /* Cell voltage measurement 10 */
	CVM_11				= 0x24, /* Cell voltage measurement 11 */
	BVM 				= 0x28, /* Block voltage measurement */
	EXT_TEMP_0 			= 0x29, /* Temp result 0 */
	EXT_TEMP_1 			= 0x2A, /* Temp result 1 */
	EXT_TEMP_2			= 0x2B, /* Temp result 2 */
	EXT_TEMP_3 			= 0x2C, /* Temp result 3 */
	EXT_TEMP_4 			= 0x2D, /* Temp result 4 */
	EXT_TEMP_R_DIAG 	= 0x2F, /* Temp result R Diagnose */
	INT_TEMP 			= 0x30, /* Chip temperature */
	MULTI_READ 			= 0x31, /* Multiread command */
	MULTI_READ_CFG 		= 0x32, /* Multiread Configuration */
	BAL_DIAG_OC 		= 0x33, /* Passive bal. diagnosis OVERCURRENT */
	BAL_DIAG_UC 		= 0x34, /* Passive bal. diagnosis UNDERCURRENT */
	CONFIG 				= 0x36, /* Configuration   */
	GPIO 				= 0x37, /* General purpose input / output */
	GPIO_PWM 			= 0x38, /* PWM settings */
	ICVID 				= 0x39, /* IC version and manufacturing ID */
	MAILBOX 			= 0x3A, /* Mailbox register */
	CUSTOMER_ID_0 		= 0x3B, /* Customer ID */
	CUSTOMER_ID_1 		= 0x3C, /* Customer ID */
	WDOG_CNT 			= 0x3D  /* Watchdog counter */

}TLE9012_Register_Index_t;


typedef union TLE9012_Register_u
{
	struct  TLE9012_Register_s
	{
		uint16_t							reserved_0;
		TLE9012_PART_CONFIG_Register_t		PART_CONFIG_Reg;
		TLE9012_OL_OV_THR_Register_t		OL_OV_THR_Reg;
		TLE9012_OL_UV_THR_Register_t		OL_UV_THR_Reg;
		TLE9012_TEMP_CONF_Register_t		TEMP_CONF_Reg;
		TLE9012_INT_OT_WARN_CONF_Register_t	INT_OT_WARN_CONF_Reg;
		uint16_t							reserved_1[2];
		TLE9012_RR_ERR_CNT_Register_t		RR_ERR_CNT_Reg;
		TLE9012_RR_CONFIG_Register_t		RR_CONFIG_Reg;
		TLE9012_FAULT_MASK_Register_t		FAULT_MASK_Reg;
		TLE9012_GEN_DIAG_Register_t			GEN_DIAG_Reg;
		TLE9012_CELL_UV_Register_t			CELL_UV_Reg;
		TLE9012_CELL_OV_Register_t			CELL_OV_Reg;
		TLE9012_EXT_TEMP_DIAG_Register_t	EXT_TEMP_DIAG_Reg;
		uint16_t							reserved_2;
		TLE9012_DIAG_OL_Register_t			DIAG_OL_Reg;
		TLE9012_REG_CRC_ERR_Register_t		REG_CRC_ERR_Reg;
		uint16_t							reserved_3[2];
		TLE9012_OP_MODE_Register_t			OP_MODE_Reg;
		TLE9012_BAL_CURR_THR_Register_t		BAL_CURR_THR_Reg;
		TLE9012_BAL_SETTINGS_Register_t		BAL_SETTINGS_Reg;
		TLE9012_AVM_CONFIG_Register_t		AVM_CONFIG_Reg;
		TLE9012_MEAS_CTRL_Register_t		MEAS_CTRL_Reg;
		TLE9012_CVM_Register_t				CVM_Reg[12];
		uint16_t							reserved_4[3];
		TLE9012_BVM_Register_t				BVM_Reg;
		TLE9012_EXT_TEMP_Register_t			EXT_TEMP_Reg[5];
		uint16_t							reserved_5;
		TLE9012_EXT_TEMP_R_DIAG_Register_t	EXT_TEMP_R_DIAG_Reg;
		TLE9012_INT_TEMP_Register_t			INT_TEMP_Reg;
		TLE9012_MULTI_READ_Register_t		MULTI_READ_Reg;
		TLE9012_MULTI_READ_CFG_Register_t	MULTI_READ_CFG_Reg;
		TLE9012_BAL_DIAG_OC_Register_t		BAL_DIAG_OC_Reg;
		TLE9012_BAL_DIAG_UC_Register_t		BAL_DIAG_UC_Reg;
		uint16_t							reserved_6;
		TLE9012_CONFIG_Register_t			CONFIG_Reg;
		TLE9012_GPIO_Register_t				GPIO_Reg;
		TLE9012_GPIO_PWM_Register_t			GPIO_PWM_Reg;
		TLE9012_ICVID_Register_t			ICVID_Reg;
		TLE9012_MAILBOX_Register_t			MAILBOX_Reg;
		TLE9012_CUSTOMER_ID_Register_t		CUSTOMER_ID_Reg;
		TLE9012_WDOG_CNT_Register_t 		WDOG_CNT_Reg;
	} Reg;
	uint16_t U16[WDOG_CNT+1];
} TLE9012_Register_t;

#endif /* TLE9012_TLE9012_REG_H_ */


