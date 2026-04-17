/*
 * board.h
 *
 *  Created on: Jan 25, 2024
 *      Author: rgeng
 */

#ifndef BSP_BOARD_INC_BOARD_H_
#define BSP_BOARD_INC_BOARD_H_

#include <IfxPort_reg.h>

#define DOUT_LOGIC_HI	(1)
#define DOUT_LOGIC_LO	(0)

/* PORT20 */
#define BMS_DOUT_ERRQ_EXT(x) 	MODULE_P20.OUT.B.P3 = (x ? 1:0)
#define BMS_DOUT_ERRQ_RES(x) 	MODULE_P20.OUT.B.P6 = (x ? 1:0)
#define BMS_DOUT_NSLEEP(x) 		MODULE_P20.OUT.B.P10 = (x ? 1:0)

typedef enum SysRstType_e
{
	eSysSystem,
	eSysApplication,

	eSysRstUnknown
} SysRstType_t;


extern void board_reset(SysRstType_t type);
extern void gpio_init( void );

#endif /* BSP_BOARD_INC_BOARD_H_ */
