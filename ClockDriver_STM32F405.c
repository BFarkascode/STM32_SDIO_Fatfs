/*
 *  Created on: Oct 30, 2023
 *  Author: BalazsFarkas
 *  Project: STM32_Dictaphone
 *  Processor: STM32F405
 *  File: ClockDriver_STM32F405.c
 *  Program version: 1.3
 *  Change history:
 *
 *v.1.0
 * Below is a custom RCC configuration function, followed by the setup of TIM6 basic timer for time measurement and TIM2 for PWM.
 *
 *v.1.1
 *  Simplified version running only the delay function and the MCU clocking at 32 MHz
 *
 *v.1.2
 *  Clocking changed to 32 MHz with no AHB/APB1 and APB2 prescaling
 *  Clocking changed to F405
 *  File name changed
 *  Added TIM7 for press/push differentiation
 *
 *v.1.3
 *  Modified PLL by adding PLLQ definition - div 2
 *  Removed TIM7
 */

#include <ClockDriver_STM32F405.h>
#include "stm32f405xx.h"														//device specific header file for registers

//1)We set up the core clock and the peripheral prescalers/dividers
void SysClockConfig(void) {
	/*
	 * Similar to L0xx counterpart. See ClockDriver project.
	 * We will clock the MCU at 32 MHz to align well with the L053R8 test device
	 *
	 */

	//1)
	//HSI16 on, wait for ready flag
	RCC->CR |= (1<<0);															//we turn on HSI16
	while (!(RCC->CR & (1<<1)));												//and wait until it becomes stable. Bit 2 should be 1.


	//2)
	//power control enabled, put to reset value
	RCC->APB1RSTR |= (1<<28);													//reset PWR interface
	PWR->CR |= (1<<14);															//we put scale1
	PWR->CR |= (1<<15);
	while ((PWR->CSR & (1<<14)));												//and regulator voltage selection ready

	//3)
	//Flash access control register - 3WS latency
	FLASH->ACR |= (3<<0);														//3 WS

	//4)Setting up the clocks

	//AHB 1
	RCC->CFGR &= ~(1<<4);														//no prescale
	RCC->CFGR &= ~(1<<5);
	RCC->CFGR &= ~(1<<6);
	RCC->CFGR &= ~(1<<7);

	//APB1 divided by 1
//	RCC->CFGR |= (5<<10);														//we put 101 to bits [10:8]. This should be a DIV4.

	//APB2 divided by 1
//	RCC->CFGR |= (5<<13);

	RCC->PLLCFGR = 0;															//wipe the existing PLL setup

	//PLL source HSI16
	RCC->PLLCFGR &= ~(1<<22);

	//PLL "M" division is 10
	RCC->PLLCFGR |= (10<<0);

	//PLL "N" multiplier is 80
	RCC->PLLCFGR |= (80<<6);

	//PLL "P" division is 4
	RCC->PLLCFGR |= (1<<16);
	RCC->PLLCFGR &= ~(1<<17);

	//PLL "Q" division is 8
	RCC->PLLCFGR |= (8<<24);													//divided by 8
																				//Note: the main PLL has an output towards SDIO, RNG and the USB
																				//Note: PLLQ is parallel to the P output, so putting it to DIV4 will lead to a 32 MHz SDIOCK

	//5)Enable PLL
	//enable and ready flag for PLL
	RCC->CR |= (1<<24);															//we turn on the PLL
	while (!(RCC->CR & (1<<25)));												//and wait until it becomes available

	//6)Set PLL as system source
	RCC->CFGR |= (2<<0);														//PLL as source

	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);						//system clock status (set by hardware in bits [3:2]) should be matching the PLL source status set in bits [1:0]

	SystemCoreClockUpdate();													//This CMSIS function must be called to update the system clock! If not done, we will remain in the original clocking (likely MSI).
}

//2) TIM6 setup for precise delay generation
void TIM6Config (void) {
	/**
	 * What happens here?
	 * We first enable the timer, paying VERY close attention on which APB it is connected to (APB1).
	 * We then prescale the (automatically x2 multiplied!) APB clock to have a nice round frequency.
	 * Since we will play with how far this timer counts, we put the automatic maximum value of count value to maximum. This means that this timer can only count 65535 cycles.
	 * We tgeb enable the timers and wait until it is engaged.
	 *
	 * TIM6 is a basic clock that is configured to provide a counter for a simple delay function (see below).
	 * It is connected to AP1B which is clocking at 16 MHz currently (see above the clock config for the PLL setup to generate 32 MHz and then the APB1 clock divider left at DIV4 and a PCLK multiplier of 2 - not possible to change)
	 * 1)Enable TIM6 clocking
	 * 2)Set prescaler and ARR
	 * 3)Enable timer and wait for update flag
	 **/

	//1)
	RCC->APB1ENR |= (1<<4);														//enable TIM6 clocking

	//2)

	TIM6->PSC = 32 - 1;															// 32 MHz/32 = 1 MHz -- 1 us delay
																				// Note: the timer has a prescaler, but so does APB1!
																				// Note: the timer has a x2 multiplier on the APB clock
																				// Here APB1 PCLK is 8 MHz

	TIM6->ARR = 0xFFFF;															//Maximum ARR value - how far can the timer count?

	//3)
	TIM6->CR1 |= (1<<0);														//timer counter enable bit
	while(!(TIM6->SR & (1<<0)));												//wait for the register update flag - UIF update interrupt flag
																				//update the timer if we are overflown/underflow with the counter was reinitialised.
																				//This part is necessary since we can update on the fly. We just need to wait until we are done with a counting cycle and thus an update event has been generated.
																				//also, almost everything is preloaded before it takes effect
																				//update events can be disabled by writing to the UDIS bits in CR1. UDIS as LOW is UDIS ENABLED!!!s
}


//3) Delay function for microseconds
void Delay_us(int micro_sec) {
	/**
	 * Since we can use TIM6 only to count up to maximum 65535 cycles (65535 us), we need to up-scale out counter.
	 * We do that by counting first us seconds...
	 *
	 *
	 * 1)Reset counter for TIM6
	 * 2)Wait until micro_sec
	 **/
	TIM6->CNT = 0;
	while(TIM6->CNT < micro_sec);												//Note: this is a blocking timer counter!
}


//4) Delay function for milliseconds
void Delay_ms(int milli_sec) {
	/*
	 * ...and then, ms seconds.
	 * This function will be equivalent to HAL_Delay().
	 *
	 * */

	for (uint32_t i = 0; i < milli_sec; i++){
		Delay_us(1000);															//we call the custom microsecond delay for 1000 to generate a delay of 1 millisecond
	}
}
