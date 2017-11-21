
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

//extern struct

extern bool AcquireSetup(uint32_t TimerClkFreq);
extern void StartLogging( void );
extern bool ADCSetup( void );
