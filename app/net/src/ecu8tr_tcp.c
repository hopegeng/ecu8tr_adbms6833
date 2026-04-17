/*
 * ecu8_tcp.c
 *
 *  Created on: Jan 26, 2024
 *      Author: rgeng
 */

#include <bsp/inc/ecu8tr_global.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
/* IFX includes */
#include <Ifx_Types.h>

/* FreeRTOS includes */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <tcp.h>
#include <tcpip.h>
#include <udp.h>


#include "shell.h"
#include "tools.h"
#include "eeprom.h"
#include "ecu8tr_cmd.h"
#include "tle9012.h"
#include "isouart.h"
#include "ecu8tr_version.h"
#include "isouart.h"
#include "board.h"
#include "qspi0mstr_illd.h"
#include "adbms6830.h"
#include "adbmsCommon.h"
#include "adbms6830_reg.h"
#include "ecu8tr_tcp.h"
#include "adbms6830SM.h"

#define ECU8_TCP_CMD_SERVER_PORT  				8000
#define ECU8_TCP_DATA_SERVER_PORT  				8001
#define NO_READ_ERRORS 							0x3Fu
#define NO_STAT_ERRORS							0x1Fu
#define NO_STAT_VOLTAGE_ERRORS					0x03u
#define ADBMS6830_NO_OF_TCPDATA_ELEMENTS_SENT	(1+ADBMS6830_NO_OF_CELLS)	//(ITMP + 16 cell voltages)

#define CMD_DELIMITER ( 0x2424 )			//"$$"

typedef enum
{
	ECU8TR_CMD_SUCCESS = 0,
	ECU8TR_CMD_FAILURE = 1,
	ECU8TR_CMD_NONE = 2,
} ECU8TR_CMD_RET_e;

extern uint8_t network_def_config[5][8];

extern boolean isouart_setNetwork( ISOUART_NetArch_t network );

__private1 ECU8TR_CMD_t ecu8tr_cmd = {0};
static struct tcp_pcb *pcb_cmd_server = NULL;
static struct tcp_pcb *pcb_data_server = NULL;
static struct tcp_pcb *pcb_data_client = NULL;
static struct tcp_pcb *pcb_cmd_client = NULL;

static ip_addr_t remote_ip;

static volatile boolean is_data_sent = TRUE;
static volatile boolean is_adbms6830_data_sent = TRUE;
static volatile ECU8TR_TLE9012_State_t tle9012_state __attribute__ ((section("NC_cpu0_dlmu"))) = ECU8TR_TLE9012_IDLE;
static volatile ECU8TR_ADBMS6830_State_t adbms6830_state __attribute__ ((section("NC_cpu0_dlmu"))) = ECU8TR_ADBMS6830_NONE;
static boolean tle9012_sleep_result = FALSE;
boolean tle9012_wakeup_result = FALSE;
boolean tle9012_commsFailure = FALSE;
static uint8 tle9012_node_for_streaming = 1;
static boolean tle9012_is_sleep = TRUE;
//static uint8 adbms6830_cell_voltages[16] = {{0}};
//static uint8 adbms6830_command[7] = {7, 33, 0, 0, 55, 3, 27};
//__private0 BATTERY_INFO_t packInfo;
__private0 CELL_INFO_t cellInfo;
//__private0 BMS_FLAGS_t bmsControl;
volatile QspiCs_t CS = eQspiHwCsNone;
volatile uint64 adbms6830_msg_cntr = 1;
//TickType_t timeSinceStart = 0;
//float secondsSinceStart = 0;
ADBMS6830_FUELCELL_INFO_t adbms6830Info_temp = {0};
volatile QspiCs_t reserveCS = 0;
volatile uint8_t openWireDetect = 0;
uint8_t dataWriteBuff[6];
uint8_t adbms6830_missing_nodes=0;
uint8_t incorrect_numOfNodes = 0;

/**
 ******************************************************************************
 *
 * \author 	AA
 * \date   	Nov2024
 * \brief	Function: CS_get()
 * 		This function retrieves the current active CS
 *
 * \retval	None.
 *****************************************************************************/
extern QspiCs_t CS_get(void)
{
	return CS;
}

/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Jan2021
 * \brief	Function: bmsControl_getCellMetrics()\n
 * 		This function retrieves the Cell metrics from the ADBMS6830 and stores it
 * 		into the adbms6830Info structure
 *
 *
 * \retval	None.
 *****************************************************************************/
static void bmsControl_getCellMetrics(void)
{
	uint8_t noErrorCV = 0;
	uint8_t noErrorACV = 0;
//	uint8_t noErrorFCV = 0;

	noErrorCV  = adbms6830_ReadCVA();
	noErrorCV |= adbms6830_ReadCVB();
	noErrorCV |= adbms6830_ReadCVC();
	noErrorCV |= adbms6830_ReadCVD();
	noErrorCV |= adbms6830_ReadCVE();
	noErrorCV |= adbms6830_ReadCVF();


	noErrorACV  = adbms6830_ReadACVA();
	noErrorACV |= adbms6830_ReadACVB();
	noErrorACV |= adbms6830_ReadACVC();
	noErrorACV |= adbms6830_ReadACVD();
	noErrorACV |= adbms6830_ReadACVE();
	noErrorACV |= adbms6830_ReadACVF();

//	noErrorFCV  = adbms6830_ReadFCA();
//	noErrorFCV |= adbms6830_ReadFCB();
//	noErrorFCV |= adbms6830_ReadFCC();
//	noErrorFCV |= adbms6830_ReadFCD();
//	noErrorFCV |= adbms6830_ReadFCE();
//	noErrorFCV |= adbms6830_ReadFCF();

	if (NO_READ_ERRORS != noErrorCV)
	{
		dbgPRINT("Error received in reading the cell voltages: %x", noErrorCV);
	}

	if (NO_READ_ERRORS != noErrorACV)
	{
		dbgPRINT("Error received in reading the averaged cell voltages");
	}

//	if (NO_READ_ERRORS != noErrorFCV)
//	{
//		dbgPRINT("Error received in reading the filtered cell voltages");
//	}
}


/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Jan2021
 * \brief	Function: bmsControl_loadCellInfo()\n
 *			This function converts the ADC results from the retrieved cell voltages
 *			and converts them into voltages in mV
 *
 * \retval	None.
 *****************************************************************************/

