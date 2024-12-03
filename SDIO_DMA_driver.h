/*
 *  Created on: Nov 28, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO_FATFS
 *  Processor: STM32F405
 *  Program version: 1.0
 *  Header file: SDIO_DMA_driver.h
 *  Change history:
 */

#ifndef INC_SDIO_DMA_DRIVER_H_
#define INC_SDIO_DMA_DRIVER_H_

#include "stdint.h"
#include "stdio.h"
#include "stm32f405xx.h"

//LOCAL CONSTANT

//LOCAL VARIABLE

//EXTERNAL VARIABLE
extern uint8_t CMDREND_flag;																		//flag to indicate that a CMD has been successfully received by the card
extern uint8_t DATAREND_flag;																		//flag to indicate that DATA has been successfully received/sent on the data bus

//FUNCTION PROTOTYPES
void SDIO_init(void);																				//this is to set up the SDIO peripheral
void SDIO_Host_Card_CMD_write(uint8_t command_index, uint16_t card_rca, uint16_t command_arg);		//this is to send a CMD with the card not selected yet
void SDIO_Host_Card_REG_upd(uint8_t command_index, uint32_t command_arg);							//this is to send a CMD with teh card selected already

void SDIO_DMA2_init(void);																			//set up the two DMAs
void DMA2_SDIO_IRQPriorEnable(void);																//assign priorities for the DMA and the SDIO interrupts

#endif /* INC_SDIO_DMA_DRIVER_H_ */
