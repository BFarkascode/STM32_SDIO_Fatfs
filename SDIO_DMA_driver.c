/*
 *  Created on: Nov 28, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO_FATFS
 *  Processor: STM32F405
 *  Program version: 1.0
 *  Source file: SDIO_DMA_driver.c
 *  Change history:
 */

#include <SDIO_DMA_driver.h>


//1)SDIO init
void SDIO_init(void) {

  /*

	Function activates the SDIO peripheral within the MCU
	Of note, all lines except the SCK need a pullup
	Also, the speed is to be limited to "medium".
	Note: these two limitations are ignored in CubeMx, thus the peripheral will not work

	*/

	//1) SDIO clocking
	RCC->APB2ENR |= (1<<11);													//SDIO clocking enabled
																				//SDIO clocks on the PLLQ output of the main PLL unless told otherwise
																				//PLLQ is set in the clock driver source file
																				//Note: SDIO runs on a separate clock domain than the MCU

	//2) GPIOS
	//SDIO DMA is on DMA2 Ch4 Stream 3 or Ch4 Steam 6

	RCC->AHB1ENR |=	(1<<2);														//PORTC clocking
	RCC->AHB1ENR |=	(1<<3);														//PORTD clocking

	//alternative mode
	GPIOC->MODER &= ~(1<<16);													//alternate function for PC8	- D0
	GPIOC->MODER |= (1<<17);													//alternate function for PC8
	GPIOC->MODER &= ~(1<<18);													//alternate function for PC9	- D1
	GPIOC->MODER |= (1<<19);													//alternate function for PC9
	GPIOC->MODER &= ~(1<<20);													//alternate function for PC10	- D2
	GPIOC->MODER |= (1<<21);													//alternate function for PC10
	GPIOC->MODER &= ~(1<<22);													//alternate function for PC11	- D3
	GPIOC->MODER |= (1<<23);													//alternate function for PC11
	GPIOC->MODER &= ~(1<<24);													//alternate function for PC12	- CLK
	GPIOC->MODER |= (1<<25);													//alternate function for PC12
	GPIOD->MODER &= ~(1<<4);													//alternate function for PD2	- CMD
	GPIOD->MODER |= (1<<5);														//alternate function for PD2

	//AF12 for SDIO
	GPIOC->AFR[1] |= (12<<0);													//PC8 - AF12
	GPIOC->AFR[1] |= (12<<4);													//PC9 - AF12
	GPIOC->AFR[1] |= (12<<8);													//PC10 - AF12
	GPIOC->AFR[1] |= (12<<12);													//PC11 - AF12
	GPIOC->AFR[1] |= (12<<16);													//PC12 - AF12
	GPIOD->AFR[0] |= (12<<8);													//PD2 - AF12

	//medium speed
	//Note: hgiher speed may not work for the SDIO
	GPIOC->OSPEEDR |= (1<<16);													//medium speed
	GPIOC->OSPEEDR |= (1<<18);													//medium speed
	GPIOC->OSPEEDR |= (1<<20);													//medium speed
	GPIOC->OSPEEDR |= (1<<22);													//medium speed
	GPIOC->OSPEEDR |= (1<<24);													//medium speed
	GPIOD->OSPEEDR |= (1<<4);													//medium speed


	//pullup
	//Note: a pullup is necessary on these lines
	GPIOC->PUPDR |= (1<<16);													//pullup on DAT0
	GPIOC->PUPDR &= ~(1<<17);
	GPIOC->PUPDR |= (1<<18);													//pullup on DAT1
	GPIOC->PUPDR &= ~(1<<19);
	GPIOC->PUPDR |= (1<<10);													//pullup on DAT2
	GPIOC->PUPDR &= ~(1<<21);
	GPIOC->PUPDR |= (1<<22);													//pullup on DAT3
	GPIOC->PUPDR &= ~(1<<23);
	GPIOD->PUPDR |= (1<<4);														//pullup on CMD
	GPIOD->PUPDR &= ~(1<<5);

	Delay_us(1);																//this delay is necessary, otherwise GPIO setup may freeze

	//------------------------------------------

	//3) Allow card power and clocking
	SDIO->POWER	|= (3<<0);														//clock and card power
	Delay_ms(1);																//we need a bit of wait here

	//4) Configure SDIO peripheral
	//set the SDIO bus to 1-wide for setup
	SDIO->CLKCR	&= ~(1<<11);													//clock and bus control [12:11] should be 2'b00 for 1-wide bus on the MCU side
	SDIO->CLKCR	&= ~(1<<12);

	SDIO->CLKCR	|= (80<<0);														//rest of the register is the CLKDIV
																				//Note: we divide the SDIOCK by 80 to have an initial clock frequency of 200 kHz from 16 MHz
	SDIO->CLKCR	|= (1<<8);														//SDIOCK enable

	//5)
	//Interrupts are critical for making the SDIO work
	SDIO->MASK |= (1<<8);														//we demask the IRQ for DATA end
	SDIO->MASK |= (1<<6);														//we demask the IRQ for CMDREND
	SDIO->MASK |= (1<<3);														//we demask the IRQ for DATA timeout
	SDIO->MASK |= (1<<2);														//we demask the IRQ for CMD timeout

	//We don't configure the data transfer part here
	//Note: we don't enable transfer here, nor do we tell, which direction the data is to flow since both will the the "data part" of the SDIO

}