static void bmsControl_loadCellInfo(void)
{
	for (uint8_t i=0; i<ADBMS6830_NUMBER_OF_NODES; i++)
	{
		//Calculate Cell Voltage
		cellInfo.cellV[i][0] = ((int16_t)adbms6830Info.rawData[i].Cell.C1V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][1] = ((int16_t)adbms6830Info.rawData[i].Cell.C2V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][2] = ((int16_t)adbms6830Info.rawData[i].Cell.C3V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][3] = ((int16_t)adbms6830Info.rawData[i].Cell.C4V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][4] = ((int16_t)adbms6830Info.rawData[i].Cell.C5V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][5] = ((int16_t)adbms6830Info.rawData[i].Cell.C6V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][6] = ((int16_t)adbms6830Info.rawData[i].Cell.C7V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][7] = ((int16_t)adbms6830Info.rawData[i].Cell.C8V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][8] = ((int16_t)adbms6830Info.rawData[i].Cell.C9V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][9] = ((int16_t)adbms6830Info.rawData[i].Cell.C10V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][10] = ((int16_t)adbms6830Info.rawData[i].Cell.C11V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][11] = ((int16_t)adbms6830Info.rawData[i].Cell.C12V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][12] = ((int16_t)adbms6830Info.rawData[i].Cell.C13V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][13] = ((int16_t)adbms6830Info.rawData[i].Cell.C14V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][14] = ((int16_t)adbms6830Info.rawData[i].Cell.C15V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.cellV[i][15] = ((int16_t)adbms6830Info.rawData[i].Cell.C16V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;

		//Calculate Average Cell voltage
		cellInfo.avgCellV[i][0] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC1V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][1] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC2V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][2] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC3V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][3] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC4V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][4] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC5V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][5] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC6V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][6] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC7V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][7] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC8V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][8] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC9V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][9] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC10V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][10] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC11V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][11] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC12V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][12] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC13V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][13] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC14V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][14] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC15V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
		cellInfo.avgCellV[i][15] = ((int16_t)adbms6830Info.rawData[i].AvCell.AC16V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;

		//Calculate Average Cell voltage
//		cellInfo.filtCellV[i][0] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC1V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][1] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC2V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][2] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC3V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][3] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC4V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][4] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC5V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][5] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC6V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][6] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC7V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][7] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC8V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][8] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC9V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][9] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC10V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][10] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC11V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][11] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC12V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][12] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC13V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][13] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC14V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][14] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC15V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;
//		cellInfo.filtCellV[i][15] = ((int16_t)adbms6830Info.rawData[i].FiltCell.FC16V * ADBMS6830_CELL_V_LSB/1000) + ADBMS6830_CELL_CONVERSION_FACTOR;

		cellInfo.packV[i] = 0; // clear before totaling
		for (int8_t j=0; j<ADBMS6830_NO_OF_CELLS; j++)
		{
			cellInfo.packV[i] += cellInfo.cellV[i][j];
		}
	}
}


/**
 ******************************************************************************
 *
 * \author 	RL
 * \date   	Jan2021
 * \brief	Function: bmsControl_getStatusVoltages()\n
 *			This function retrieves the Aux voltages from the status registers
 *
 * \retval	None.
 *  *****************************************************************************/
static void bmsControl_getStatusVoltages(void)
{
	uint16_t calcTemp = 0;

	for (uint8_t i = 0; i< ADBMS6830_NUMBER_OF_NODES; i++)
	{
		uint8_t noStatErrors = 0;

		noStatErrors = adbms6830_ReadStatA();
		noStatErrors |= adbms6830_ReadStatB();


		if (NO_STAT_VOLTAGE_ERRORS != noStatErrors)
		{
			dbgPRINT("Error received in reading the status registers: %x", noStatErrors);
		}
		else
		{
			calcTemp = ((adbms6830Info.rawData[i].Status_Voltages.ITMP * (uint16_t)ADBMS6830_CELL_V_LSB) / ADBMS6830_VOLT_SCALING) + (uint16_t)ADBMS6830_CELL_CONVERSION_FACTOR;
			calcTemp = (calcTemp * ADBMS6830_TEMP_SCALING / ADBMS6830_MVPERDEGREEC) - ADBMS6830_TEMP_KELVIN;

			cellInfo.statusV[i].Vref2 = ((adbms6830Info.rawData[i].Status_Voltages.VREF2 * (uint16_t)ADBMS6830_CELL_V_LSB) / ADBMS6830_VOLT_SCALING) + (uint16_t)ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.statusV[i].Itmp = calcTemp;
			cellInfo.statusV[i].Vd = ((adbms6830Info.rawData[i].Status_Voltages.VD * (uint16_t)ADBMS6830_CELL_V_LSB) / ADBMS6830_VOLT_SCALING) + (uint16_t)ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.statusV[i].Va = ((adbms6830Info.rawData[i].Status_Voltages.VA * (uint16_t)ADBMS6830_CELL_V_LSB) / ADBMS6830_VOLT_SCALING) + (uint16_t)ADBMS6830_CELL_CONVERSION_FACTOR;
			cellInfo.statusV[i].Vres = ((adbms6830Info.rawData[i].Status_Voltages.VRES * (uint16_t)ADBMS6830_CELL_V_LSB) / ADBMS6830_VOLT_SCALING) + (uint16_t)ADBMS6830_CELL_CONVERSION_FACTOR;
		}
	}
}

//static void ecu8tr_tcpDataClientTask( void *arg );

static void  ecu8tr_dataErr(void *arg, err_t err)
{
	PRINTF( "The TCP/IP error on data connection: %d\r\n", err );
}

static err_t ecu8tr_dataSent( void *arg, struct tcp_pcb *tpcb, uint16 len )
{
	////PRINTF( "Data connection done sending\r\n" );
	is_data_sent = TRUE;
	return ERR_OK;

}

static err_t ecu8tr_dataRecv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    // Echo data back
    if( p != NULL )
    {
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
    }
    else if( err == ERR_OK )
    {
    	PRINTF( "Data connection closed\r\n" );
        // Close connection
        pcb_data_client = NULL;
        tcp_close(tpcb);

    }

    return ERR_OK;

}

