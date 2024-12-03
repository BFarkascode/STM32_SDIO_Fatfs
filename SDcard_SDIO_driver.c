/*
 *  Created on: Nov 13, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO_FATFS
 *  Processor: STM32F405
 *  Program version: 1.0
 *  Source file: SDcard_SDIO_driver.c
 *  Change history:
 *
 */

//FATFS compliant version

#include <SDcard_SDIO_driver.h>

//1)SDcard init
uint8_t SDCard_Card_ID_Mode_w_SDIO(void) {  //fatfs demands "DRSTATUS" as an output. DRSTATUS if a BYTE that is "0" for success, "1" for no init and "2" for no disk. Reset value is 0x1.

	/*
	 *     Function initializes SD card using SDIO.
	 *     We are using an SD card (!) here and not an SD I/O or SDIO card, which is apparently different. The peripheral to communicate with either is called SDIO though.
	 *     So, when checking both the SDIO specs on the MCU and the SDcard specs, it must be kept in mind that we are using SDIO on an SDcard and not on an SDIO card.
	 *     Gives back "1" if init was successfull. Gives back "0" in case of failure. (Needed as "stat" values for fatfs library)
	 * 	   So, we have the "CS" pulled high by hardware, so we will be in SD mode to communicate with the card.
	 *	   When a CMD register is written to and the enable bit is set, the command is being transferred.
	 *	   Of note, both the responses and the arguments for the commands are different compared to SPI
	 *
	 */

  uint32_t ID_mode_response_buf = 0x0;		//id mode response buffer to extract RESP1


  //1)Reach card idle state - Wait for 74 cycles + 1 ms (see section 6.4)
  Delay_ms(250);												//we need a delay here to ensure that everything is properly set up
  	  	  	  	  	  	  	  	  	  	  	  	  				//delay size found by trial and error

  //set command response and timeout
  SDIO->CMD &= ~(1<<9);											//CMDPEND not used
//  SDIO->CMD |= (1<<8);										//bit 8 set removes CPSM timeout and activates interrupts instead. We stay with the timeout.

  SDIO_Host_Card_CMD_write(CMD0_CMD, 0x0, CMD0_ARG);     		//we send the idle command
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//CMD0 has no response
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//CMD0 ARG is dummy
  while((SDIO->STA & (1<<7)) != (1<<7));						//while CMD is not sent

  SDIO->ICR |= (1<<7);											//here we poll and not use interrupts

  Delay_ms(1);													//we need a small delay here

  //2)Validate host
  SDIO_Host_Card_CMD_write(CMD8_CMD, 0x0, CMD8_ARG);     		//CMD8
 	  	  	  	  	  	 	 	 	 	 	 	 	 	 	 	//CMD8 has R7 response - 32 bits where bits [11:8] is voltage accepted
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//CMD8 ARG is the same as before (0x1aa) where 0xaa is the check pattern and VHS is 2.7-3.6V (p92)

  while(CMDREND_flag);						  					//while response with CRC is not received

  CMDREND_flag = 1;

  //3)Initialize card

  uint8_t init_timeout = 0;

  while ((ID_mode_response_buf & (1 << 31)) != (1 << 31)) {  	//state detection goes differently in SD mode compared to SPI mode
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//ACMD41 response is R3 (operation control register, p179). If bit 32 is HIGH, the card is ready
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	    //ACMD41 argument (p49) - we need to set HCS [30], voltage levels and others depending on the card voltage (response from CMD8)

  if (init_timeout > 10) {  									//if after 10 attempts we still haven't managed to get the idle bit sorted

      printf(" \r\n");
      printf("Initialization error...");
      printf(" \r\n");
      printf("Restart device...");
      printf(" \r\n");
      ReBoot();													//this is here to remove the double start bug
      while(1);
      return 1;													//we should never get here

  }

  SDIO_Host_Card_CMD_write(CMD55_CMD, 0x0, CMD55_ARG);         //CMD55		-	switch to app specific command with RCA of 0x0 in idle state

  while(CMDREND_flag);										   //while response with CRC is not received

  CMDREND_flag = 1;

  SDIO_Host_Card_CMD_write(ACMD41_CMD, 0x4010, 0x0);       	   //ACMD41	-	we send HCS HIGH to the card
    															 //Note: ACMD41 has a full 32 bit argument unlike other commands which are RCA+ARG

  while((SDIO->STA & (1<<0)) != (1<<0));					    //while response without CRC is not received
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	    //here we poll and not use interrupts

  SDIO->ICR |= (1<<0);										    //clear STA flag

  ID_mode_response_buf = SDIO->RESP1;

  init_timeout++;

  }

  ID_mode_response_buf = 0x0;

  SDIO_Host_Card_CMD_write(CMD2_CMD, 0x0, CMD2_ARG);			//CMD2
	 	 	 	 	 	 	 	 	 	 	 	 	 	 		//CMD2 has R2 LONG response - 136 bits where bits [127:1] is CID
 	  	  	  	  	  	 	 	 	 	 	 	 	 	 		//CMD2 ARG is dummy
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//RESPCMD is "63"

  while(CMDREND_flag);											//while response with CRC is not received

  CMDREND_flag = 1;

  SDIO_Host_Card_CMD_write(CMD3_CMD, 0x0, CMD3_ARG);			//CMD3
	 	 	 	 	 	 	 	 	 	 	 	 	 	 		//CMD3 has R6 SHORT response - 32 bits where bits [31:16] is the card's RCA
 	  	  	  	  	  	 	 	 	 	 	 	 	 	 		//CMD3 ARG is dummy

  while(CMDREND_flag);

  CMDREND_flag = 1;

  Delay_ms(1);													//we wait a little so the RESP1 registers are properly updated

  SD_RCA = (uint16_t) (SDIO->RESP1 >> 16);						//we remove the first 16 bits which are the card status bits

  return 0;

}