//2)Send CMDs before the card is selected
void SDIO_Host_Card_CMD_write(uint8_t command_index, uint16_t card_rca, uint16_t command_arg){

	/*
	 * Send ac, bc or bcr type of commands to the card
	 */

	SDIO->CMD &= ~(1<<10);														//deactivate CSMD
																				//Note: a running CSMD will send a CMD immediately when the CMD register is updated!
	SDIO->CMD &= ~(63<<0);														//wipe the command index

	if((command_index == 0x0) || (command_index == 0x77)){											//we expect no response for CMD0

		SDIO->CMD &= ~(1<<6);
		SDIO->CMD &= ~(1<<7);
		if(command_index == 0x0){

			//do nothing

		} else {

			command_index = 0x7;

		}

	} else if((command_index == 0x2) || (command_index == 0x9) || (command_index == 0x10)){			//we expect a long response for CMD2, CMD9 and CMD10

		SDIO->CMD |= (1<<6);
		SDIO->CMD |= (1<<7);

	} else {																						//we expect a short response for the rest

		SDIO->CMD |= (1<<6);
		SDIO->CMD &= ~(1<<7);

	}

	SDIO->ARG = (uint32_t) (card_rca << 16) | (uint32_t)command_arg;								//we load the argument as the RCA and the CMD arg

	SDIO->CMD |= (command_index << 0);																//here the CMD will not be sent yet since CSMD is deactivated
	SDIO->CMD |= (1<<10);																			//send the CMD

}

//3)Send a CMD to a card that has already be selected
void SDIO_Host_Card_REG_upd(uint8_t command_index, uint32_t command_arg){

	/*
	 * Sends actr CMDs
	 * No RCA is fed into the CSMD here
	 */

	SDIO->CMD &= ~(1<<10);																			//deactivate CSMD
	SDIO->CMD &= ~(63<<0);																			//wipe the command index
	SDIO->CMD |= (1<<6);																			//set R1 response
	SDIO->CMD &= ~(1<<7);
	SDIO->ARG = command_arg;																		//here the argument will be 32 bits
	SDIO->CMD |= (command_index << 0);
	SDIO->CMD |= (1<<10);																			//send CMD

}

