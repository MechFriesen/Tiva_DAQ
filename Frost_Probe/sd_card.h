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
//
// Defines the size of the buffers that hold the path, or temporary data from
// the SD card.  There are two buffers allocated of this size.  The buffer size
// must be large enough to hold the longest expected full path name, including
// the file name, and a trailing null character.
//
//*****************************************************************************
#define PATH_BUF_SIZE           80

//*****************************************************************************
//
// Defines the size of the buffer that holds the command line.
//
//*****************************************************************************
#define CMD_BUF_SIZE            64

//*****************************************************************************
//
// This buffer holds the full path to the current working directory.  Initially
// it is root ("/").
//
//*****************************************************************************
static char g_pcCwdBuf[PATH_BUF_SIZE] = "/";

//*****************************************************************************
//
// A temporary data buffer used when manipulating file paths, or reading data
// from the SD card.
//
//*****************************************************************************
static char g_pcTmpBuf[PATH_BUF_SIZE];

//*****************************************************************************
//
// The buffer that holds the command line.
//
//*****************************************************************************
static char g_pcCmdBuf[CMD_BUF_SIZE];

//*****************************************************************************
//
// The following are data structures used by FatFs.
//
//*****************************************************************************
static FATFS g_sFatFs;
static DIR g_sDirObject;
static FILINFO g_sFileInfo;
static FIL g_sFileObject;

//*****************************************************************************
//
// A structure that holds a mapping between an FRESULT numerical code, and a
// string representation.  FRESULT codes are returned from the FatFs FAT file
// system driver.
//
//*****************************************************************************
typedef struct
{
    FRESULT iFResult;
    char *pcResultStr;
} tFResultString;

//*****************************************************************************
//
// A macro to make it easy to add result codes to the table.
//
//*****************************************************************************
#define FRESULT_ENTRY(f)        { (f), (#f) }



//*****************************************************************************
//
// A macro that holds the number of result codes.
//
//*****************************************************************************
#define NUM_FRESULT_CODES       (sizeof(g_psFResultStrings) /                 \
                                 sizeof(tFResultString))

//*****************************************************************************
//
// Graphics context used to show text on the CSTN display.
//
//*****************************************************************************
tContext g_sContext;

//// This is the table that holds the command names, implementing functions, and
//// brief description.
////
////*****************************************************************************
//tCmdLineEntry g_psCmdTable[] =
//{
//    { "help",   Cmd_help,   "Display list of commands" },
//    { "h",      Cmd_help,   "alias for help" },
//    { "?",      Cmd_help,   "alias for help" },
//    { "ls",     Cmd_ls,     "Display list of files" },
//    { "chdir",  Cmd_cd,     "Change directory" },
//    { "cd",     Cmd_cd,     "alias for chdir" },
//    { "pwd",    Cmd_pwd,    "Show current working directory" },
//    { "cat",    Cmd_cat,    "Show contents of a text file" },
//    { 0, 0, 0 }
//};

//*****************************************************************************
// Functions
extern const char * StringFromFResult(FRESULT iFResult);

// User commands via serial
extern int Cmd_ls(int argc, char *argv[]);
extern int Cmd_cd(int argc, char *argv[]);
extern int Cmd_pwd(int argc, char *argv[]);
extern int Cmd_cat(int argc, char *argv[]);
extern int Cmd_help(int argc, char *argv[]);
extern int SerialUI(void);