static err_t ecu8tr_dataAccept( void *arg, struct tcp_pcb *new_pcb, err_t err )
{
	//tcp_write( new_pcb, ECU8_LOGO, strlen(ECU8_LOGO), 1 );
	PRINTF( "Received a new data connection\r\n" );
    tcp_recv( new_pcb, ecu8tr_dataRecv );
    tcp_err( new_pcb, ecu8tr_dataErr );
    tcp_sent( new_pcb, ecu8tr_dataSent );

    ////remote_ip = new_pcb->remote_ip;

    pcb_data_client = new_pcb;

    if( tle9012_state == ECU8TR_TLE9012_WAKEUP_DONE )
    {
    	tle9012_state = ECU8TR_TLE9012_DATA_STREAMING;
    }

    return ERR_OK;
}

static uint16 build_cmd_return( ECU8TR_CMD_e cmd, ECU8TR_CMD_RET_e ret, void *buffer )
{
	uint32 i;
	size_t len;

	len = strlen( &((const char*)(buffer))[8] );
	i = htonl( cmd );
	memmove( buffer, (const void*)&i, 4 );
	i = htonl( ret );
	memmove( &((uint8*)buffer)[4], (const void*)&i, 4 );

	return( len+8 );
}

uint8_t current_code = 200;
static void process_tcp_request( struct tcp_pcb *tpcb, uint8 *p_data, uint16 data_len )
{
	static uint8 pos = 0;
	char buffer[256];
	int buffer_pos = 0;
	ECU8TR_CMD_RET_e ret;
	uint16 ret_len = 0;
	uint8 *p_req;

	p_req = (uint8*)( ((uint8*)&ecu8tr_cmd) + pos );

	if( pos == 0 )
	{
		memset(ecu8tr_cmd.cmd_body, 0, CMD_BODY_SIZE );
	}

	//sanity check
	//PRINTF( "Receved commad len = %d, ECU8TR_CMD_t size = %d, pos = %d\r\n", data_len, sizeof(ECU8TR_CMD_t), pos );
	if( (pos+data_len) > sizeof(ECU8TR_CMD_t) )
	{
		pos = 0;
		return;
	}

	memcpy( p_req, p_data, data_len );
	pos += data_len;

	if( pos != sizeof(ECU8TR_CMD_t) )
	{
		return;
	}

	pos = 0;

	if( ecu8tr_cmd.delimiter != CMD_DELIMITER )
	{
		return;
	}

	ecu8tr_cmd.cmd_code = ntohl( ecu8tr_cmd.cmd_code );
	PRINTF( "Received command = %d\r\n", ecu8tr_cmd.cmd_code );

	if(ecu8tr_cmd.cmd_code > CMD_NONE)
	{
		PRINTF( "Incorrect command received = %d !\r\n", ecu8tr_cmd.cmd_code );
		return;
	}
	else
	{
		current_code = ecu8tr_cmd.cmd_code;
	}

	switch( ecu8tr_cmd.cmd_code )
	{
		case CMD_ADBMS6830_WAKEUP:
			PRINTF( "Received command ADBMS6830 WAKEUP\r\n" );
			if(adbms6830Info.isInitialized == true)
			{
				adbms6830_state = ECU8TR_TLE9012_WAKEUP;
			}
			else
			{
				PRINTF("Error: ADBMS6830 initialization failed!\r\n");
				sprintf( buffer+8, "%s\r\n$$", (adbms6830_state==ECU8TR_ADBMS6830_WAKEUP)? "Wakeup" : "Sleep" );
			}
			break;

		case CMD_ADBMS6830_SLEEP:
			PRINTF( "Received command ADBMS6830 SLEEP\r\n" );
			adbms6830_state = ECU8TR_TLE9012_SLEEP;
			break;

		case CMD_ADBMS6830_RUNNING_MODE:
			sprintf( buffer+8, "%s\r\n$$", (adbms6830_state==ECU8TR_ADBMS6830_WAKEUP)? "Wakeup" : "Sleep" );
			ret_len = build_cmd_return( CMD_ADBMS6830_RUNNING_MODE, ECU8TR_CMD_SUCCESS, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;

		case CMD_NET_SET_MAC:
		{
			if( TRUE == eeprom_set_mac( ecu8tr_cmd.cmd_body ) )
			{
				sprintf( buffer+8, "set mac to %s successfully\r\n$$", ecu8tr_cmd.cmd_body  );
				ret = ECU8TR_CMD_SUCCESS;
			}
			else
			{
				sprintf( buffer+8, "failed to set mac address\r\n$$" );
				ret = ECU8TR_CMD_SUCCESS;
			}

			ret_len = build_cmd_return( CMD_NET_SET_MAC, ret, buffer );

			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
		}
			break;

		case CMD_NET_SHOW_NETWORK:

			if( network_def_config[1][4] == 1 )
			{
				buffer_pos = sprintf( buffer+8, "DHCP is enabled\r\n" );
			}
			else
			{
				buffer_pos = sprintf( buffer+8, "DHCP is disabled\r\n" );
			}
			//tcp_write( tpcb, buffer, strlen(buffer), 1 );

			buffer_pos += sprintf( buffer+buffer_pos+8, "mac address = %02x:%02x:%02x:%02x:%02x:%02x\r\n",
						network_def_config[0][0],network_def_config[0][1], network_def_config[0][2], network_def_config[0][3], network_def_config[0][4], network_def_config[0][5] );

			buffer_pos += sprintf( buffer+buffer_pos+8, "ip address = %02d.%02d.%02d.%02d\r\n",
						network_def_config[1][0],network_def_config[1][1], network_def_config[1][2], network_def_config[1][3] );

			buffer_pos += sprintf( buffer+buffer_pos+8, "netmask = %02d.%02d.%02d.%02d\r\n",
						network_def_config[2][0],network_def_config[2][1], network_def_config[2][2], network_def_config[2][3] );

			sprintf( buffer+buffer_pos+8, "gateway = %02d.%02d.%02d.%02d\r\n$$",
						network_def_config[3][0],network_def_config[3][1], network_def_config[3][2], network_def_config[3][3] );
			ret_len = build_cmd_return( CMD_NET_SHOW_NETWORK, ECU8TR_CMD_SUCCESS, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;

		case CMD_NET_SET_IP:
			if( eeprom_set_ip( ecu8tr_cmd.cmd_body ) == TRUE )
			{
				sprintf( buffer+8, "set ip to %s successfully\r\n$$", ecu8tr_cmd.cmd_body );
				ret = ECU8TR_CMD_SUCCESS;
			}
			else
			{
				sprintf( buffer+8, "Failed to set the ip address\r\n$$" );
				ret = ECU8TR_CMD_FAILURE;
			}
			ret_len = build_cmd_return( CMD_NET_SET_IP, ret, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );

			break;

		case CMD_NET_SET_NETMASK:
			if( eeprom_set_netmask( ecu8tr_cmd.cmd_body ) == TRUE )
			{
				sprintf( buffer+8, "set netmask to %s successfully\r\n$$", ecu8tr_cmd.cmd_body );
				ret = ECU8TR_CMD_SUCCESS;
			}
			else
			{
				sprintf( buffer+8, "Failed to set the ip address\r\n$$" );
				ret = ECU8TR_CMD_FAILURE;
			}
			ret_len = build_cmd_return( CMD_NET_SET_NETMASK, ret, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );

			break;

		case CMD_NET_SET_GATEWAY:
			if( eeprom_set_gateway( ecu8tr_cmd.cmd_body ) == TRUE )
			{
				sprintf( buffer+8, "set gateway to %s successfully\r\n$$", ecu8tr_cmd.cmd_body );
				ret = ECU8TR_CMD_SUCCESS;
			}
			else
			{
				sprintf( buffer+8, "Failed to set the ip address\r\n$$" );
				ret = ECU8TR_CMD_FAILURE;
			}
			ret_len = build_cmd_return( CMD_NET_SET_GATEWAY, ret, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );

			break;

		case CMD_TLE9012_SLEEP:
			//Here we don't do anything, because if the data is streaming,then we have to wait it to exit from the streaming state7
//			int pos;

			if (tle9012_state == ECU8TR_TLE9012_IDLE)
			{
				ret = ECU8TR_CMD_SUCCESS;
				sprintf( buffer+8, "The TLE9012 nodes are already in sleep mode\r\n$$" );

				ret_len = build_cmd_return( CMD_TLE9012_WAKEUP, ret, buffer );
				tcp_write( tpcb, buffer, ret_len, 1 );
				tcp_output( tpcb );
			}
			else
			{
				tle9012_state = ECU8TR_TLE9012_SLEEP;
			}

			break;


		case CMD_TLE9012_WAKEUP:
		{
			int pos;

			if( tle9012_wakeup_result != TRUE )
			{
				tle9012_state = ECU8TR_TLE9012_WAKEUP;
				while( (tle9012_state != ECU8TR_TLE9012_WAKEUP_DONE) && (tle9012_state != ECU8TR_TLE9012_IDLE) && (tle9012_state == ECU8TR_TLE9012_WAKEUP))
				{
					vTaskDelay(pdMS_TO_TICKS(5) );
				}
				if( TRUE == tle9012_wakeup_result )
				{
					tle9012_is_sleep = FALSE;
					pos = sprintf( buffer+8, "The tle9012 was put in the operation mode successfully\r\n" );
					sprintf( buffer + pos+8, "Total %d tle9012 nodes found on the network with ring structure = %s\r\n$$", tle9012_getNodeNr(), (isouart_getRing())?"Yes" : "No" );
					ret = ECU8TR_CMD_SUCCESS;
					if( pcb_data_client != NULL )
					{
						tle9012_state = ECU8TR_TLE9012_DATA_STREAMING;
					}
				}
				else
				{
					sprintf( buffer+8, "Failed to put the tle9012 into the operation mode\r\n$$" );
					ret = ECU8TR_CMD_FAILURE;
				}
			}
			else if (TRUE == tle9012_isWakeup())// Need to read ID from chip to verify that it is in wakeup and that comms have not failed
			{
				pos = sprintf( buffer+8, "The tle9012 is already in operation mode \r\n" );
				sprintf( buffer + pos+8, "Total %d tle9012 nodes found on the network with ring structure = %s\r\n$$", tle9012_getNodeNr(), (isouart_getRing())?"Yes" : "No" );
				ret = ECU8TR_CMD_SUCCESS;
			}
			else // comms have failed
			{
				pos = sprintf( buffer+8, "isoUart communication has failed \r\n" );
				sprintf( buffer+ pos+8, "Attempting to put the tle9012 into the operation mode\r\n$$" );
				ret = ECU8TR_CMD_FAILURE;
				tle9012_wakeup_result = FALSE;
				tle9012_state = ECU8TR_TLE9012_WAKEUP;
			}
			ret_len = build_cmd_return( CMD_TLE9012_WAKEUP, ret, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;
		}
#if 0
		case CMD_TLE9012_READ_REG:
		{
			int dev, reg;
			boolean parse_result;


			PRINTF( "tle9012_read_reg:%s\r\n", ecu8tr_cmd.cmd_body );
			parse_result = parse_tle9012_read_params( ecu8tr_cmd.cmd_body, &dev, &reg );
			PRINTF( "tle9012_read_reg: %d, 0x%x\r\n", dev, reg );
			if( parse_result == FALSE )
			{
				sprintf( buffer, "Failed to parse the parameters %s\r\n$$", ecu8tr_cmd.cmd_body );
				tcp_write( tpcb, buffer, strlen(buffer), 1 );
				tcp_output( tpcb );
			}
			ret = tle9012_rdReg( dev, reg, (uint8*)buffer );
			sprintf( buffer+ret, "\r\n$$" );
			tcp_write( tpcb, buffer, ret+4, 1 );
			tcp_output( tpcb );
		}
			break;

		case CMD_TLE9012_WRITE_REG:
		{
			int dev, reg, val;
			boolean parse_result;

			PRINTF( "tle9012_write_reg:%s\r\n", ecu8tr_cmd.cmd_body );
			parse_result = parse_tle9012_write_params( ecu8tr_cmd.cmd_body, &dev, &reg , &val );
			if( parse_result == FALSE )
			{
				sprintf( buffer, "Failed to parse the parameters %s\r\n$$", ecu8tr_cmd.cmd_body );
				tcp_write( tpcb, buffer, strlen(buffer), 1 );
				tcp_output( tpcb );
			}
			ret = tle9012_wrReg( dev, reg, val, (uint8*)buffer );
			if( buffer[0] == ISOUART_COMM_SUCCESS )
			{
				sprintf( buffer, "Write successfully\r\n$$" );
			}
			else
			{
				sprintf( buffer, "Write failed\r\n$$" );
			}
			tcp_write( tpcb, buffer, strlen(buffer), 1 );
			tcp_output( tpcb );

		}
			break;
#endif

		case CMD_TLE9012_SET_NETWORK:
		{
			boolean parse_result;
			int network;

			sprintf( buffer+8, "Failed to set the network\r\n$$" );
			ret = ECU8TR_CMD_FAILURE;

			parse_result = parse_tle9012_network_params( ecu8tr_cmd.cmd_body, &network );
			if( parse_result == TRUE )
			{
				if( isouart_setNetwork( network ) == TRUE )
				{
					tle9012_state = ECU8TR_TLE9012_IDLE;
					sprintf( buffer+8, "Network set successfully\r\n$$" );
					ret = ECU8TR_CMD_SUCCESS;

				}
			}
			ret_len = build_cmd_return( CMD_TLE9012_SET_NETWORK, ret, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
		}
			break;

		case CMD_ECU8TR_VERSION:
			sprintf( buffer+8, "v%d.%d.%d\r\n$$", ECU8TR_MAJOR, ECU8TR_MINIOR, ECU8TR_PATCH );
			ret_len = build_cmd_return( CMD_ECU8TR_VERSION, ECU8TR_CMD_SUCCESS, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;

		case CMD_TLE9012_GET_NETWORK:
			sprintf( buffer+8, (isouart_get_network())? "HighSide\r\n$$" : "LowSide\r\n$$" );
			ret_len = build_cmd_return( CMD_TLE9012_GET_NETWORK, ECU8TR_CMD_SUCCESS, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;

		case CMD_TLE9012_RUNNING_MODE:
			sprintf( buffer+8, "%s\r\n$$", (tle9012_is_sleep==FALSE)? "Wakeup" : "Sleep" );
			ret_len = build_cmd_return( CMD_TLE9012_RUNNING_MODE, ECU8TR_CMD_SUCCESS, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;

		case CMD_TLE9012_SET_BITWIDTH:
		{
			boolean parse_result;
			int bitWidth;

			sprintf( buffer+8, "Failed to set the TLE9012 bit width\r\n$$" );
			ret = ECU8TR_CMD_FAILURE;

			parse_result = parse_tle9012_bitwidth_params( ecu8tr_cmd.cmd_body, &bitWidth );
			if( parse_result == TRUE )
			{
				if( tle9012_setBitWidth( bitWidth ) == TRUE )
				{
					//tle9012_state = ECU8TR_TLE9012_IDLE;
					sprintf( buffer+8, "TLE9012 bit width set successfully\r\n$$" );
					ret = ECU8TR_CMD_SUCCESS;
				}
			}
			ret_len = build_cmd_return( CMD_TLE9012_SET_BITWIDTH, ret, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
		}
			break;

		case CMD_TLE9012_GET_BITWIDTH:
			sprintf( buffer+8, (tle9012_getBitWidth()==TLE9012_BITWIDTH_16)? "Bit Width 16bit\r\n$$" : "Bit Width 10bit\r\n$$" );//1 = 10 bits, 0 = 16 bits
			ret_len = build_cmd_return( CMD_TLE9012_GET_BITWIDTH, ECU8TR_CMD_SUCCESS, buffer );
			tcp_write( tpcb, buffer, ret_len, 1 );
			tcp_output( tpcb );
			break;

		case CMD_RESET:
		{
			board_reset( eSysSystem );
			break;
		}
		default:
			PRINTF( "Received unknown command\r\n" );
			break;
	}

}

static err_t ecu8tr_cmdSent( void *arg, struct tcp_pcb *tpcb, uint16 len )
{
	PRINTF( "Command connection done sending\r\n" );

	return ERR_OK;
}

static err_t ecu8tr_cmdRecv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    // Echo data back
    if( p != NULL )
    {
    	process_tcp_request( tpcb, p->payload, p->len );
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
    }
    else if( err == ERR_OK )
    {
    	pcb_cmd_client = NULL;
    	PRINTF( "Command connection closed\r\n" );
        // Close connection
        tcp_close(tpcb);
    }


    return ERR_OK;

}

static void  ecu8tr_cmdErr(void *arg, err_t err)
{
	PRINTF( "The TCP/IP error on command connection: %d\r\n", err );
}

static err_t ecu8tr_cmdAccept( void *arg, struct tcp_pcb *new_pcb, err_t err )
{
	//tcp_write( new_pcb, ECU8_LOGO, strlen(ECU8_LOGO), 1 );
	PRINTF( "Received a new command connection\r\n" );
	pcb_cmd_client = new_pcb;
    tcp_recv( new_pcb, ecu8tr_cmdRecv );
    tcp_err( new_pcb, ecu8tr_cmdErr );
    tcp_sent( new_pcb, ecu8tr_cmdSent );
    remote_ip = new_pcb->remote_ip;

    return ERR_OK;
}


static void ecu8tr_startCmdServer( void )
{
    pcb_cmd_server = tcp_new();
    tcp_bind( pcb_cmd_server, IP_ADDR_ANY, ECU8_TCP_CMD_SERVER_PORT );
    pcb_cmd_server = tcp_listen( pcb_cmd_server );
    tcp_accept( pcb_cmd_server, ecu8tr_cmdAccept );
}

static void ecu8tr_startDataServer( void )
{
    pcb_data_server = tcp_new();
    tcp_bind( pcb_data_server, IP_ADDR_ANY, ECU8_TCP_DATA_SERVER_PORT );
    pcb_data_server = tcp_listen( pcb_data_server );
    tcp_accept( pcb_data_server, ecu8tr_dataAccept );
}


static void ecu8tr_tcpServerTask( void *arg )
{
    vTaskDelay( pdMS_TO_TICKS(3000) ); // Yield to other tasks

    ecu8tr_startCmdServer();
    ecu8tr_startDataServer();

    while (1)
    {
        vTaskDelay( pdMS_TO_TICKS(1000) ); // Yield to other tasks
    }
}

/**
 ******************************************************************************
 *
 * \author 	AA
 * \date   	Nov2024
 * \brief	Function: adbms6830_reverse_rawData_array()
 *			This function reverses the order of the data read.
 *			This is used to maintain the node number when using daisy chain
 *
 * \retval	None.
 *  *****************************************************************************/
static void adbms6830_reverse_rawData_array(ADBMS6830_FUELCELL_INFO_t *adbmsData)
{
	RAW_DATA_t tempVar;
	for(uint8 i=0; i<ADBMS6830_NUMBER_OF_NODES/2; ++i)
	{
		tempVar = adbmsData->rawData[i];
		adbmsData->rawData[i] = adbmsData->rawData[ADBMS6830_NUMBER_OF_NODES - i - 1];
		adbmsData->rawData[ADBMS6830_NUMBER_OF_NODES - i - 1] = tempVar;
	}
}

/**
 ******************************************************************************
 *
 * \author 	AA
 * \date   	Nov2024
 * \brief	Function: adbms6830_switch_CS()
 *			This function switches the active chip select between 2 and 6
 *
 * \retval	None.
 *  *****************************************************************************/
static void adbms6830_switch_CS(void)
{
	// switch to the other CS
	if(CS == eQspiHwCs02)
	{
		CS = eQspiHwCs06;
	}
	else if(CS == eQspiHwCs06)
	{
		CS = eQspiHwCs02;
	}
}

/**
 ******************************************************************************
 *
 * \author 	AA
 * \date   	Nov2024
 * \brief	Function: adbms6830_openWire_detect()\n
 *			This function compares the CVs & ITMP values measures from both chip
 *			selects to check for an open wire
 *			This function runs every 1 second
 *
 * \retval	None.
 *  *****************************************************************************/
static void adbms6830_openWire_detect(void)
{
	if(adbms6830_msg_cntr%10 == 0)
	{
		// trigger single measurement for all CVs
		if (false == adbms6830_write(ADBMS6830_ADCV_BASE_REG, dataWriteBuff,0 ))
		{
			dbgPRINT("Failed to start measurements");
		}

		adbms6830_SNAP();		// Freeze registers
		reserveCS = CS;

		// Get data from CS6 channel
		CS = eQspiHwCs06;

		// read all cell voltages
		bmsControl_getCellMetrics();

		// Copy data read into temp buffer
		memcpy(&adbms6830Info_temp, &adbms6830Info, sizeof(adbms6830Info));

		// flip the order of the CVs and ITMPs (opposite order for different CSs)
		adbms6830_reverse_rawData_array(&adbms6830Info_temp);

		// Get data from CS2 channel
		CS = eQspiHwCs02;

		// read all cell voltages
		bmsControl_getCellMetrics();

		for (uint8 i=0; i<ADBMS6830_NUMBER_OF_NODES; ++i)
		{
			// Check that data from both chip selects are the same
			if( adbms6830Info_temp.rawData[i].Cell.C1V  != adbms6830Info.rawData[i].Cell.C1V  || \
				adbms6830Info_temp.rawData[i].Cell.C2V  != adbms6830Info.rawData[i].Cell.C2V  || \
				adbms6830Info_temp.rawData[i].Cell.C3V  != adbms6830Info.rawData[i].Cell.C3V  || \
				adbms6830Info_temp.rawData[i].Cell.C4V  != adbms6830Info.rawData[i].Cell.C4V  || \
				adbms6830Info_temp.rawData[i].Cell.C5V  != adbms6830Info.rawData[i].Cell.C5V  || \
				adbms6830Info_temp.rawData[i].Cell.C6V  != adbms6830Info.rawData[i].Cell.C6V  || \
				adbms6830Info_temp.rawData[i].Cell.C7V  != adbms6830Info.rawData[i].Cell.C7V  || \
				adbms6830Info_temp.rawData[i].Cell.C8V  != adbms6830Info.rawData[i].Cell.C8V  || \
				adbms6830Info_temp.rawData[i].Cell.C9V  != adbms6830Info.rawData[i].Cell.C9V  || \
				adbms6830Info_temp.rawData[i].Cell.C10V != adbms6830Info.rawData[i].Cell.C10V || \
				adbms6830Info_temp.rawData[i].Cell.C11V != adbms6830Info.rawData[i].Cell.C11V || \
				adbms6830Info_temp.rawData[i].Cell.C12V != adbms6830Info.rawData[i].Cell.C12V || \
				adbms6830Info_temp.rawData[i].Cell.C13V != adbms6830Info.rawData[i].Cell.C13V || \
				adbms6830Info_temp.rawData[i].Cell.C14V != adbms6830Info.rawData[i].Cell.C14V || \
				adbms6830Info_temp.rawData[i].Cell.C15V != adbms6830Info.rawData[i].Cell.C15V || \
				adbms6830Info_temp.rawData[i].Cell.C16V != adbms6830Info.rawData[i].Cell.C16V)
			{
				dbgPRINT("ERROR: CS values do not match!");
				//todo: check how much time is needed between prints to avoid overwrite
				vTaskDelay( pdMS_TO_TICKS(10) );
				//check for an open wire
				if( adbms6830Info_temp.rawData[i].Cell.C1V  == 0 || adbms6830Info.rawData[i].Cell.C1V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C2V  == 0 || adbms6830Info.rawData[i].Cell.C2V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C3V  == 0 || adbms6830Info.rawData[i].Cell.C3V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C4V  == 0 || adbms6830Info.rawData[i].Cell.C4V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C5V  == 0 || adbms6830Info.rawData[i].Cell.C5V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C6V  == 0 || adbms6830Info.rawData[i].Cell.C6V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C7V  == 0 || adbms6830Info.rawData[i].Cell.C7V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C8V  == 0 || adbms6830Info.rawData[i].Cell.C8V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C9V  == 0 || adbms6830Info.rawData[i].Cell.C9V  == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C10V == 0 || adbms6830Info.rawData[i].Cell.C10V == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C11V == 0 || adbms6830Info.rawData[i].Cell.C11V == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C12V == 0 || adbms6830Info.rawData[i].Cell.C12V == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C13V == 0 || adbms6830Info.rawData[i].Cell.C13V == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C14V == 0 || adbms6830Info.rawData[i].Cell.C14V == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C15V == 0 || adbms6830Info.rawData[i].Cell.C15V == 0 || \
					adbms6830Info_temp.rawData[i].Cell.C16V == 0 || adbms6830Info.rawData[i].Cell.C16V == 0)
				{
					// if cell voltage = 0, there is an open wire
					dbgPRINT("ERROR: Open Wire Detected!");
					openWireDetect = 1;
					break;
				}
				else
				{
					// if CS values dont match but no value is 0, wrong number of nodes connected
					dbgPRINT("ERROR: Incorrect Number of Nodes!");
					openWireDetect = 0;
					incorrect_numOfNodes = 1;
				}
			}
			else
			{
				openWireDetect = 0;
			}
		}

		CS = reserveCS;			// Return CS back to original value
		adbms6830_UNSNAP();		// Unfreeze registers
	}
}

uint16_t streamingTimeOut = 0;
static uint8_t streamingTimeOutLimit = 0;

static void ecu8tr_TLE9012Task( void *arg )
{
	CS = eQspiHwCs02;
	ECU8TR_DATA_t data;
	ECU8TR_ADBMS6830_DATA_t adbms6830_data;
	uint8  nodeNr = 0;
	uint32 crc;
	uint32 crc_AD;
	uint8 tx_buff[1];
	uint8 rx_buff[1] = {0};
	uint16 ret=0;

	/* Wake up isoSPI */
	tx_buff[0] = 0x0;

//	static uint16_t streamingTimeOut = 0;

	while( adbms6830Info.isInitialized == false )
	{
		if (false == adbms6830Info.isInitialized)
		{
			// try initializing adbms6830 with CS=2
			if (true == adbms6830_Config())
			{
				adbms6830Info.isInitialized = true;
				dbgPRINT("ADBMS6830 Initialized Successfully!\r\n");
			}
			else
			{
				// try initializing adbms6830 with CS=6
				CS = eQspiHwCs06;
				if (true == adbms6830_Config())
				{
					adbms6830Info.isInitialized = true;
					dbgPRINT("ADBMS6830 Initialized Successfully!\r\n");
				}
				else
				{
					dbgPRINT("Error: ADBMS6830 was not initialized successfully\r\n");
				}
			}
		}
		if( adbms6830Info.isInitialized == false )
		{
			PRINTF( "Failed to initialize the ADBMS6830, try again\r\n" );
			vTaskDelay( 1000 );
		}
	}

	data.delimiter = CMD_DELIMITER;
	adbms6830_data.delimiter = CMD_DELIMITER;

	adbms6830_state = ECU8TR_ADBMS6830_WAKEUP;

	while( 1 )
	{
		//timeSinceStart = xTaskGetTickCount();
		//secondsSinceStart = (float) timeSinceStart / configTICK_RATE_HZ;

		switch(adbms6830_state)
		{
		case ECU8TR_ADBMS6830_WAKEUP:
			adbms6830_missing_nodes=0;
			if (adbms6830Info.isInitialized == true)
			{
				//Reset data buffer
				//memcpy(&adbms6830Info, 0, sizeof(adbms6830Info));

				// check for an open wire
				adbms6830_openWire_detect();

				// if open wire detected, concatenate results
				if (openWireDetect == 1)
				{

					// get CVs and ITMP from one CS
					if (false == adbms6830_write(ADBMS6830_ADCV_BASE_REG, dataWriteBuff,0 ))
					{
						dbgPRINT("Failed to start measurements");
						continue;
					}
					bmsControl_getCellMetrics();
					bmsControl_loadCellInfo();
					if (true == adbms6830_TrigAuxMeasurement())
					{
						bmsControl_getStatusVoltages();
					}

					// Copy data read into temp buffer
					memcpy(&adbms6830Info_temp, &adbms6830Info, sizeof(adbms6830Info));

					// flip the order of the CVs and ITMPs (opposite order for different CSs)
					adbms6830_reverse_rawData_array(&adbms6830Info_temp);

					// switch to the other CS
					adbms6830_switch_CS();

					// get CVs and ITMP from other CS
					if (false == adbms6830_write(ADBMS6830_ADCV_BASE_REG, dataWriteBuff,0 ))
					{
						dbgPRINT("Failed to start measurements");
						continue;
					}
					bmsControl_getCellMetrics();
					bmsControl_loadCellInfo();
					if (true == adbms6830_TrigAuxMeasurement())
					{
						bmsControl_getStatusVoltages();
					}

					// copy over missing register values into main struct
					for (uint8 i=0; i<ADBMS6830_NUMBER_OF_NODES; ++i)
					{
						if(adbms6830Info.rawData[i].Cell.C1V   == 0 ||\
							adbms6830Info.rawData[i].Cell.C2V  == 0|| \
							adbms6830Info.rawData[i].Cell.C3V  == 0|| \
							adbms6830Info.rawData[i].Cell.C4V  == 0|| \
							adbms6830Info.rawData[i].Cell.C5V  == 0|| \
							adbms6830Info.rawData[i].Cell.C6V  == 0|| \
							adbms6830Info.rawData[i].Cell.C7V  == 0|| \
							adbms6830Info.rawData[i].Cell.C8V  == 0|| \
							adbms6830Info.rawData[i].Cell.C9V  == 0|| \
							adbms6830Info.rawData[i].Cell.C10V == 0 || \
							adbms6830Info.rawData[i].Cell.C11V == 0 || \
							adbms6830Info.rawData[i].Cell.C12V == 0 || \
							adbms6830Info.rawData[i].Cell.C13V == 0 || \
							adbms6830Info.rawData[i].Cell.C14V == 0 || \
							adbms6830Info.rawData[i].Cell.C15V == 0 || \
							adbms6830Info.rawData[i].Cell.C16V == 0)
						{
							adbms6830Info.rawData[i]=adbms6830Info_temp.rawData[i];
						}
					}

					// checking for missing nodes
					for (uint8 i=0; i<ADBMS6830_NUMBER_OF_NODES; ++i)
					{
						if(adbms6830Info.rawData[i].Cell.C1V   == 0 ||\
							adbms6830Info.rawData[i].Cell.C2V  == 0|| \
							adbms6830Info.rawData[i].Cell.C3V  == 0|| \
							adbms6830Info.rawData[i].Cell.C4V  == 0|| \
							adbms6830Info.rawData[i].Cell.C5V  == 0|| \
							adbms6830Info.rawData[i].Cell.C6V  == 0|| \
							adbms6830Info.rawData[i].Cell.C7V  == 0|| \
							adbms6830Info.rawData[i].Cell.C8V  == 0|| \
							adbms6830Info.rawData[i].Cell.C9V  == 0|| \
							adbms6830Info.rawData[i].Cell.C10V == 0 || \
							adbms6830Info.rawData[i].Cell.C11V == 0 || \
							adbms6830Info.rawData[i].Cell.C12V == 0 || \
							adbms6830Info.rawData[i].Cell.C13V == 0 || \
							adbms6830Info.rawData[i].Cell.C14V == 0 || \
							adbms6830Info.rawData[i].Cell.C15V == 0 || \
							adbms6830Info.rawData[i].Cell.C16V == 0)
						{
							adbms6830_missing_nodes++;
						}
					}
					dbgPRINT("ADBMS6830 Missing Nodes = %d", adbms6830_missing_nodes);

					// switch back to the original CS
					adbms6830_switch_CS();

				}
				else	// no open wire detected
				{
					// Switch between CS2 and CS6 every cycle, to ensure valid data on both channels
					if(adbms6830_msg_cntr%2 == 0)
					{
						CS = eQspiHwCs06;
					}
#if 0
					else if(adbms6830_msg_cntr%2 == 1)
					{
						CS = eQspiHwCs02;
					}
#endif
					// Request Single Read
					if (false == adbms6830_write(ADBMS6830_ADCV_BASE_REG, dataWriteBuff,0 ))
					{
						dbgPRINT("Failed to start measurements");
						vTaskDelay( 1000 );
						continue;
					}
					bmsControl_getCellMetrics();
					bmsControl_loadCellInfo();
					if (true == adbms6830_TrigAuxMeasurement())
					{
						bmsControl_getStatusVoltages();
					}

					if(CS == eQspiHwCs06)
					{
						// flip the order of the CVs and ITMPs (opposite order for different CSs)
						adbms6830_reverse_rawData_array(&adbms6830Info);
					}

				}
			}

			// Setup TCP data to be sent
			for (uint8 i = 0; i < ADBMS6830_NUMBER_OF_NODES; ++i)
			{
			    adbms6830_data.dev[i] = i;

			    // Total body count for all nodes (moved outside inner loop - it's constant)
			    adbms6830_data.body_cnt = ADBMS6830_NUMBER_OF_NODES * ADBMS6830_NO_OF_TCPDATA_ELEMENTS_SENT;

			    // Base index for this node's data block
			    uint8 base = i * ADBMS6830_NO_OF_TCPDATA_ELEMENTS_SENT;

			    // Array of all 17 values we want to send (16 cells + ITMP)
			    uint16_t values[17] = {
			        adbms6830Info.rawData[i].Cell.C1V,
			        adbms6830Info.rawData[i].Cell.C2V,
			        adbms6830Info.rawData[i].Cell.C3V,
			        adbms6830Info.rawData[i].Cell.C4V,
			        adbms6830Info.rawData[i].Cell.C5V,
			        adbms6830Info.rawData[i].Cell.C6V,
			        adbms6830Info.rawData[i].Cell.C7V,
			        adbms6830Info.rawData[i].Cell.C8V,
			        adbms6830Info.rawData[i].Cell.C9V,
			        adbms6830Info.rawData[i].Cell.C10V,
			        adbms6830Info.rawData[i].Cell.C11V,
			        adbms6830Info.rawData[i].Cell.C12V,
			        adbms6830Info.rawData[i].Cell.C13V,
			        adbms6830Info.rawData[i].Cell.C14V,
			        adbms6830Info.rawData[i].Cell.C15V,
			        adbms6830Info.rawData[i].Cell.C16V,
			        adbms6830Info.rawData[i].Status_Voltages.ITMP
			    };

			    // Fill the data_body for this node
			    for (uint8 j = 0; j < 17; ++j)
			    {
			        uint16_t ret = values[j];

			        adbms6830_data.data_body[base + j].reg = j + 1;           // reg = 1..17
			        adbms6830_data.data_body[base + j].reg_value = (ret << 8) | (ret >> 8);  // byte swap

			        // Optional: keep the print only for debugging (or remove it)
			        PRINTF("Node %d, Reg %d = 0x%04X\r\n", i, j+1, ret);
			    }
			}

			// Calculate CRC
			crc_AD = crc32( (const uint8*)&adbms6830_data, sizeof(ECU8TR_ADBMS6830_DATA_t) - 6 );
			adbms6830_data.crc = htonl( crc_AD );

			// Send TCP data
			if( pcb_data_client != NULL )
			{
				tcp_write( pcb_data_client, (const void*)&adbms6830_data, sizeof(ECU8TR_ADBMS6830_DATA_t), 1 );
				tcp_output( pcb_data_client );
			}

			// Run loop every 100ms
			if((adbms6830_msg_cntr)%10 == 0)
			{
				vTaskDelay(pdMS_TO_TICKS(6));			// Valid data check takes more time so wait only 6ms
			}
			else
			{
				vTaskDelay(pdMS_TO_TICKS(62));			// code execution time = 38ms
			}

			adbms6830_msg_cntr++;
			if(adbms6830_msg_cntr >= UINT64_MAX)		// Reset counter if too big
			{
				adbms6830_msg_cntr=5;					// reset to 0 since max value is ....615
			}
			break;

		case ECU8TR_ADBMS6830_SLEEP:
			//PRINTF("ADBMS6830 State = SLEEP\r\n");
			vTaskDelay( pdMS_TO_TICKS(50) );
			break;

		default:
			vTaskDelay(pdMS_TO_TICKS(50));
			break;
		}
	}
}


ECU8TR_TLE9012_State_t ecu8tr_getTLE9012State( void )
{
	return tle9012_state;
}

void ecu8_tcpServerInit( void )
{
    xTaskCreate( ecu8tr_tcpServerTask, "TCPServer", configMINIMAL_STACK_SIZE, NULL, 0, NULL);
    xTaskCreate( ecu8tr_TLE9012Task, "TCPClient", 2*configMINIMAL_STACK_SIZE, NULL, 0, NULL);
}


/*********************** Test Version *******************************/
#if 0

#define BUFFER_SIZE 256
#define PAYLOAD (BUFFER_SIZE-4)
uint8 buffer[BUFFER_SIZE];
static void ecu8tr_tcpDataClientTask( void *arg )
{
	uint32 crc;
	uint32 *p_crc;

	p_crc = (uint32*)(&buffer[PAYLOAD]);

	for( int i = 0; i < PAYLOAD; i++ )
	{
		buffer[i] = 'a';
	}

	crc = crc32( buffer, PAYLOAD );
	*p_crc = htonl( crc );


    vTaskDelay( pdMS_TO_TICKS(3000) ); // Yield to other tasks

    while (1)
    {
    	if( is_tle9012_wakeup == TRUE && pcb_data_client != NULL && is_data_sent == TRUE )
    	{
    		is_data_sent = FALSE;
    		tcp_write( pcb_data_client, buffer, BUFFER_SIZE, 1 );
    		tcp_output( pcb_data_client );
    	}
        vTaskDelay( pdMS_TO_TICKS(4) ); // Yield to other tasks
    }
}
#endif



