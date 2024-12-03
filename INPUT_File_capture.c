/*
 *  Created on: Nov 13, 2024
 *  Author: BalazsFarkas
 *  Project: STM32_SDIO_FATFS
 *  Processor: STM32F405RG
 *  Program version: 1.0
 *  Source file: INPUT_File_capture.c
 *  Change history:
 *
 */

/*
 * This particular code is identical to the dictaphone project's related source code.
 *
 */

#include <INPUT_File_capture.h>

FATFS fs;             //file system
FIL fil;              //file
FILINFO filinfo;
FRESULT fresult;
char char_buffer[100];
uint8_t hex_buffer[100];

UINT br, bw;

FATFS *pfs;
DWORD fre_clust;
uint32_t total, free_space;

//1)
void SDcard_start(void){

	/*
	 * Activate card
	 */

	  fresult = f_mount(&fs, "", 0);                   													//mount card
	  if (fresult != FR_OK) printf("Card mounting failed... \r\n");
	  else ("Card mounted... \r\n");

	  //--Capacity check--//
	  f_getfree("", &fre_clust, &pfs);

	  total = (uint32_t) ((pfs->n_fatent - 2) * pfs->csize * 0.5);
	  printf("SD card total size is: \r\n");
	  printf("%d",total);

	  free_space = (uint32_t) (fre_clust * pfs->csize * 0.5);
	  printf("SD card free space is: \r\n");
	  printf("%d",free_space);

}

//2)
void FILE_wav_create(void){

	/*
	 * We create a wav file by writing a header
	 */

	  uint8_t file_cnt = 0;

	  //NOTE: this part below checks the file names and see what the newest one should be called
	  //We start from the name "000.wav"

	  fresult = f_stat(INPUT_SIDE_file_name, &filinfo);													//we check if the file exists already
	  while(fresult != FR_NO_FILE){

		  file_cnt++;
		  INPUT_SIDE_file_name[2] = file_cnt%10 + '0';
		  INPUT_SIDE_file_name[1] = (file_cnt%100)/10 + '0';
		  INPUT_SIDE_file_name[0] = file_cnt/100 + '0';
		  fresult = f_stat(INPUT_SIDE_file_name, &filinfo);												//we check if the file exists already

	  }

	  //we create a 16-bit 22050 Hz sampling rate mono wav file
	  fresult = f_stat(INPUT_SIDE_file_name, &filinfo);													//we check if the file exists already
	  if(fresult == FR_NO_FILE){																		//if it does not exist

		  f_open(&fil, INPUT_SIDE_file_name, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
		  hex_buffer[0] = 0x52;		   															    	// RIFF signature
		  hex_buffer[1] = 0x49;
		  hex_buffer[2] = 0x46;
		  hex_buffer[3] = 0x46;

		  hex_buffer[4] = 0xBC;		   															    	// FileLength (Header + data chunk)
		  hex_buffer[5] = 0xC5;
		  hex_buffer[6] = 0x06;
		  hex_buffer[7] = 0x00;


		  hex_buffer[8] = 0x57;		   															    	//WAVE
		  hex_buffer[9] = 0x41;
		  hex_buffer[10] = 0x56;
		  hex_buffer[11] = 0x45;

		  hex_buffer[12] = 0x66;		   															    //fmt
		  hex_buffer[13] = 0x6D;
		  hex_buffer[14] = 0x74;
		  hex_buffer[15] = 0x20;

		  hex_buffer[16] = 0x10;	   															    	//fmtChunkSize
		  hex_buffer[17] = 0x00;
		  hex_buffer[18] = 0x00;
		  hex_buffer[19] = 0x00;

		  hex_buffer[20] = 0x01;	   															    	//formatTag (PCM)
		  hex_buffer[21] = 0x00;

		  hex_buffer[22] = 0x01;		   															    //channel number (mono)
		  hex_buffer[23] = 0x00;


		  hex_buffer[24] = 0x22;		   															    //sampling rate (22050)
		  hex_buffer[25] = 0x56;
		  hex_buffer[26] = 0x00;
		  hex_buffer[27] = 0x00;

		  hex_buffer[28] = 0x44;		   															    //byterate (44100 - because we have 16 bits PCM, so 1 byte comes in at 44 kHz)
		  hex_buffer[29] = 0xAC;
		  hex_buffer[30] = 0x00;
		  hex_buffer[31] = 0x00;

		  hex_buffer[32] = 0x02;		   															    //blockalign/block size (2 - because we have 2 bytes)
		  hex_buffer[33] = 0x00;

		  hex_buffer[34] = 0x10;		   															    //bitspersample (16)
		  hex_buffer[35] = 0x00;

		  hex_buffer[36] = 0x64;  		   															    //data
		  hex_buffer[37] = 0x61;
		  hex_buffer[38] = 0x74;
		  hex_buffer[39] = 0x61;

		  hex_buffer[40] = 0x00;		   															    //datachunkSize for 5 sec with the data width defined above (5*44100*2)
		  hex_buffer[41] = 0xB8;
		  hex_buffer[42] = 0x06;
		  hex_buffer[43] = 0x00;

		  f_write(&fil, hex_buffer, 44, &bw);
		  bufclear();

		  f_close(&fil);

	  }

}

//3)
void bufclear (void)                       																//wipe buffer
{

  for (int i = 0; i < 100; i++){

    char_buffer[i] = '\0';
    hex_buffer[i] = 0x00;

  }

}