//2)
void SDIO_speed_change(void){

	/*
	 * We change the SDIO speed to 4 MHz
	 * Note: clocking is very tricky. The SDIO source must be clocked half as fast as the DMA. Also, the CLKDIV can't be any value...
	 *
	 */

	  SDIO->CLKCR &= ~(0xFF<<0);								//we wipe the CLKDIV register

	  SDIO->CLKCR |= (2<<0);									//DIV4

}

//3)
void SDIO_Select_Card(void){

	/*
	 *
	 * This function activates the selected card
	 * SD_RCA is generated within the init function (see CMD3)
	 * after this section, the card should be in "tran" state
	 *
	 */

	//1)select card
	SDIO_Host_Card_CMD_write(CMD7_CMD, SD_RCA, CMD7_ARG);			//CMD7
			 	 	 	 	 	 	 	 	 	 	 	 	 	 	//CMD7 has R1b SHORT response
		 	  	  	  	  	  	 	 	 	 	 	 	 	 	 	//CMD7 ARG is RCA with dummy
		  	  	  	  	  	  	  	  	  	  	  	  	  	  	    //RESPCMD is "7"
	while(CMDREND_flag);

	CMDREND_flag = 1;

}

//4)
void SDIO_DeSelect_Card(void){
	/*
	 *
	 * This function activates the selected card
	 * SD_RCA is generated within the init function
	 * after this section, the card should be in "stb" state - and not be selected
	 *
	 */

	//1)select card
	SDIO_Host_Card_CMD_write(CMD77_CMD, 0x0, CMD7_ARG);				//CMD7
			 	 	 	 	 	 	 	 	 	 	 	 	 	 	//CMD7 has R1b SHORT response - only if card is selected
		 	  	  	  	  	  	 	 	 	 	 	 	 	 	 	//CMD7 ARG is RCA with dummy
		  	  	  	  	  	  	  	  	  	  	  	  	  	  	    //RESPCMD is "7"
	while((SDIO->STA & (1<<7)) != (1<<7));							//while CMD is not sent
																	//here we poll and not use interrupts
	SDIO->ICR |= (1<<7);

}

//5)
void SDIO_Change_bus_width(void){

	/*
	 * Change bus width to 4
	 * We need to do it on both the card (ACMD6) and the SDIO
	 *
	 */


    SDIO_Host_Card_CMD_write(CMD55_CMD, SD_RCA, CMD55_ARG);         //CMD55		-	switch to app specific command with RCA of 0x0 in idle state
    																			     //Note: CMD55 may not be necessary, but it won't do any harm either so it stays for now
    while(CMDREND_flag);											//while response with CRC is not received

	CMDREND_flag = 1;

    SDIO_Host_Card_CMD_write(ACMD6_CMD, 0x0, 0x2);       			//ACMD6	-	change to 4-bit bus

    while(CMDREND_flag);											//while response with CRC is not received

	CMDREND_flag = 1;

	//put SDIO to 4-bit version
	SDIO->CLKCR |= (1<<11);
	SDIO->CLKCR &= ~(1<<12);

}


