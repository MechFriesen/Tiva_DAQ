/*
 * acquire.c
 *
 *  Created on: Nov 15, 2017
 *      Author: Dirk
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "driverlib/adc.h"
#include "driverlib/debug.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/sw_crc.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "FP_acquire.h"


static volatile uint32_t RTCIntCounter = 0, TimerIntCounter = 0;     // counts number of RTC interrupts for measurement scheduling
tCfgState ConfigState;

// This function is called when the timer sends an interrupt.
void
Timer0Handler(void)
{
    uint32_t pui32ADC0Value[4];     // buffer for ADC data

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Disable timer interrupts while the measurement is handled
    IntDisable(INT_TIMER0A);

    //  Trigger the ADC conversion.
    ADCProcessorTrigger(ADC0_BASE, 1);

    // Wait for conversion to be completed.
    while(!ADCIntStatus(ADC0_BASE, 1, false))
    {
    }

    ADCIntClear(ADC0_BASE, 1);  // Clear the ADC interrupt flag.

    TimerIntCounter++;
    ADCSequenceDataGet(ADC0_BASE, 1, &pui32ADC0Value);       // Read ADC Value.

    //Output data to serial console
    UARTprintf("%5i, %05i, %4i", TimerIntCounter, HibernateRTCSSGet(), pui32ADC0Value[1]); // Output data to Serial

    if( TimerIntCounter < ConfigState.MeasurementsPerSample)
    {
        IntEnable(INT_TIMER0A);
    }
}

bool
RTCHandler(void)
{
    uint32_t ui32Status, RTCWakeTime, NextMatch;
    struct tm tempDayRollover;
    // Clear interrupts
    ui32Status = HibernateIntStatus(1);
    HibernateIntClear(ui32Status);

    // Retrieve settings from hibernation memory
    while(!GetHibData( &ConfigState ))
    {
    }

    // Get the time
    RTCWakeTime = HibernateRTCGet();

    // Set up the ADC
    while(!ADCSetup())
    {
    }

    UARTprintf("Sample Number, RTC subseconds [1/32768 s], CH0\n");

    // Set up the Timer interrupt shit
    TimerIntCounter = 0;
    IntRegister( INT_TIMER0A, Timer0Handler);           // Set Timer0Handler to handle timer interrupts
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);       // Enable timer 0
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);    // Configure as full-width
    TimerClockSourceSet( TIMER0_BASE, TIMER_CLOCK_SYSTEM);  // Sets the clock for the module to use
    TimerLoadSet( TIMER0_BASE, TIMER_A, ConfigState.MeasPeriod);    // sets the timer interrupt period
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);    // Enable the interrupt generation
    IntEnable(INT_TIMER0A);     // Enable the interrupt handling
    TimerEnable(TIMER0_BASE, TIMER_A);      // Start the timer

    while(TimerIntCounter != ConfigState.MeasurementsPerSample)
    {
        // hang here until the required measurements are taken
    }

    UARTprintf("Finished the measurements for this sample.\n");

    // Set the RTC time for the next sample to be taken
    ConfigState.RTCMatchCount++;    // Count which sample this is
    if(ConfigState.RTCMatchCount <= ConfigState.SamplesPerDay)
    {
        NextMatch = ConfigState.RTCMatchPeriod[ConfigState.RTCMatchCount] + RTCWakeTime;
        HibernateRTCMatchSet(0, NextMatch);

        // Debugging output
        UARTprintf("Next Match (match %i): %i\n", ConfigState.RTCMatchCount, NextMatch);
    }
    else
    {
        // something to handle roll-over to the next day
        ulocaltime( RTCWakeTime, &tempDayRollover);
        tempDayRollover.tm_yday++;
        (umktime( &tempDayRollover ) != -1) ? NextMatch = umktime( &tempDayRollover )
                : tempDayRollover.tm_yday++ ;
        NextMatch = umktime( &tempDayRollover );

        // for debugging
        UARTprintf("Next Match (match %i): %i\n", ConfigState.RTCMatchCount, NextMatch);
    }

    return(true);
}

// Function to get parameters from user
bool
AcquireSetup()
{
    uint32_t SampleFreq = 1,  ii, MaxInputLen = 10, FirstMatch = 0;
    uint32_t TimerClkFreq = SysCtlClockGet();
    float SampleDuration = 1;
    struct tm tempSampleTime;   // temporary time structure for validity checks
    const char** endptr = ' ';  // useless little pointer that ustrtoul needs to feel good about itself
    char uartBuf[10] = {"\0"};  // Buffer for text from serial

    UARTprintf("How many samples per day? (max 12)\n");
    UARTgets(uartBuf, MaxInputLen);
//    ConfigState.SamplesPerDay = 0;
    ConfigState.SamplesPerDay = ustrtoul(uartBuf, endptr, 10);
    UARTprintf("SamplesPerDay: %i\n", ConfigState.SamplesPerDay);

    ulocaltime( HibernateRTCGet(), &tempSampleTime);    // load values to tm struct
    for( ii = 1; ii <= ConfigState.SamplesPerDay; ii++)
    {
        UARTprintf("\nWhen (HH:MM) should sample %i be taken? \n", ii);
        UARTgets(uartBuf, MaxInputLen);
        char* token = strtok(uartBuf, ":");
        int hour = ustrtoul( token, endptr, 10) % 24;
        tempSampleTime.tm_hour = hour;
        token = strtok(NULL, ":");
        int min = ustrtoul( token, endptr, 10);
        tempSampleTime.tm_min = min;
        tempSampleTime.tm_sec = 0;          // Samples start on the minute

        if( umktime( &tempSampleTime) == -1 )
        {
            UARTprintf("Invalid time\n");
            ii--;
        }
        else
        {
            ConfigState.RTCMatchPeriod[ii] = hour*60 + min;      // time from midnight in minutes

            // for debugging
            UARTprintf("RTCMatchPeriod %i: %i\n", ii, ConfigState.RTCMatchPeriod[ii]);
        }
    }

    // Determine sample rate
    UARTprintf("\nWhat sample rate (Hz) do you want?\n");
    UARTgets( uartBuf, MaxInputLen);
    SampleFreq = ustrtoul( uartBuf, endptr, 10);

    // MeasFreq is in Hz. Converts to clock ticks.
    // 1 clock tick = 1/12500000 s
    ConfigState.MeasPeriod = TimerClkFreq / SampleFreq;

    // Determine sample duration
    UARTprintf("\nDuration (sec) of sampling?\n");
    UARTgets( uartBuf, MaxInputLen);
    SampleDuration = ustrtof( uartBuf, endptr);
    ConfigState.MeasurementsPerSample = (SampleDuration * TimerClkFreq) / ConfigState.MeasPeriod;

    // set up the RTC interrupt shit.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);       // Enable hibernate module
    HibernateEnableExpClk(SysCtlClockGet());
    HibernateRTCTrimSet(0x7FFF);        // Apparently needed per silicon erratum 2.1
    HibernateRTCEnable();               // Set the RTC running
//    SetSavedState(&g_sConfigState);
    HibernateWakeSet(HIBERNATE_WAKE_PIN | HIBERNATE_WAKE_RTC);
//    HibernateIntRegister(RTCHandler);

    // Set first RTC match
    ii = 0;
    while(FirstMatch < HibernateRTCGet() && ii < ConfigState.SamplesPerDay)
    {
        FirstMatch = RTCMatchGenerate( ConfigState.RTCMatchPeriod[ii]);
        ii++;
    }


    UARTprintf("\nMeasPeriod: %i\n", ConfigState.MeasPeriod);
    UARTprintf("Measurements Per Sample: %i\n", ConfigState.MeasurementsPerSample);
    UARTprintf("Firstmatch: %i\n", FirstMatch);

    HibernateRTCMatchSet(0, FirstMatch);

    // Save the configuration to Hib module
    while(!SetHibData( &ConfigState ))
    {
    }

    return true;
}

// Generates an RTC match value given minutes from midnight
uint32_t
RTCMatchGenerate( uint16_t MinutesSinceMidnight)
{
    struct tm MatchTime;
    uint32_t PresentTime = HibernateRTCGet();
    int32_t MatchSeconds;
    ulocaltime( PresentTime, &MatchTime);
    MatchTime.tm_hour = MinutesSinceMidnight / 60;
    MatchTime.tm_min = MinutesSinceMidnight % 60;
    MatchTime.tm_sec = 0;
    MatchSeconds = umktime( &MatchTime);
    if( MatchSeconds == -1 )
    {
        UARTprintf("Invalid RTC match time\n");
        return(-1);
    }
    else
    {
        return MatchSeconds;
    }
}

// Start logging
bool
StartLogging()
{
    char uartBuf[10];

    UARTprintf("To begin logging press ENTER\n");
    UARTgets(uartBuf, 2);

    HibernateRequest();
    while(1)    // loop until it falls asleep
    {
    }
    return true;    // Ha! Not possible. I'm sleeping now.
}

bool
ADCSetup()
{
    //
    // The ADC0 peripheral must be enabled for use.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

    //
    // For this example ADC0 is used with AIN0 on port E7.
    // The actual port and pins used may be different on your part, consult
    // the data sheet for more information.  GPIO port E needs to be enabled
    // so these pins can be used.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    //
    // Select the analog ADC function for these pins.
    // Consult the data sheet to see which functions are allocated per pin.
    //
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1
                   | GPIO_PIN_0);

    //
    // Enable sample sequence 0 with a processor signal trigger.  Sequence 0
    // will do a single sample when the processor sends a signal to start the
    // conversion.  Each ADC module has 4 programmable sequences, sequence 0
    // to sequence 3.  This example is arbitrarily using sequence 1.
    //
    ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);

    // Configure steps 1-4 on sequence 1.  Sample channel 0 (ADC_CTL_CH0) in
    // single-ended mode (default) and configure the interrupt flag
    // (ADC_CTL_IE) to be set when the measurement is done.  Tell the ADC logic
    // that this is the last conversion on sequence 1 (ADC_CTL_END).

    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_CH1);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CTL_CH2);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 3, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END);

    //
    // Since sample sequence 0 is now configured, it must be enabled.
    //
    ADCSequenceEnable(ADC0_BASE, 1);

    //
    // Clear the interrupt status flag.  This is done to make sure the
    // interrupt flag is cleared before we sample.
    //
    ADCIntClear(ADC0_BASE, 1);

    return 1;

}

bool
GetHibData( tCfgState* ConfigState)
{
    uint32_t CfgStateLen;
    uint16_t ui16Crc16;

    //
    // Check the arguments
    //
    ASSERT(ConfigState);
    if(!ConfigState)
    {
        return(false);
    }

    // Initialize locals.
    CfgStateLen = sizeof(tCfgState) / 4;

    // Read a block from hibernation memory into the application state
    // structure.
    HibernateDataGet((uint32_t *) ConfigState, CfgStateLen);

    // Check first to see if the "cookie" value is correct.
    if(ConfigState->Cookie != STATE_COOKIE)
    {
        return(false);
    }

    // Find the 16-bit CRC of the block.  The CRC is stored in the last
    // location, so subtract 1 word from the count.
    ui16Crc16 = Crc16Array(CfgStateLen - 1, (const uint32_t *)ConfigState);

    // If the CRC does not match, then the block is not good.
    if(ConfigState->uiCRC != (uint32_t)ui16Crc16)
    {
        return(false);
    }

    // At this point the state structure that was retrieved from the
    // battery backed memory has been validated, so return it as a valid
    // logger state configuration.
    return(true);
}

bool
SetHibData( tCfgState* ConfigState)
{
    uint32_t CfgStateLen;
    uint16_t ui16Crc16;

    //
    // Check the arguments
    //
    ASSERT(ConfigState);

    // Initialize locals.
    CfgStateLen = sizeof(tCfgState) / 4;

    if(ConfigState)
    {
        //
        // Write the cookie value to the block
        //
        ConfigState->Cookie = STATE_COOKIE;

        //
        // Find the 16-bit CRC of the block.  The CRC is stored in the last
        // location, so subtract 1 word from the count.
        //
        ui16Crc16 = Crc16Array(CfgStateLen - 1,
                                   (const uint32_t *)ConfigState);

        //
        // Save the computed CRC into the structure.
        //
        ConfigState->uiCRC = (uint32_t)ui16Crc16;

        //
        // Now write the entire block to the Hibernate memory.
        //
        HibernateDataSet((uint32_t *)ConfigState, CfgStateLen);

        // We saved it (to the hibernate module)!
        return(true);

    }
    else
    {
        return(false);
    }
}
