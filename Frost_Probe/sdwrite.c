/*
 * sdwrite.c
 *
 *  Created on: Dec 2, 2017
 *      Author: Nina
 */

// I still don't know what I'm doing but first here's the code from that place that I copy/pasted:

//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "grlib/grlib.h"
#include "utils/cmdline.h"
#include "utils/uartstdio.h"
#include "fatfs/src/ff.h"
#include "fatfs/src/diskio.h"
#include "drivers/cfal96x64x16.h"

//*****************************************************************************

int WriteFile(char *file, char *data)
{

unsigned short usBytesWrite;

//
// Open the file for create new text file.
//

if (f_open(&g_sFileObject, file, FA_CREATE_NEW | FA_WRITE)==FR_OK)
{
//
// write file
//
if(f_write(&g_sFileObject,data,strlen(data),&usBytesWrite)==FR_OK)
{
UARTprintf("Write file successn");
f_sync(&g_sFileObject);
}
else
{
UARTprintf("Write into file Errorrn");
}
}
else
{
// if the file exists
if (f_open(&g_sFileObject, file, FA_CREATE_NEW | FA_WRITE)==FR_EXIST)

{

if (f_open(&g_sFileObject, file, FA_OPEN_EXISTING | FA_WRITE)==FR_OK)
{
//
// write file
//
if(f_lseek(&g_sFileObject, g_sFileObject.fsize)==FR_OK)
{
if(f_write(&g_sFileObject,data,strlen(data),&usBytesWrite)==FR_OK)
{
UARTprintf("File append successn");
f_close(&g_sFileObject);
}
else
{
UARTprintf("Write into file Errorrn");
}
}
}
else
{
UARTprintf("Append File ceation Error, file not foundn");
}

}
else
UARTprintf("New File ceation Errorn");
}

return(0);
}