//6)SDIO SD single write
uint8_t SDCard_Card_Data_Mode_Single_Block_Write_w_SDIO(uint32_t start_write_block_addr, uint8_t* write_buf_ptr) {

	/*
	 *
	 * We read one block of data from an address
	 * A block is 512 bytes
	 *
	 */

	  SDIO->DCTRL &= ~(1<<0);										//turn off DPSM
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Note: a full DCTRL reset is necessary between transfers

	  SDIO->DTIMER = 0xFFFF;										//timeout value in SCK ticks
																	//this starts to count down when the command module is in "wait" or "busy"
																	//if tripped, it raises a flag
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: we aren't actually using this

	  SDIO->DLEN = 512;												//we read out one SD block at a time

	  SDIO->DCTRL &= ~(1<<2);										//block mode
	  	  	  	  	  	  	  	  	  	  	  	  	  	  			//Note: stream mode is not available on SD cards
	  SDIO->DCTRL &= ~(1<<1);										//data direction is from MCU to card

	  SDIO->DCTRL |= (1<<3);										//DMA enabled

	  DMA2_Stream3->M0AR = write_buf_ptr;							//we connect the DMA to the memory buffer

	  DMA2_Stream3->CR |= (1<<0);									//DMA Tx side enabled

	  SDIO->DCTRL |= (9<<4);										//block size of 512 bytes	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: this means that the CRC will come after 512 bytes

	  SDIO_Host_Card_REG_upd(CMD24_CMD, start_write_block_addr);

	  while(CMDREND_flag);											//while response with CRC is not received
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Note: this must be sent over before we activate the DPSM, otehrwise we might time out
	  CMDREND_flag = 1;

	  SDIO->DCTRL |= (1<<0);										//enable DPSM

	  while(DATAREND_flag);

	  DATAREND_flag = 1;

	  SDIO_Wait_for_idle_SD();										//wai until card is idle again ("tran" state)

	  DMA2_Stream3->CR &= ~(1<<0);									//DMA Tx side disabled

	  return 0;

}

//7)SDIO SD single read
uint8_t SDCard_Card_Data_Mode_Single_Block_Read_w_SDIO(uint32_t start_read_block_addr, uint8_t* read_buf_ptr) {

	/*
	 * Writes a single 512 byte block to the SDcard.
	 */

	  SDIO->DCTRL &= ~(1<<0);										//turn off DPSM
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Note: a full DCTRL reset is necessary between transfers

	  SDIO->DTIMER = 0xFFFF;										//timeout value in SCK ticks

	  SDIO->DLEN = 512;												//we write one block at a time

	  SDIO->DCTRL &= ~(1<<2);										//block mode

	  SDIO->DCTRL |= (1<<1);										//data direction is from card to MCU

	  SDIO->DCTRL |= (1<<3);										//DMA enabled

	  DMA2_Stream6->M0AR = read_buf_ptr;							//we want the data to be funnelled to the memory buffer

	  DMA2_Stream6->CR |= (1<<0);									//DMA Rx side enabled

	  SDIO->DCTRL |= (9<<4);										//block size of 512 bytes
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Note: this means that the CRC will come after 512 bytes
	  SDIO_Host_Card_REG_upd(CMD17_CMD, start_read_block_addr);		//CMD17
		 	 	 	 	 	 	 	 	 	 	 	 	 	 		//CMD17 has R1 SHORT response
	 	  	  	  	  	  	 	 	 	 	 	 	 	 	 		//CMD17 ARG is data address
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//RESPCMD is "17"

	  while(CMDREND_flag);											//while response with CRC is not received

	  CMDREND_flag = 1;

	  SDIO->DCTRL |= (1<<0);

	  while(DATAREND_flag);

	  DATAREND_flag = 1;

	  SDIO_Wait_for_idle_SD();

	  DMA2_Stream6->CR &= ~(1<<0);									//DMA Rx side disabled

	  return 0;

}

