/*
 * eeprom.h
 *
 *  Created on: Jan 24, 2024
 *      Author: rgeng
 */

#ifndef BSP_EEPROM_INC_EEPROM_H_
#define BSP_EEPROM_INC_EEPROM_H_

#include "isouart.h"
#include "tle9012.h"
/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

extern boolean eeprom_set_ip( const char *ip_str );
extern boolean eeprom_set_gateway( const char *gw_str );
extern boolean eeprom_set_netmask( const char *netmask_str );
extern boolean eeprom_set_mac( const char *mac_str );
extern boolean eeprom_set_dhcp( const char *dhcp_str );
extern boolean eeprom_read_config( void );
extern boolean eeprom_set_isouartNetwork( ISOUART_NetArch_t network );
extern boolean eeprom_set_tle9012_resolution( TLE9012_BITWIDTH_t bitWidth );
#endif /* BSP_EEPROM_INC_EEPROM_H_ */
