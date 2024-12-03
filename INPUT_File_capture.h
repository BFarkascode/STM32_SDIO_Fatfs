/*
 *  Created on: Nov 13, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO
 *  Processor: STM32F405RG
 *  Program version: 1.0
 *  Header file: INPUT_File_capture_SDIO.h
 *  Change history:
 */

#ifndef INC_SDCARD_IMAGE_CAPTURE_H_
#define INC_SDCARD_IMAGE_CAPTURE_H_

#include <SDcard_SDIO_diskio.h>
#include "stdlib.h"
#include "ff.h"

//LOCAL CONSTANT

//LOCAL VARIABLE

//EXTERNAL VARIABLE

extern char INPUT_SIDE_file_name[];

//FUNCTION PROTOTYPES

void SDcard_start(void);

void FILE_wav_create(void);

#endif /* INC_SDCARD_IMAGE_CAPTURE_H_ */