//4)Init the DMAs
void SDIO_DMA2_init(void){

	/*
	 * SDIO is on DMA2 Stream3 Ch4 or Stream6 Ch4
	 * We use both by the way to Tx and Rx
	 * Alternative is to change the DMA setup between Tx and Rx, though this may lead to additional problems...
	 *
	 */

	RCC->AHB1ENR |= (1<<22);																		//enable DMA2 clocking

	//1)
	DMA2_Stream3->CR &= ~(1<<0);																	//we disable the DMA stream
	DMA2_Stream6->CR &= ~(1<<0);																	//we disable the DMA stream

	//Tx
	DMA2_Stream3->CR &= ~(1<<8);																	//circular mode is off
	DMA2_Stream3->CR |= (1<<5);																		//peri is the flow controller
	DMA2_Stream3->CR |= (1<<4);																		//transfer complete IRQ activated
	DMA2_Stream3->CR |= (1<<3);																		//half transfer IRQ activated
	DMA2_Stream3->CR |= (1<<2);																		//error IRQ activated
	DMA2_Stream3->CR |= (1<<21);																	//peri side incremental burst of 4	-	this means we will be writing 32 bits on one run, not 4x8
	DMA2_Stream3->CR &= ~(1<<22);																	//peri side incremental burst of 4
	DMA2_Stream3->CR |= (1<<23);																	//mem side incremental burst of 4
	DMA2_Stream3->CR &= ~(1<<24);																	//mem side incremental burst of 4
	DMA2_Stream3->CR |= (1<<6);																		//read from mem to peri
	DMA2_Stream3->CR &= ~(1<<7);																	//read from mem to peri
	DMA2_Stream3->CR |= (4<<25);																	//select channel 4
	DMA2_Stream3->CR &= ~(1<<13);																	//memory data size is 8 bits
	DMA2_Stream3->CR &= ~(1<<14);																	//memory data size is 8 bits
	DMA2_Stream3->CR &= ~(1<<11);																	//peri data size is 32 bits
	DMA2_Stream3->CR |= (1<<12);																	//peri data size is 32 bits
	DMA2_Stream3->CR |= (1<<10);																	//memory increment active
	DMA2_Stream3->PAR = (uint32_t) (&(SDIO->FIFO));													//we want the data to be written to the SDIO FIFO register
	DMA2_Stream3->FCR |= (1<<7);																	//FIFO error interrupt enabled
	DMA2_Stream3->FCR |= (1<<0);																	//FIFO threshold FULL
	DMA2_Stream3->FCR |= (1<<1);
	DMA2_Stream3->FCR |= (1<<2);																	//direct mode disabled

	//Rx
	DMA2_Stream6->CR &= ~(1<<8);																	//circular mode is off
	DMA2_Stream6->CR |= (1<<5);																		//peri is the flow controller
																									//Note: this above passes the transfer number from the peripheral to the DMA
	DMA2_Stream6->CR |= (1<<4);																		//transfer complete IRQ activated
	DMA2_Stream6->CR |= (1<<3);																		//half transfer IRQ activated
	DMA2_Stream6->CR |= (1<<2);																		//error IRQ activated
	DMA2_Stream6->CR |= (1<<21);																	//peri side incremental burst of 4
	DMA2_Stream6->CR &= ~(1<<22);																	//peri side incremental burst of 4
	DMA2_Stream6->CR |= (1<<23);																	//mem side incremental burst of 4
	DMA2_Stream6->CR &= ~(1<<24);																	//mem side incremental burst of 4
	DMA2_Stream6->CR &= ~(1<<6);																	//read from peri to mem
	DMA2_Stream6->CR &= ~(1<<7);																	//read from peri to mem
	DMA2_Stream6->CR |= (4<<25);																	//select channel 4
	DMA2_Stream6->CR &= ~(1<<13);																	//memory data size is 8 bits
	DMA2_Stream6->CR &= ~(1<<14);																	//memory data size is 8 bits
	DMA2_Stream6->CR &= ~(1<<11);																	//peri data size is 32 bits
	DMA2_Stream6->CR |= (1<<12);																	//peri data size is 32 bits
	DMA2_Stream6->CR |= (1<<10);																	//memory increment active
	DMA2_Stream6->PAR = (uint32_t) (&(SDIO->FIFO));
	DMA2_Stream6->FCR |= (1<<7);																	//FIFO error interrupt enabled
	DMA2_Stream6->FCR |= (1<<0);																	//FIFO threshold FULL
	DMA2_Stream6->FCR |= (1<<1);
	DMA2_Stream6->FCR |= (1<<2);																	//direct mode disabled

}

