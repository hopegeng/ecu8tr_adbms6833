/*
 * qspi_mcal.c
 *
 *  Created on: Mar 17, 2022
 *      Author: rgeng
 */

#include "Spi.h"

#if (__RL_SHELL == 1)
#include "shell.h"
#endif /* (__RL_SHELL == 1) */

#define ISO_SPI0_CS2		(SpiConf_SpiChannel_SpiChannel_0)
#define ISO_SPI1_CS10		(SpiConf_SpiChannel_SpiChannel_1)
#define ISO_SPI2_CS0		(SpiConf_SpiChannel_SpiChannel_2)

//#define LTC2949_CHANNEL		(ISO_SPI2_CS0)		/* We use TC377 BMS board for testing */
#define LTC2949_CHANNEL			(ISO_SPI0_CS2)			/* We use newly designed TC399 BMS board for testing */
#define ADBMS6830_CHANNEL		()

/*******************************************************************************
** Syntax       : uint8 Spi_Synch_Demo(void)                                  **
**                                                                            **
** Service ID   : NA                                                          **
**                                                                            **
** Sync/Async   : Synchronous                                                 **
**                                                                            **
** Reentrancy   : Non Reentrant                                               **
**                                                                            **
** Parameters (in) : none                                                     **
**                                                                            **
** Parameters (out): none                                                     **
**                                                                            **
** Return value    : none                                                     **
**                                                                            **
** Description : This routine will start the SPI configured for sequence ID 0 **
**               Transmit mode     : Synchronous                              **
**               LevelDelivered    : 02                                       **
**               External Devices  : QSPI0                                    **
**               CS Selection      : CS_VIA_PERIPHERAL_ENGINE                 **
**               QSPI0 Pins used   : P20.08(CS-0),P20.13(SCLK),P20.14(MTSR)   **
**                                   P20.12(MRTS)                             **
*******************************************************************************/
volatile __private0 Spi_DataBufferType  SpiWrite_Data_Buffer[20u];
volatile __private0 Spi_DataBufferType  SpiRead_Data_Buffer[20u];
static volatile uint8 QSPI_Transfer_Status = TRUE;

/* Debug area */
static volatile __private0 Spi_SeqResultType ray_seqResult;

#if 0
uint8 Spi_Synch_Demo_IB(void)
{
  volatile uint8 Transmit_Data_incr;
  volatile uint8 Receive_Data_incr;
  QSPI_Transfer_Status = TRUE;

  PRINTF ("\n Internal data buffer transmitted on QSPI2 : \n");

  for(Transmit_Data_incr = 0; Transmit_Data_incr < 20; Transmit_Data_incr++)
  {
	    SpiWrite_Data_Buffer[Transmit_Data_incr] = 0x00;

//    PRINTF ("\n Write IB[%d] : %d \n ", Transmit_Data_incr, SpiWrite_Data_Buffer[Transmit_Data_incr]);
  }
  SpiWrite_Data_Buffer[0] = 0x88;

	#if (SPI_HW_QSPI2_USED == STD_ON)
//	Spi_ControlLoopBack(SPI_QSPI2_INDEX, SPI_LOOPBACK_ENABLE);
	#endif

	#if (SPI_HW_QSPI0_USED == STD_ON)
//	Spi_ControlLoopBack(SPI_QSPI0_INDEX, SPI_LOOPBACK_ENABLE);
	#endif

  Spi_WriteIB(SpiConf_SpiChannel_SpiChannel_0, (uint8 *) &SpiWrite_Data_Buffer[0u]);


  Spi_SyncTransmit(SpiConf_SpiSequence_SpiSequence_0);
  ray_seqResult = Spi_GetSequenceResult(SpiConf_SpiSequence_SpiSequence_0);



  while( ray_seqResult != SPI_SEQ_OK)
  {
	  ray_seqResult = Spi_GetSequenceResult(SpiConf_SpiSequence_SpiSequence_0);
  }
  Spi_ReadIB(SpiConf_SpiChannel_SpiChannel_0, (uint8 *)&SpiRead_Data_Buffer[0u]);

  PRINTF ("\r\n Internal data buffer Received on QSPI2 : \r\n");

  /* Evaluate the Recieved Buffer data */
  for(Receive_Data_incr = 0; Receive_Data_incr < 20; Receive_Data_incr++)
  {
	  PRINTF ("\r\n ReadIB[%d] : %d \r\n ", Receive_Data_incr, SpiRead_Data_Buffer[Receive_Data_incr]);

    if(SpiRead_Data_Buffer[Receive_Data_incr] != SpiWrite_Data_Buffer[Receive_Data_incr])
    {
      QSPI_Transfer_Status = FALSE;
      break;
    }
  }

  return QSPI_Transfer_Status;
}
#endif


uint8 Spi_Synch_Demo_EB(void)
{
	int len = 10;

	for( int idx = 0; idx < len; idx++ )
		SpiWrite_Data_Buffer[idx] = idx+1;

	Spi_SetupEB(SpiConf_SpiChannel_SpiChannel_1, (uint8 *) &SpiWrite_Data_Buffer[0], (uint8 *)&SpiRead_Data_Buffer[0], len );

	Spi_SyncTransmit( SpiConf_SpiSequence_SpiSequence_1 );

	ray_seqResult = Spi_GetSequenceResult(SpiConf_SpiChannel_SpiChannel_1);


	while( ray_seqResult != SPI_SEQ_OK)
	{
	  ray_seqResult = Spi_GetSequenceResult(SpiConf_SpiChannel_SpiChannel_1);
	}

	for( int idx = 0; idx < len; idx++ )
		SpiWrite_Data_Buffer[0] = 0x00;

	for( int idx = 0; idx < len; idx++ )
		PRINTF( "Spi_Synch_Demo_EB: Received data = 0x%x\r\n", SpiRead_Data_Buffer[idx] );

	return QSPI_Transfer_Status;

}


/* Send the data to QSPI0 with CS = 2, corresponding QSPI Channel is SpiConf_SpiChannel_SpiChannel_0 */
int32_t qspimcal_Send( uint16_t u16Length, uint8_t *pu8SrcBuff, uint8_t *pu8DstBuff )
{
	int32_t i32Rtn = 0;
	Spi_SeqResultType status;


	Spi_SetupEB( LTC2949_CHANNEL, (uint8 *) &pu8SrcBuff[0], (uint8 *)&pu8DstBuff[0], u16Length );

	Spi_SyncTransmit( LTC2949_CHANNEL );
	status = Spi_GetSequenceResult( LTC2949_CHANNEL );

	while( status != SPI_SEQ_OK)
	{
		status = Spi_GetSequenceResult( LTC2949_CHANNEL );
	}


	return( i32Rtn );

}

