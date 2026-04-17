/*
 * tools.h
 *
 *  Created on: Feb 3, 2024
 *      Author: rgeng
 */

#ifndef APP_INC_TOOLS_H_
#define APP_INC_TOOLS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>
#include <Stm/Std/IfxStm.h>


static void delayMillSecond( uint32 delay )
{
	float32 fre = IfxStm_getFrequency( &MODULE_STM0 );
	uint32 ticks = delay * (fre/1000.0);

	IfxStm_waitTicks( &MODULE_STM0, ticks );
}


static boolean convert_ip_to_uint8( const char* ip_str, uint8 ip_bytes[4] )
{
    int parts[4];
    int num_converted;

    num_converted = sscanf(ip_str, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]);

    if (num_converted == 4)
    {
        for (int i = 0; i < 4; i++)
        {
            if (parts[i] >= 0 && parts[i] <= 255)
            {
                ip_bytes[i] = (uint8)parts[i]; // Assign converted part to output array
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    return false;
}


static boolean mac_str_2_mac_addr( const char *mac_str, uint8 mac_addr[6] )
{
	int fields = sscanf( mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
	                        &mac_addr[0], &mac_addr[1], &mac_addr[2],
	                        &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    return fields == 6;
}


static uint32 crc32(const uint8 *data, size_t length)
{
    uint32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        uint8 byte = data[i];
        crc = crc ^ byte;

        for (uint8 j = 0; j < 8; j++)
        {
            uint32 mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}


static void delay_ms( uint32 ms )
{
	if( 1 )
	{
		vTaskDelay( pdMS_TO_TICKS(ms) );
	}
	else
	{
		delayMillSecond( ms );
	}
}

static boolean parse_tle9012_network_params( const char *str, int *pNetwork )
{
    int readCount = 0;
    char *token;
    char buffer[100];


    strncpy(buffer, str, 99);
    buffer[99] = '\0';

    token = strtok(buffer, " ");
    while( token != NULL && readCount < 1 )
    {
        if (strstr(token, "0x") != NULL || strstr(token, "0X") != NULL)
        {
            // Attempt to read as hexadecimal
            readCount += sscanf(token, "%x", pNetwork) == 1 ? 1 : 0;
        }
        else
        {
            // Attempt to read as decimal
            readCount += sscanf(token, "%d", pNetwork) == 1 ? 1 : 0;
        }
        token = strtok(NULL, " "); // Get next token
    }

    // Determine outcome based on readCount
    if( readCount == 0 )
    {
        return FALSE;
    }
	else if( readCount == 1 )
	{
		return TRUE;
    }

    return FALSE;

}

static boolean parse_tle9012_bitwidth_params( const char *str, int *pBitWidth )
{
    int readCount = 0;
    char *token;
    char buffer[100];


    strncpy(buffer, str, 99);
    buffer[99] = '\0';

    token = strtok(buffer, " ");
    while( token != NULL && readCount < 1 )
    {
        if (strstr(token, "0x") != NULL || strstr(token, "0X") != NULL)
        {
            // Attempt to read as hexadecimal
            readCount += sscanf(token, "%x", pBitWidth) == 1 ? 1 : 0;
        }
        else
        {
            // Attempt to read as decimal
            readCount += sscanf(token, "%d", pBitWidth) == 1 ? 1 : 0;
        }
        token = strtok(NULL, " "); // Get next token
    }

    // Determine outcome based on readCount
    if( readCount == 0 )
    {
        return FALSE;
    }
	else if( readCount == 1 )
	{
		return TRUE;
    }

    return FALSE;

}


static boolean parse_tle9012_read_params(const char *str, int *pDev, int *pReg )
{
    int readCount = 0;
    char *token;
    char buffer[100];


    strncpy(buffer, str, 99);
    buffer[99] = '\0';

    token = strtok(buffer, " ");
    while( token != NULL && readCount < 2 )
    {
        if (strstr(token, "0x") != NULL || strstr(token, "0X") != NULL)
        {
            // Attempt to read as hexadecimal
            if (readCount == 0) {
                readCount += sscanf(token, "%x", pDev) == 1 ? 1 : 0;
            }
            else
            {
                readCount += sscanf(token, "%x", pReg) == 1 ? 1 : 0;
            }
        }
        else
        {
            // Attempt to read as decimal
            if (readCount == 0)
            {
                readCount += sscanf(token, "%d", pDev) == 1 ? 1 : 0;
            }
            else
            {
                readCount += sscanf(token, "%d", pReg) == 1 ? 1 : 0;
            }
        }
        token = strtok(NULL, " "); // Get next token
    }

    // Determine outcome based on readCount
    if( readCount == 0 || readCount == 1 )
    {
        return FALSE;
    }
	else if( readCount == 2 )
	{
		return TRUE;
    }

    return FALSE;
}



static boolean parse_tle9012_write_params(const char *str, int *pDev, int *pReg, int *pVal )
{
    int readCount = 0;
    char *token;
    char buffer[100];


    strncpy(buffer, str, 99);
    buffer[99] = '\0';

    token = strtok(buffer, " ");
    while( token != NULL && readCount < 3 )
    {
        if (strstr(token, "0x") != NULL || strstr(token, "0X") != NULL)
        {
            // Attempt to read as hexadecimal
            if (readCount == 0) {
                readCount += sscanf(token, "%x", pDev) == 1 ? 1 : 0;
            }
            else if( readCount == 1 )
            {
                readCount += sscanf(token, "%x", pReg) == 1 ? 1 : 0;
            }
            else if( readCount == 2 )
            {
                readCount += sscanf(token, "%x", pVal) == 1 ? 1 : 0;
            }
        }
        else
        {
            // Attempt to read as decimal
            if (readCount == 0)
            {
                readCount += sscanf(token, "%d", pDev) == 1 ? 1 : 0;
            }
            else if( readCount == 1 )
            {
                readCount += sscanf(token, "%d", pReg) == 1 ? 1 : 0;
            }
            else
            {
                readCount += sscanf(token, "%d", pVal) == 1 ? 1 : 0;
            }
        }
        token = strtok(NULL, " "); // Get next token
    }

    // Determine outcome based on readCount
    if( readCount == 0 || readCount == 1 || readCount == 2)
    {
        return FALSE;
    }
	else if( readCount == 3 )
	{
		return TRUE;
    }

    return FALSE;
}

#endif /* APP_INC_TOOLS_H_ */
