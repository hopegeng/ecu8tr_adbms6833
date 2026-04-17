/*
 * eth_help.c
 *
 *  Created on: Jan 2024
 *      Author: rgeng
 */

/*----- Includes -------------------------------------------------------------*/
/* C includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* IFX includes */
#include "Platform_Types.h"
/* Application */
#include <GETH_Help.h>


uint16 swap_u16( uint16 u16 )
{
	return( ((u16 >> 8)&0xFF) | ((u16 &0xFF)<<8) );
}

uint32 swap_u32( uint32 u32 )
{
	return( ((u32 >> 24)&0xFF) | ((u32>>8)&0xFF00) | ((u32<<24)&0xFF000000) | ((u32<<8)&0xFF0000) );
}

