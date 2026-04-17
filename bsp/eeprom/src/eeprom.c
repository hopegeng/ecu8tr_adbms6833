/*
 * eeprom.c
 *
 *  Created on: Jan 24, 2024
 *      Author: rgeng
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <Ifx_Types.h>
#include <IfxFlash.h>
#include <IfxCpu.h>

#include "shell.h"
#include "eeprom.h"
#include "tools.h"
#include "tle9012.h"

#define FLASH_MODULE                0                           /* Macro to select the flash (PMU) module           */
#define DATA_FLASH_0                IfxFlash_FlashType_D0       /* Define the Data Flash Bank to be used            */

#define DFLASH_NUM_SECTORS          1                           /* Number of DFLASH sectors to be erased            */
#define DFLASH_PAGE_LENGTH          IFXFLASH_DFLASH_PAGE_LENGTH /* 0x8 = 8 Bytes (smallest unit that can be */
#define DFLASH_STARTING_ADDRESS     0xAF000000                  /* Address of the DFLASH where the data is written  */
#define DFLASH_NUM_PAGE_TO_FLASH    5                           /* Number of pages to flash in the DFLASH           */

#define MEM(address)                *((uint32 *)(address))      /* Macro to simplify the access to a memory address */


#define DFLASH_PAGE_SIZE_IN_WORD (DFLASH_PAGE_LENGTH/4)

const uint32 p_MAC_ADDRESS = 0xAF000000;
const uint32 p_IP_ADDRESS = 0xAF000000 + DFLASH_PAGE_LENGTH;
const uint32 p_NETMASK = 0xAF000000 + DFLASH_PAGE_LENGTH + DFLASH_PAGE_LENGTH;
const uint32 p_GATEWAY = 0xAF000000 + DFLASH_PAGE_LENGTH + DFLASH_PAGE_LENGTH + DFLASH_PAGE_LENGTH;
const uint32 p_CRC = 0xAF000000 + DFLASH_PAGE_LENGTH + DFLASH_PAGE_LENGTH + DFLASH_PAGE_LENGTH + DFLASH_PAGE_LENGTH;


static void writeDataFlash( uint32 data_address, uint32* p_data, uint32 data_len );
static void verifyDataFlash(void);     /* Function that verifies the data written in the Data Flash memory                 */
static boolean ip_str_2_ip_addr( const char *ip_str, uint8 ip_addr[4] );
static boolean mac_str_2_mac_addr( const char *mac_str, uint8 mac_addr[6] );


//#pragma section farbss "user_test_bss"
uint8_t network_def_config[5][8] __attribute__ ((section("NC_cpu0_dlmu"))) =
{
		{ 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00, 0x00 },		//MAC
		{ 192, 168, 1, 200, 0, 0, 0, 0 },		//IP & DHCP flag, the nework_def_config[1][4] is the DHCP flag, == 0: DHCP is disabled, == 1: DHCP is enabled. nework_def_config[1][5]: isouart network: 1 = Highside; 0 = lowSide
												//nework_def_config[1][5]: isouart network: 1 = Highside; 0 = lowSide
												//nework_def_config[1][6]: 10bits or 16bits TLE9012 ADC resolution: 0 = 16 bit, 1 = 10bit
		{ 255, 255, 255, 0, 0, 0, 0, 0 },		//Net Mask
		{ 192, 168, 1, 1, 0, 0, 0, 0 },			//Gateway
		{ 0, 0, 0, 0, 0, 0, 0, 0 },				//CRC
};



static void writeDataFlash( uint32 data_address, uint32* p_data, uint32 data_len )
{
    uint32 page;                                                /* Variable to cycle over all the pages             */

    /* --------------- ERASE PROCESS --------------- */
    /* Get the current password of the Safety WatchDog module */
    uint16 endInitSafetyPassword = IfxScuWdt_getSafetyWatchdogPassword();

    /* Erase the sector */
    IfxScuWdt_clearSafetyEndinit( endInitSafetyPassword );        /* Disable EndInit protection                       */
    IfxFlash_eraseMultipleSectors( data_address, DFLASH_NUM_SECTORS ); /* Erase the given sector           */
    IfxScuWdt_setSafetyEndinit( endInitSafetyPassword );          /* Enable EndInit protection                        */

    /* Wait until the sector is erased */
    IfxFlash_waitUnbusy( FLASH_MODULE, DATA_FLASH_0 );

    /* --------------- WRITE PROCESS --------------- */
    for(page = 0; page < DFLASH_NUM_PAGE_TO_FLASH; page++)      /* Loop over all the pages                          */
    {
        uint32 pageAddr = DFLASH_STARTING_ADDRESS + (page * DFLASH_PAGE_LENGTH); /* Get the address of the page     */

        /* Enter in page mode */
        IfxFlash_enterPageMode(pageAddr);

        /* Wait until page mode is entered */
        IfxFlash_waitUnbusy(FLASH_MODULE, DATA_FLASH_0);

        /* Load data to be written in the page */
        IfxFlash_loadPage2X32(pageAddr, p_data[2*page], p_data[2*page+1]); /* Load two words of 32 bits each            */

        /* Write the loaded page */
        IfxScuWdt_clearSafetyEndinit(endInitSafetyPassword);    /* Disable EndInit protection                       */
        IfxFlash_writePage(pageAddr);                           /* Write the page                                   */
        IfxScuWdt_setSafetyEndinit(endInitSafetyPassword);      /* Enable EndInit protection                        */

        /* Wait until the data is written in the Data Flash memory */
        IfxFlash_waitUnbusy(FLASH_MODULE, DATA_FLASH_0);
    }
}