//8)SDIO SD single write
uint8_t SDCard_Card_Data_Mode_Multi_Block_Write_w_SDIO(uint32_t start_write_block_addr, uint16_t write_block_cnt, uint8_t* write_buf_ptr) {

	/*
	 * Write multiple blocks
	 * Note: we don't need CMD12 to close the transmission!
	 *
	 */

	  SDIO->DCTRL &= ~(1<<0);									//turn off DPSM

	  SDIO->DTIMER = 0xFFFF;									//timeout value in SCK ticks

	  SDIO->DLEN = (write_block_cnt * 512);						//we write one SD block at a time

	  SDIO->DCTRL &= ~(1<<2);									//block mode

	  SDIO->DCTRL &= ~(1<<1);									//data direction is from MCU to card

	  SDIO->DCTRL |= (1<<3);									//DMA enabled

	  DMA2_Stream3->M0AR = write_buf_ptr;

	  DMA2_Stream3->CR |= (1<<0);								//DMA Tx side enabled

	  SDIO->DCTRL |= (9<<4);									//block size of 512 bytes	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: this means that the CRC will come after 512 bytes

	  SDIO_Host_Card_REG_upd(CMD23_CMD, write_block_cnt);		//block count sent

	  while(CMDREND_flag);
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: this must be sent over before we activate the DPSM, otehrwise we might time out
	  CMDREND_flag = 1;

	  SDIO_Host_Card_REG_upd(CMD25_CMD, start_write_block_addr);//multi-write CMD sent

	  while(CMDREND_flag);										//while response with CRC is not received
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: this must be sent over before we activate the DPSM, otehrwise we might time out
	  CMDREND_flag = 1;

	  SDIO_Wait_for_rcv_SD();

	  SDIO->DCTRL |= (1<<0);									//activate DPSM after the card is sent for reception

	  while(DATAREND_flag);

	  DATAREND_flag = 1;

	  SDIO_Wait_for_idle_SD();

	  DMA2_Stream3->CR &= ~(1<<0);								//DMA Tx side disabled

	  return 0;

}

//9)SDIO SD single read
uint8_t SDCard_Card_Data_Mode_Multi_Block_Read_w_SDIO(uint32_t start_read_block_addr, uint16_t read_block_cnt, uint8_t* read_buf_ptr) {

	/*
	 *
	 * Writes a single 512 byte block to the SDcard.
	 *
	 */

	  SDIO->DCTRL &= ~(1<<0);									//turn off DPSM
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: a full DCTRL reset is necessary between transfers


	  SDIO->DTIMER = 0xFFFF;									//timeout value in SCK ticks
																//this starts to count down when the command module is in "wait" or "busy"
																//if tripped, it raises a flag

	  SDIO->DLEN = (read_block_cnt * 512);											//we read out one block at a time

	  SDIO->DCTRL &= ~(1<<2);									//block mode
	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Note: stream mode is not available on SD cards
	  SDIO->DCTRL |= (1<<1);									//data direction is from card to MCU

	  SDIO->DCTRL |= (1<<3);									//DMA enabled

	  DMA2_Stream6->M0AR = read_buf_ptr;						//we want the data to be read from the SDIO FIFO register

	  DMA2_Stream6->CR |= (1<<0);								//DMA Rx side enabled

	  SDIO->DCTRL |= (9<<4);									//block size of 512 bytes
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: this means that the CRC will come after 512 bytes

	  SDIO_Host_Card_REG_upd(CMD23_CMD, read_block_cnt);		//block count sent

	  while(CMDREND_flag);										//while response with CRC is not received
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: this must be sent over before we activate the DPSM, otehrwise we might time out
	  CMDREND_flag = 1;

	  SDIO->DCTRL |= (1<<0);									//we enable first the DPSM

	  SDIO_Host_Card_REG_upd(CMD18_CMD, start_read_block_addr);	//once this command is sent, the card will send data to the MCU

	  while(CMDREND_flag);										//while response with CRC is not received

	  CMDREND_flag = 1;

	  while(DATAREND_flag);										//wait until the data is received

	  DATAREND_flag = 1;

	  SDIO_Wait_for_idle_SD();

	  DMA2_Stream6->CR &= ~(1<<0);								//DMA Rx side disabled

	  return 0;

}

