/*
 *  Created on: Nov 13, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO_FATFS
 *  Processor: STM32F405
 *  Program version: 1.0
 *  Header file: SDcard_SDIO_driver.h
 *  Change history:
 */

#ifndef INC_SDCARD_SDIO_DRIVER_H_
#define INC_SDCARD_SDIO_DRIVER_H_

#include "stdint.h"
#include "stdio.h"
#include "stm32f405xx.h"
#include <SDIO_DMA_driver.h>

//LOCAL CONSTANT
/*
 * //Card commands
 * Each command is 6 bytes long, where we have a start bit (0), a Tx bit (1), the CMD itself (6 bits), the arg (32 bits), the crc (7 bits) and the end bit (1)
 * As such, a CMD0 reset command would be 0x40, 0x0, 0x0, 0x0, 0x0, 0x95 where 0x40is the CMD, the four 0x0 are the arg, and 0x95 is the crc with the end bit.
 * The command is just the command index in hex (CMD55 will be 0x77 since it is 01110111 where 0 is the start bit, 1 is tx bit and 110111 is the command index - 55 in decimal)
 */

//-----------------------------//

//card is not selected yet, RCA is 0x0
const static uint8_t CMD0_CMD	  			= 0x0;									//go to idle/reset card
const static uint16_t CMD0_ARG	  			= 0x0;

const static uint8_t CMD8_CMD	  			= 0x8;									//check voltage level
const static uint16_t CMD8_ARG	  			= 0x1aa;

const static uint8_t CMD55_CMD	  			= 0x37;									//chose ACMD
const static uint16_t CMD55_ARG	  			= 0x0;

const static uint8_t ACMD13_CMD	  			= 0xd;									//get SD status

const static uint8_t ACMD41_CMD	  			= 0x29;									//init card

const static uint8_t ACMD51_CMD	  			= 0x33;									//init card

const static uint8_t CMD2_CMD	  			= 0x2;									//request CID
const static uint16_t CMD2_ARG	  			= 0x0;

const static uint8_t CMD3_CMD				= 0x3;									//requests/generates RCA for the card
const static uint16_t CMD3_ARG	  			= 0x0;

//-----------------------------//

//card is not selected yet, but RCA is necessary
const static uint8_t CMD9_CMD	  			= 0x9;									//read CSD
const static uint16_t CMD9_ARG	  			= 0x0;

const static uint8_t CMD10_CMD	  			= 0xa;									//read CID
const static uint16_t CMD10_ARG	  			= 0x0;

const static uint8_t CMD7_CMD	  			= 0x7;									//select card
const static uint8_t CMD77_CMD	  			= 0x77;									//de-select card
																					//Note: CMD77 does not exist in the SDcard. It is a dummy to differentiate between select and de-select
const static uint16_t CMD7_ARG				= 0x0;

const static uint8_t ACMD6_CMD				= 0x6;									//choose bus width

const static uint8_t CMD13_CMD				= 0xd;									//get status
const static uint16_t CMD13_ARG_CARD_STA	= 0x0;									//card status
const static uint16_t CMD13_ARG_TASK_STA	= 0x8000;								//task status

//-----------------------------//

//card is selected, no RCA used
//also arguments are 32 bits

const static uint8_t CMD12_CMD	  			= 0xc;									//stop transfer action

const static uint8_t CMD17_CMD	  			= 0x11;									//read single block

const static uint8_t CMD18_CMD	  			= 0x12;									//read multi block

const static uint8_t CMD23_CMD	  			= 0x17;									//select block count for CMD18 and CMD25

const static uint8_t CMD24_CMD	  			= 0x18;									//write single block

const static uint8_t CMD25_CMD	  			= 0x19;									//write multi block


//LOCAL VARIABLE
static uint16_t SD_RCA	  	  				= 0x0;									//the RCA generated for the card (see CMD3)

//EXTERNAL VARIABLE

extern uint8_t CMDREND_flag;														//SDIO global flag
extern uint8_t DATAREND_flag;														//SDIO global flag

//FUNCTION PROTOTYPES
uint8_t SDCard_Card_ID_Mode_w_SDIO(void);											//this is to go through card ID mode using 200 kHz SDIOCK
void SDIO_speed_change(void);														//this is to go change SDIOCK to higher speed
void SDIO_Select_Card(void);														//select card using CMD7
void SDIO_DeSelect_Card(void);														//de-select card using CMD7 (will be using CMD77 instead to distinguish)
void SDIO_Change_bus_width(void);													//change bus width to 4-wide
uint8_t SDCard_Card_Data_Mode_Single_Block_Write_w_SDIO(uint32_t start_write_block_addr, uint8_t* write_buf_ptr);
uint8_t SDCard_Card_Data_Mode_Single_Block_Read_w_SDIO(uint32_t start_read_block_addr, uint8_t* read_buf_ptr);
uint8_t SDCard_Card_Data_Mode_Multi_Block_Write_w_SDIO(uint32_t start_write_block_addr, uint16_t write_block_cnt, uint8_t* write_buf_ptr);
uint8_t SDCard_Card_Data_Mode_Multi_Block_Read_w_SDIO(uint32_t start_read_block_addr, uint16_t read_block_cnt, uint8_t* read_buf_ptr);
void SDIO_Wait_for_idle_SD(void);													//wait until card is in "tran" state - waiting for transmission
void SDIO_Wait_for_rcv_SD(void);													//wait until card is in "rcv" state - receiving data
void SDIO_Wait_for_data_SD(void);													//wait until card is in "data" state - sending data
void SDCard_Card_Data_Mode_CSD_w_SDIO(void);										//extract CSD register - used for capacity calculation
void SDCard_Card_Data_Mode_SCR_w_SDIO(void);										//debug function

#endif /* INC_SDCARD_SDIO_DRIVER_H_ */
