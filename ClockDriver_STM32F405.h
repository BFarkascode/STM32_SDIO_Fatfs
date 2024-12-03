/*
 *  Created on: Oct 30, 2023
 *  Author: BalazsFarkas
 *  Project: STM32_Dictaphone
 *  Processor: STM32F405
 *  File: ClockDriver_STM32F405.h
 *  H2ader version: 1.3
 */

#ifndef INC_RCCTIMPWMDELAY_CUSTOM_H_
#define INC_RCCTIMPWMDELAY_CUSTOM_H_

#include "stdint.h"

//LOCAL CONSTANT

//LOCAL VARIABLE

//EXTERNAL VARIABLE

//FUNCTION PROTOTYPES
void SysClockConfig(void);
void TIM6Config (void);
void Delay_us(int micro_sec);
void Delay_ms(int milli_sec);

#endif /* RCCTIMPWMDELAY_CUSTOM_H_ */