//10)
void SDIO_Wait_for_idle_SD(void){

	/*
	 *
	 * Wait until card is back in "tran" state - which is technically the idle state after the card is selected
	 *
	 */

	//we check the initial values for RESP1
	//Note: the RESP1 values will reflect on the state of the card when the CMD is received, not taking into consideration the CMD yet...

	SDIO_Host_Card_CMD_write(CMD13_CMD, SD_RCA, CMD13_ARG_CARD_STA);

	while(CMDREND_flag);									//while response with CRC is not received

	CMDREND_flag = 1;

	while(!(((SDIO->RESP1 & (1<<11)) == (1<<11)) && ((SDIO->RESP1 & (1<<10)) != (1<<10)) && ((SDIO->RESP1 & (1<<9)) != (1<<9)))){									//while we are not "data"

			//"tran" is when RESP1[12:9] = 0x4
			SDIO_Host_Card_CMD_write(CMD13_CMD, SD_RCA, CMD13_ARG_CARD_STA);	//CMD13
				 	 	 	 	 	 	 	 	 	 	 	 	 	 			//CMD13 has R1 SHORT response
			 	  	  	  	  	  	 	 	 	 	 	 	 	 	 			//CMD13 ARG is RCA with dummy
			  	  	  	  	  	  	  	  	  	  	  	  	  	  	    		//RESPCMD is "13"

			while(CMDREND_flag);

			CMDREND_flag = 1;

	 }

}

//11)
void SDIO_Wait_for_rcv_SD(void){

	/*
	 *
	 * Wait until card is back in "data" state
	 *
	 */

	SDIO_Host_Card_CMD_write(CMD13_CMD, SD_RCA, CMD13_ARG_CARD_STA);

	while(CMDREND_flag);

	CMDREND_flag = 1;

	while(!(((SDIO->RESP1 & (1<<11)) == (1<<11)) && ((SDIO->RESP1 & (1<<10)) == (1<<10)) && ((SDIO->RESP1 & (1<<9)) != (1<<9)))){									//while we are not "rcv"

			//"rcv" is when RESP1[12:9] = 0x6
			SDIO_Host_Card_CMD_write(CMD13_CMD, SD_RCA, CMD13_ARG_CARD_STA);

			while(CMDREND_flag);

			CMDREND_flag = 1;

	 }

}

//12)
void SDIO_Wait_for_data_SD(void){

	/*
	 *
	 * Wait until card is back in "data" state
	 *
	 */

	SDIO_Host_Card_CMD_write(CMD13_CMD, SD_RCA, CMD13_ARG_CARD_STA);

	while(CMDREND_flag);

	CMDREND_flag = 1;

	while(!(((SDIO->RESP1 & (1<<11)) == (1<<11)) && ((SDIO->RESP1 & (1<<10)) != (1<<10)) && ((SDIO->RESP1 & (1<<9)) == (1<<9)))){									//while we are not "tran"

			//"data" is when RESP1[12:9] = 0x5
			SDIO_Host_Card_CMD_write(CMD13_CMD, SD_RCA, CMD13_ARG_CARD_STA);

			while(CMDREND_flag);

			CMDREND_flag = 1;

	 }

}


//13)Extract card capacity
void SDCard_Card_Data_Mode_CSD_w_SDIO(void) {

	/*
	 * Ws extract the CSD register
	 * Note: we assume that we have only one card on the bus, thus the SD_RCA (the card realtive address) is kept as-is.
	 *
	 */

	SDIO_Host_Card_CMD_write(CMD9_CMD, SD_RCA, CMD9_ARG);		//CMD9
		 	 	 	 	 	 	 	 	 	 	 	 	 	 	//CMD9 has R2 LONG response - 136 bits where bits [127:1] is CSD
	 	  	  	  	  	  	 	 	 	 	 	 	 	 	 	//CMD2 ARG is dummy
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//RESPCMD is "63"

	while(CMDREND_flag);							//while response with CRC is not received

	CMDREND_flag = 1;

}



