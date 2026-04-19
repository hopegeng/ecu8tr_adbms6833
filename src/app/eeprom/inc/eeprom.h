/*
 * eeprom.h
 *
 *  Created on: Jan 24, 2024
 *      Author: rgeng
 */

#ifndef BSP_EEPROM_INC_EEPROM_H_
#define BSP_EEPROM_INC_EEPROM_H_


/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

extern boolean eeprom_set_ip( const char *ip_str );
extern boolean eeprom_set_gateway( const char *gw_str );
extern boolean eeprom_set_netmask( const char *netmask_str );
extern boolean eeprom_set_mac( const char *mac_str );
extern boolean eeprom_set_dhcp( const char *dhcp_str );
extern boolean eeprom_read_config( void );

#endif /* BSP_EEPROM_INC_EEPROM_H_ */