/* This function verifies if the data has been correctly written in the Data Flash */
static void verifyDataFlash()
{
    uint32 page;                                                /* Variable to cycle over all the pages             */
    uint32 offset;                                              /* Variable to cycle over all the words in a page   */
    uint32 errors = 0;                                          /* Variable to keep record of the errors            */

    /* Verify the written data */
    for(page = 0; page < DFLASH_NUM_PAGE_TO_FLASH; page++)                          /* Loop over all the pages      */
    {
        uint32 pageAddr = DFLASH_STARTING_ADDRESS + (page * DFLASH_PAGE_LENGTH);    /* Get the address of the page  */

        for(offset = 0; offset < DFLASH_PAGE_LENGTH; offset += 0x4)                 /* Loop over the page length    */
        {
            /* Check if the data in the Data Flash is correct */
            if(MEM(pageAddr + offset) != 0xaa55)
            {
                /* If not, count the found errors */
                errors++;
            }
        }
    }

    /* If the data is correct, turn on the LED2 */
    if(errors == 0)
    {
    	//Error handling
    }
}


static void update_network_2_flash( void )
{
	uint32 crc;
	uint32 *p_data = (uint32*)network_def_config;
	uint32* p_crc = (uint32*)network_def_config[4];

	crc = crc32( (uint8*)network_def_config, 32 );
	p_crc[0] = p_crc[1] = crc;
	writeDataFlash( p_MAC_ADDRESS, p_data, 10 );		//10 uint32

}


boolean eeprom_read_config( void )
{
	uint32 crc;
	uint32 *p_data = (uint32*)network_def_config;
	uint32* p_crc_flash = (uint32*)p_CRC;

	crc = crc32( (uint8*)p_MAC_ADDRESS, 32 );

	if( crc != p_crc_flash[0] )
	{
		uint32* pCrc = (uint32*)network_def_config[4];
		pCrc[0] = pCrc[1] = crc;
		PRINTF( "\r\nFailed on CRC: read = 0x%x, calculated = %x\r\n", *p_crc_flash, crc );
		writeDataFlash( p_MAC_ADDRESS, p_data, 10 );		//10 uint32
		return FALSE;
	}
	else
	{
		uint8* p_network = (uint8*)p_MAC_ADDRESS;
		for( int i = 0; i < 6; i++ )
		{
			network_def_config[0][i] = p_network[i];
		}
		for( int i = 0; i < 4; i++ )
		{
			network_def_config[1][i] = p_network[i+8];		//ip
			network_def_config[2][i] = p_network[i+2*8];
			network_def_config[3][i] = p_network[i+3*8];
		}
		network_def_config[1][4] = p_network[8+4];		//DHCP
		network_def_config[1][5] = p_network[8+5];		//ISOUART NETWORK
		network_def_config[1][6] = p_network[8+6];		//TLE9012 ADC resolution
	}

	isouart_apply_network( (ISOUART_NetArch_t)network_def_config[1][5] );
	tle9012_appyBitWidth( (TLE9012_BITWIDTH_t)network_def_config[1][6] );

	return TRUE;
}

boolean eeprom_set_dhcp( const char *dhcp_str )
{
	if( TRUE == strcmp("1", dhcp_str) == 0 )
	{
		network_def_config[1][4] = 1;
		update_network_2_flash();
		return TRUE;
	}
	else
	{
		network_def_config[1][4] = 0;
		update_network_2_flash();
		return TRUE;
	}

}

boolean eeprom_set_mac( const char *ip_str )
{
	if( TRUE == mac_str_2_mac_addr( ip_str, network_def_config[0] ) )
	{
		update_network_2_flash();
		return TRUE;
	}
	return FALSE;
}
boolean eeprom_set_ip( const char *ip_str )
{
	if( TRUE == convert_ip_to_uint8( ip_str, network_def_config[1] ) )
	{
		update_network_2_flash();
		return TRUE;
	}
	return FALSE;
}

boolean eeprom_set_netmask( const char *ip_str )
{

	if( TRUE == convert_ip_to_uint8( ip_str, network_def_config[2] ) )
	{
		update_network_2_flash();
		return TRUE;
	}
	return FALSE;
}

boolean eeprom_set_gateway( const char *ip_str )
{
	if( TRUE == convert_ip_to_uint8( ip_str, network_def_config[3] ) )
	{
		update_network_2_flash();
		return TRUE;
	}
	return FALSE;
}



boolean eeprom_set_isouartNetwork( ISOUART_NetArch_t network )
{
	if( network >= ISOUART_NET_NONE )
	{
		return FALSE;
	}

	network_def_config[1][5] = (uint8)network;
	update_network_2_flash();

	return TRUE;
}

/* By default it is 16bit resolution, because
 * network_def_config[1][6] = 0: 16 bit
 * network_def_config[1][6] = 1: 10 bit
 */
boolean eeprom_set_tle9012_resolution( TLE9012_BITWIDTH_t bitWidth )
{

	network_def_config[1][6] = (uint8)bitWidth;
	update_network_2_flash();

	return TRUE;
}
