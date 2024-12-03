/*
 *  Created on: Nov 13, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO_FATFS
 *  Processor: STM32F405
 *  Program version: 1.0
 *  Source file: SDcard_SDIO_diskio.c
 *  Change history:
 */

/*
 * This is the diskio level of FatFs, running here using SDIO, not SPI.
 * Code is still SDHC only!
 *
 */

#include <SDcard_SDIO_diskio.h> /* Declarations of disk functions */


/*-----------------------------------------------------------------------*/
/* Custom variables                                                      */
/*-----------------------------------------------------------------------*/

  uint16_t Tx_timeout_cnt;  //this will be decreased by an IRQ
  uint16_t Rx_timeout_cnt;  //this will be decreased by an IRQ

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

/*

Just a dummy since we only have one SD card.

*/

DSTATUS disk_status(
  BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  return 0x0;  //disk status is a dummy in our design which should not change the DRSTATUS value
               //only mount_volume in the fatfs function (see ff.cpp) that calls both init and status. It is called BEFORE init so it won't reset the init return answer.
}


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

/*

This is to initialize the card.
Fatfs doesn't care, how it is done. It just wants the card running and a valid status for it:
"0" - all is well
"1" - init error
"2" - no card found

Write protect doesn't work in our card so that answer is discarded.

*/

DSTATUS disk_initialize(
  BYTE pdrv /* Physical drive nmuber to identify the drive */
) {

  uint8_t init_result = SDCard_Card_ID_Mode_w_SDIO();

  SDIO_speed_change();																	//we change the SDIO clocking to 4 MHz from 200 kHz

  SDIO_Select_Card();																	//we select the card

  SDIO_Change_bus_width();																//we change the bus width to 4-wide

  SDIO_DeSelect_Card();																	//we de-select the card
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: certain commands sent by FATfs should be done on a non-selected card, so we de-select the card when not interfacing with it directly

  return init_result;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

/*

This is to send commands.
Currently only 3 commands are recognised: SYNCH, SIZE GET and SECTOR GET. Other commands are not being called by fatfs here - either not used (TRIM) or specifically made for other types of cards (MMC).

*/

DRESULT disk_ioctl(
  BYTE pdrv, /* Physical drive nmuber (0..) */  //we ignore this
  BYTE cmd,                                     /* Control code */
  void *buff                                    /* Buffer to send/receive control data */
) {

  //----
  //Note: with pdrv, we do nothing
  //Note: I removed the power management cmd section. It doesn't seem to be used.
  //Note: the status of the card is only updated within the init function and the status function. As such, after an unsuccesfull init, we can just break the execution completely.
  //The trick is that if we have an extern value storing the state of the card locally too, we can poll that value and potentially skip execution. We can give back error messages upon continous re-engagement for the card
  //----

  DRESULT response = RES_ERROR;  //this is our response byte, resets to R/W error

  switch (cmd) {

    //----SECTOR COUNT command----//

    case GET_SECTOR_COUNT: { 															//if we get the sector count command
                            															//we need to send CMD9 to the card and load the reply into the buff pointer
                            															//we extract the csize from the csd and then project the value to the buff pointer

      SDCard_Card_Data_Mode_CSD_w_SDIO();												//Note: the CSD will be in the RESP[1..4] registers of the SDIO
    																					//		CSD comes through the CMD line and is 128 bits
    																					//		CSD was 152 bits on SPI because we captured the entire response
    																					//		C_size is bits [69:48] in the CSD

																						//CSD example:
																						//	RESP1 = 01000000000011100000000000110010		//[127:96]
																						//	RESP2 = 01000000000000010010110000000000		//[95:64]			((uint32_t) (SDIO->RESP2)) & ((uint32_t)0x3F)
																						//	RESP3 = 11101110000100110111111110000000		//[63:32]			(uint32_t) (SDIO->RESP3 >> 16);
																						//	RESP4 = 00010100100000000000000010011000		//[31:1]0b

      uint32_t capacity = ((((((uint32_t) (SDIO->RESP2)) & (((uint32_t)0x3F))) << 16)) | (uint32_t) ((SDIO->RESP3) >> 16));
      	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	 	//c_size is the number of blocks, not bytes

      *(uint32_t *)buff = (uint32_t)capacity << 10;                                 	//we get the byte number

      response = RES_OK;

    } break;

    //----SECTOR SIZE command----//

    case GET_SECTOR_SIZE:  {//if we get the sector size command, the sector size is hard wired to 512

      *(WORD *)buff = 512;																//SDHC is always 512

      response = RES_OK;

    } break;


    //----SYNC command----//

    case CTRL_SYNC: {

    	//if we conclude all write operations within disk_write, this can be left blank

    } break;

    //----Default----//

    default:

      response = RES_PARERR;  //default value is invalid parameter

  }

  return response;  //we return "all is well" or RES_OK in fatfs speak

}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(
  BYTE pdrv,    /* Physical drive nmuber to identify the drive */
  BYTE *buff,   /* Data buffer to store read data/uint8_t */
  LBA_t sector, /* Start sector in LBA/uint32_t */
  UINT count    /* Number of sectors to read/uint16_t */
) {

  DRESULT response = RES_ERROR;

  uint8_t result;

  SDIO_Select_Card();

	if (count == 1)  {

		/* READ_SINGLE_BLOCK */
		result = SDCard_Card_Data_Mode_Single_Block_Read_w_SDIO(sector, buff);

  } else {

	  	/* READ_MULTI_BLOCK */
	  	result = SDCard_Card_Data_Mode_Multi_Block_Read_w_SDIO(sector, buff, count);

  }

  SDIO_DeSelect_Card();

  if(result == 0) response = RES_OK;			//Note: for a robust solution, this should generate error messages as well

  return response;  //we return "all is well" or RES_OK in fatfs speak
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/


DRESULT disk_write(
  BYTE pdrv,        /* Physical drive number to identify the drive */
  const BYTE *buff, /* Data to be written */
  LBA_t sector,     /* Start sector in LBA */
  UINT count        /* Number of sectors to write */
) {

  DRESULT response = RES_ERROR;

  uint8_t result;

  SDIO_Select_Card();

  if (count == 1)  {

		/* WRITE_SINGLE_BLOCK */
		result = SDCard_Card_Data_Mode_Single_Block_Write_w_SDIO(sector, buff);

  } else {

	    /* WRITE_SINGLE_BLOCK */
	  	result = SDCard_Card_Data_Mode_Multi_Block_Write_w_SDIO(sector, buff, count);

  }

	/* Idle */

  SDIO_DeSelect_Card();

  if(result == 0) response = RES_OK;

  return response;  //we return "all is well" or RES_OK in fatfs speak
}

