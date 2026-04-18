/*
 * bootloader.c
 *
 *  Created on: Sep 27, 2021
 *      Author: rgeng
 */

#include "string.h"
/* IFX includes */
#include "Ifx_reg.h"
#include "Cpu/Std/Ifx_Types.h"
#include "_Impl/IfxFlash_cfg.h"
#include "swap.h"
#include "flash.h"
#include "board.h"
#include "tools.h"
#include "shell.h"
#include "ecu8tr_cmd.h"


#define BOOTLOADER_ENABLE        1

static uint8 blState = 0x0u;
static uint32 totalLen = 0x0u;
static uint32 totalBlock = 0x0u;
static uint32 expectedBlock = 0x0u;
static uint32 receivedLen = 0x0u;

static uint32 bankAddr;
static uint32 currentSector;
static uint32 sectorOffset;
static volatile ECU8TR_UPGRADE_STATUS_e upgrade_status __attribute__ ((section("NC_cpu0_dlmu"))) = UPGRADE_STATUS_IDLE;

ECU8TR_UPGRADE_STATUS_e bootloader_get_upgradeStatus( void )
{
	return upgrade_status;
}

void bootloader_set_upgradeStatus( ECU8TR_UPGRADE_STATUS_e status )
{
	upgrade_status = status;
}

boolean bootloader_doFlash( uint8 *pData, uint32 dataLen )
{
	boolean ret = TRUE;


	pfls_write( bankAddr, (uint32*)pData, dataLen/4 );
	//ret = flash_write( bankAddr, pData, dataLen );

	bankAddr += dataLen;

	return( ret );
}

boolean bootloader_doUpGrade( uint8 *pData, uint16 dataLen, uint32 *pBlockSeq )
{
	uint32 crc_field;
	uint32 crc_calculated;
	uint32 block_seq;
	uint32 frame_len;


	*pBlockSeq = (0x0u);
	switch( blState )
	{
	case 0x0u:		/* Start upgrade */
		if( dataLen == 14 && pData[0] == 'N' && pData[1] == 'C' )
		{
			upgrade_status = UPGRADE_STATUS_IN_PROGRESS;
			crc_field = pData[2] << 24 | pData[3] << 16 | pData[4] << 8 | pData[5];
			crc_calculated =	crc32( pData+6,  8 );	/* totalLen + total blocks */
			if( crc_calculated != crc_field )
			{
				PRINTF( "Failed on crc32, crc_calculated = 0x%x\r\n", crc_calculated );
				PRINTF( "received CRC = 0x%x\r\n", crc_field );
				return FALSE;
			}
			totalLen = pData[6] << 24 | pData[7] << 16 | pData[8] << 8 | pData[9];
			totalBlock = pData[10] << 24 | pData[11] << 16 | pData[12] << 8 | pData[13];

			blState = 0x01u;
			expectedBlock = 0x1u;
			receivedLen = 0x0u;

			currentSector = 0x0u;
			sectorOffset = 0x0u;
			if( get_currentBank() == 0x55 )		/* Bank A: PF0, PF1, PF4 */
			{
				bankAddr = IFXFLASH_PFLASH_P2_START;
			}
			else
			{
				bankAddr = IFXFLASH_PFLASH_P0_START;  /* Bank B: PF2, PF3, PF5 */
			}

			pfls_MassErase( bankAddr );
		}
		else
		{
			PRINTF( "Wrong header\r\n" );
			return FALSE;
		}

		break;

	case 0x01u:
		block_seq = pData[0] << 24 | pData[1] << 16 | pData[2] << 8 | pData[3];
		crc_field = pData[4] << 24 | pData[5] << 16 | pData[6] << 8 | pData[7];
		frame_len = dataLen - 8;
		receivedLen = receivedLen + frame_len;

		*pBlockSeq = block_seq;
		crc_calculated = crc32( pData + 8, frame_len );
		if( crc_field != crc_calculated )
		{
			PRINTF( "Failed on crc32, crc_calculated = 0x%x, crc_received = 0x%x\r\n", crc_calculated, crc_field );
			blState = 0x0u;
			return FALSE;
		}
		if( block_seq != expectedBlock )
		{
			PRINTF( "The wrong block sequnce number received.\r\n" );
			blState = 0x0u;
			return FALSE;
		}

#if BOOTLOADER_ENABLE
		if( bootloader_doFlash( pData+8, frame_len ) == FALSE )
		{
			return FALSE;
		}

#endif

		if( block_seq == totalBlock )
		{
			blState = 0x02u;
		}
		else
		{
			expectedBlock += 1;
		}

		break;

	case 0x02u:		/* Done */
		if( memcmp( pData, "NC-DO-SWAP", 10) == 0x0u && totalLen == receivedLen )
		{
			if( get_currentBank() == 0x55 )
			{
#if 1	//2024_02_13
				enable_swap();
				configure_swap( 0xaa );
#endif
			}
			else
			{
#if 1	//2024_02_13
				enable_swap();
				configure_swap( 0x55 );
#endif
			}
			PRINTF( "Successflly received the all packets, now swap memory.\r\n" );
			*pBlockSeq = 0xFFFFFFFF;
		}
		else
		{
			PRINTF( "Failed to receive swap command.\r\n" );
			return FALSE;
		}
		//do_swap;
		//reboot;
		break;
	}

	return TRUE;

}

