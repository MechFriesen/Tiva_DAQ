
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

extern struct tm RealTime;

typedef struct
{
    uint32_t Cookie;                // Cookie
    uint32_t MeasPeriod;            // inverse of sample rate
    uint32_t RTCMatchPeriod[6];     // seconds until next RTC match
    uint32_t SamplesPerDay;         // number of samples per day
    uint32_t MeasurementsPerSample; // number of measurements in a sample
    uint32_t RTCMatchCount;         // number of RTC matches
    uint32_t uiCRC;                 // Checksum
}
tCfgState;

#define STATE_COOKIE            0x0355AAC0

extern bool AcquireSetup(uint32_t TimerClkFreq);
extern bool StartLogging( void );
extern bool ADCSetup( void );
extern bool RTCHandler( void);
extern bool GetHibData( tCfgState *ConfigState);
extern bool SetHibData( tCfgState *ConfigState);