//14)DEBUG function
void SDCard_Card_Data_Mode_SCR_w_SDIO(void) {

	/*
	 *
	 * Read out the SCR register from the card to the data FIFO
	 *
	 */

/*	//1)select card
	  //after this section, the card should be in "tran" state
	  SDIO_Host_Card_CMD_write(CMD7_CMD, SD_RCA, CMD7_ARG);		//CMD7
		 	 	 	 	 	 	 	 	 	 	 	 	 	 	//CMD7 has R1b SHORT response
	 	  	  	  	  	  	 	 	 	 	 	 	 	 	 	//CMD7 ARG is RCA with dummy
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	    //RESPCMD is "7"
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: R1b comes with a "busy" flag!!!
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: card status R1 is 11100000000 - [8] ready_for_data, [10:9]card in standby
	  	  	  	  	  	  										//card status description is on p120

	  while(CMDREND_flag);					//while response with CRC is not received

	   //SDIO->ICR = 0xFFFFFFFF;										//clear STA flag

	  //wait until the card is in "tran" mode

	  SDIO_Wait_for_idle_SD();*/

	//this section above is only necessary if the card has not been selected yet using CMD7

//---------------------------------------------------------------

	  //extract card status register
	  //Note: simple CMD13 just sends the card status R1 but not the actual register

	  SDIO->DCTRL &= ~(1<<0);									//turn off DPSM

	  SDIO->DTIMER = 0xFFFF;									//timeout value in SCK ticks
																//this starts to count down when the command module is in "wait" or "busy"
																//if tripped, it raises a flag

	  SDIO->DLEN = 64;											//512 bits is 64 bytes

	  SDIO->DCTRL &= (~1<<2);									//block mode
	  	  	  	  	  	  	  	  	  	  	  	  	  	  		//Note: stream mode is not available on SD cards
	  SDIO->DCTRL |= (1<<1);									//data direction is from card to MCU

	  SDIO->DCTRL |= (1<<3);									//DMA enabled

	  DMA2_Stream6->CR |= (1<<0);								//DMA Rx side enabled

	  SDIO->DCTRL |= (6<<4);									//block size of 64 bytes


	  SDIO_Host_Card_CMD_write(CMD55_CMD, SD_RCA, CMD55_ARG);       //CMD55		-	switch to app specific command with RCA of 0x0 in idle state
	  																			     //Note: CMD55 may not be necessary, but it won't do any harm either so it stays for now
	  while(CMDREND_flag);								//while response with CRC is not received

	  CMDREND_flag = 1;

	  SDIO->DCTRL |= (1<<0);										//enable DPSM
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//DPSM timeout is 64 cycles of SCK
	  SDIO_Host_Card_CMD_write(ACMD13_CMD, 0x0, 0x0);       		//ACMD13
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//adtc type
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//CMD55 to select it
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//data transfer is on the DAT bus with CRC16
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//can only be sent when the card is in "tran"
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//reply will be 512 bits in the FIFO plus the CRC
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//R1 on the CMD line

	  while(CMDREND_flag);								//while response with CRC is not received

	  CMDREND_flag = 1;

	  uint32_t sta3 = SDIO->STA;									//10000010000001000000		- RXFIFOE(19), RXACT(13) CMDREND

	  while((SDIO->STA & (1<<13)) == 0);

	  while((SDIO->STA & (1<<21)) == 0);

}

//15)Reboot function
//here to remove the double start bug from the SDcard
void ReBoot(void)
{

	uint32_t Boot_Section_Start_Addr = 0x8000000;													//FLASH base address
	uint32_t Boot_reset_vector_addr;																//this is the address of the app's reset vector (which is also a function pointer!)
	void (*Start_Boot_func_ptr)(void);																//the local function pointer we define

	if((*(uint32_t*)Boot_Section_Start_Addr) == 0x20020000)											//we check, what is stored at the Boot_Section_Addr. It should be the very first word of the app's code.
																									//we do this check since we may run a device without a bootloader
	{
		printf("Rebooting...\r\n");
		Boot_reset_vector_addr = *(uint32_t*)(Boot_Section_Start_Addr + 4);							//we define a pointer to APP_ADDR + 4 and then dereference it to extract the reset vector for the app
																									//JumpAddress will hold the reset vector address (which won't be the same as APP_ADDR + 4, the address is just stored there)
		Start_Boot_func_ptr = Boot_reset_vector_addr;													//we call the local function pointer with the address of the app's reset vector
																									//Note: for the bootloader, this address is an integer. In reality, it will be a function pointer once the app is placed.
		__set_MSP(*(uint32_t*) Boot_Section_Start_Addr);												//we move the stack pointer to the APP address
		Start_Boot_func_ptr();																		//here we call the APP reset function through the local function pointer
	} else {
		printf("Boot not found. \r\n");
	}

}
