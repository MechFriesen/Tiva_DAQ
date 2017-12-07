
// datetimeset.h - Prototypes and definitions for application menus.

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

#include "fatfs/src/ff.h"
#include "fatfs/src/diskio.h"

#define LOG_PATH_BUF_SIZE   18

typedef struct
{
    uint32_t Cookie;                        // Cookie
    char     logFilePath[LOG_PATH_BUF_SIZE];    // File path to where logged data is saved
    uint32_t MeasPeriod;                    // inverse of sample rate
    uint16_t RTCMatchPeriod[12];            // seconds until next RTC match
    uint16_t SamplesPerDay;                 // number of samples per day
    uint16_t MeasurementsPerSample;         // number of measurements in a sample
    uint16_t RTCMatchCount;                 // number of RTC matches
    uint32_t uiCRC;                         // Checksum
}
tCfgState;

#define STATE_COOKIE            0x0355AAC0

extern bool AcquireSetup( void );
extern uint32_t RTCMatchGenerate( uint16_t MinutesSinceMidnight );
extern bool StartLogging( void );
extern bool ADCSetup( void );
extern void LogDirectorySet( char *LogDirBuf );
extern bool RTCHandler( void);
extern bool GetHibData( tCfgState *ConfigState );
extern bool SetHibData( tCfgState *ConfigState );