//5)Tx side IRQ
void DMA2_Stream3_IRQHandler (void){

	/*
	 * Handler for DMA2 ST3
	 * This will be the Tx side of the SDIO
	 *
	 */

	//1)
	if ((DMA2->LISR & (1<<27)) == (1<<27)) {														//if we had the full transmission triggered on channel 4

		DMA2->LIFCR |= (1<<27);																		//we remove the full complete flag

	} else if ((DMA2->LISR & (1<<26)) == (1<<26)){													//if we had half transmission

		DMA2->LIFCR |= (1<<26);																		//we remove the half complete flag

	} else if ((DMA2->LISR & (1<<22)) == (1<<22)){													//if we had an error trigger on channel 4

		DMA2->LIFCR |= (1<<22);																		//we remove the FIFO error as well
		DMA2->LIFCR |= (1<<27);																		//we remove the full complete flag

	} else if ((DMA2->LISR & (1<<25)) == (1<<25)){													//if we had an error trigger on channel 4

		printf("DMA transmission error!");
		while(1);

	} else {

		//do nothing

	}

}

//6)Rx side IRQ
void DMA2_Stream6_IRQHandler (void){

	/*
	 * Handler for DMA2 ST3
	 * This will be the Rx side of the SDIO
	 *
	 */

	//1)
	if ((DMA2->HISR & (1<<21)) == (1<<21)) {														//if we had the full transmission triggered on channel 4

		DMA2->HIFCR |= (1<<21);																		//we remove the full complete flag

	} else if ((DMA2->HISR & (1<<20)) == (1<<20)){													//if we had half transmission

		DMA2->HIFCR |= (1<<20);																		//we remove the half complete flag

	} else if ((DMA2->HISR & (1<<16)) == (1<<16)){													//if we had an error trigger on channel 4

		DMA2->HIFCR |= (1<<16);																		//we remove the FIFO error as well
		DMA2->HIFCR |= (1<<21);																		//we remove the full complete flag

	}  else if ((DMA2->HISR & (1<<19)) == (1<<19)){													//if we had an error trigger on channel 4

		printf("DMA transmission error!");
		while(1);

	} else {

		//do nothing

	}

}

//7)SDIO IRQ
void SDIO_IRQHandler (void){

	/*
	 * Handler for SDIO
	 *
	 */

	//1)
	if ((SDIO->STA & (1<<2)) == (1<<2)) {												//CMD timeout error

		SDIO->ICR |= (1<<2);
		while(1);

	} else if((SDIO->STA & (1<<3)) == (1<<3)) {											//DATA timeout error

		SDIO->ICR |= (1<<3);
//		while(1);

	} else if ((SDIO->STA & (1<<6)) == (1<<6)) {										//CMDREND - CMD send and response received

		SDIO->ICR |= (1<<6);
		CMDREND_flag = 0;

	}  else if ((SDIO->STA & (1<<8)) == (1<<8)) {										//DATA received or sent

		SDIO->ICR |= (1<<8);
		DATAREND_flag = 0;

	} else {

		//do nothing

	}

}


//8)DMA IRQ priority
void DMA2_SDIO_IRQPriorEnable(void) {
	/*
	 * We call the two special CMSIS functions to set up/enable the IRQ.
	 * Note: hardware interrupt priority should be lower than what is set up for the RTOS (currently level 5)
	 *
	 */
	NVIC_SetPriority(DMA2_Stream3_IRQn, 1);										//IRQ priority for stream7
	NVIC_EnableIRQ(DMA2_Stream3_IRQn);											//IRQ enable for stream7
	NVIC_SetPriority(DMA2_Stream6_IRQn, 1);										//IRQ priority for stream7
	NVIC_EnableIRQ(DMA2_Stream6_IRQn);											//IRQ enable for stream7
	NVIC_SetPriority(SDIO_IRQn,1);
	NVIC_EnableIRQ(SDIO_IRQn);
}
