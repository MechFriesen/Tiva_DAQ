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
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "FP_acquire.h"


static volatile uint32_t RTCIntCounter = 0, TimerIntCounter;     // counts number of RTC interrupts for measurement scheduling
tCfgState ConfigState;


// Function to get parameters from Main
bool
AcquireSetup(uint32_t TimerClkFreq)
{
    uint32_t SampleFreq = 1,  ii, MaxInputLen = 10, SampleDuration = 1;
    struct tm tempSampleTime;           // temporary time structure for validity checks
    const char** endptr = 0;
    char uartBuf[10];

    UARTprintf("How many samples per day? (max 1440)\n");
    UARTgets(uartBuf, MaxInputLen);
    ConfigState.SamplesPerDay = ustrtoul(uartBuf, endptr, 10);

    ulocaltime( HibernateRTCGet(), &tempSampleTime);    // load values to tm struct
    for( ii = 1; ii <= ConfigState.SamplesPerDay; ii++)
    {
        UARTprintf("When (HH:MM) should sample %i be taken? \n", ii);
        UARTgets(uartBuf, MaxInputLen);
        char* token = strtok(uartBuf, ":");
        int hour = ustrtoul( token, endptr, 10);
        tempSampleTime.tm_hour = hour;
        token = strtok(NULL, ":");
        int min = ustrtoul( token, endptr, 10);
        tempSampleTime.tm_min = min;

        UARTprintf("Sample Time: %i:%02i\n", tempSampleTime.tm_hour,
                   tempSampleTime.tm_min);   // for debugging
        if( umktime( &tempSampleTime) == -1)
        {
            UARTprintf("Invalid time\n");
            ii--;
        }
        else
        {
            if( ii == 1)
            {
                ConfigState.RTCMatchPeriod[ii] = hour*3600 + min*60;      // time from midnight in seconds
            }
            else
            {
                // time from last sample
                ConfigState.RTCMatchPeriod[ii] = hour*3600 + min*60 - ConfigState.RTCMatchPeriod[ii-1];
            }
        }
    }

    // Determine sample rate
    UARTprintf("What sample rate (Hz) do you want?\n");
    UARTgets( uartBuf, MaxInputLen);
    SampleFreq = ustrtoul( uartBuf, endptr, 10);

    // MeasFreq is in Hz. Converts to clock ticks.
    // 1 clock tick = 1/12500000 s
    ConfigState.MeasPeriod = TimerClkFreq / SampleFreq;

    // Determine sample duration
    UARTprintf("Duration (sec) of sampling?\n");
    UARTgets( uartBuf, MaxInputLen);
    SampleDuration = ustrtoul( uartBuf, endptr, 10);
    ConfigState.MeasurementsPerSample = SampleDuration / ConfigState.MeasPeriod;

    // set up the RTC interrupt shit.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);       // Enable hibernate module
    HibernateEnableExpClk(SysCtlClockGet());
    HibernateRTCTrimSet(0x7FFF);        // Apparently needed per silicon erratum 2.1
    HibernateRTCEnable();               // Set the RTC running
//    SetSavedState(&g_sConfigState);
    HibernateWakeSet(HIBERNATE_WAKE_PIN | HIBERNATE_WAKE_RTC);

    // Set first match
    uint32_t FirstMatch = umktime( &tempSampleTime);
    ii = 0;
    while(FirstMatch < HibernateRTCGet())
    {
        FirstMatch += ConfigState.RTCMatchPeriod[ii];
        ii++;
    }


    UARTprintf("Firstmatch: %i\n", FirstMatch);
    HibernateRTCMatchSet(0, FirstMatch);


    return true;
}

// Start logging
void
StartLogging()
{
    char uartBuf[10];

    UARTprintf("To begin logging press ENTER\n");
    UARTgets(uartBuf, 2);

    // Save the configuration to Hib module and then lights out
//    SetSavedState(&g_sConfigState);     // gotta figure out this part still
    HibernateRequest();
    while(1)    // loop until it falls asleep
    {
    }
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
    // to sequence 3.  This example is arbitrarily using sequence 0.
    //
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);

    //
    // Configure step 0 on sequence 0.  Sample channel 0 (ADC_CTL_CH0) in
    // single-ended mode (default) and configure the interrupt flag
    // (ADC_CTL_IE) to be set when the sample is done.  Tell the ADC logic
    // that this is the last conversion on sequence 3 (ADC_CTL_END).  Sequence
    // 3 has only one programmable step.  Sequence 1 and 2 have 4 steps, and
    // sequence 0 has 8 programmable steps.  Since we are only doing a single
    // conversion using sequence 3 we will only configure step 0.  For more
    // information on the ADC sequences and steps, reference the datasheet.
    //

//    ROM_ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH1 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH2 | ADC_CTL_IE);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 3, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END);

    //
    // Since sample sequence 0 is now configured, it must be enabled.
    //
    ADCSequenceEnable(ADC0_BASE, 0);

    //
    // Clear the interrupt status flag.  This is done to make sure the
    // interrupt flag is cleared before we sample.
    //
    ADCIntClear(ADC0_BASE, 0);

    return 1;

}


// This function is called when the timer sends an interrupt.
void
TimerHandler(void)
{
    struct tm TimeStamp;            // holds RTC timestamp seconds
    uint32_t TimeStampSS;           // holds RTC timestamp subseconds
    uint32_t pui32ADC0Value[8];     // buffer for ADC data

//    uint8_t interruptStatus;

    TimerIntCounter++;

    ADCProcessorTrigger(ADC0_BASE, 0);  // Trigger the ADC conversion.

    // Wait for conversion to be completed.
    while(!ADCIntStatus(ADC0_BASE, 0, false))
    {
    }

    ADCIntClear(ADC0_BASE, 0);  // Clear the ADC interrupt flag.

    ADCSequenceDataGet(ADC0_BASE, 0, pui32ADC0Value);       // Read ADC Value.

    ulocaltime(HibernateRTCGet(), &TimeStamp);           // gets time from RTC and stores in a struct
    TimeStampSS = HibernateRTCSSGet() * 1000 / 32768;  // gets subsecond value from RTC in ms

    // Display timestamp
    // possibly get rid of the minutes and seconds part for faster operation
    UARTprintf("%i:%i.%i,",  TimeStamp.tm_min, TimeStamp.tm_sec, TimeStampSS);
    // Display digital values for CH0 - CH3
    UARTprintf("%4d, %4d, %4d, %4d\n", pui32ADC0Value[0], pui32ADC0Value[1],
               pui32ADC0Value[2], pui32ADC0Value[3]);

    if( TimerIntCounter > ConfigState.MeasurementsPerSample)
    {
        IntDisable(INT_TIMER0A);
    }
}

void
RTCHandler(void)
{
    uint32_t ui32Status, RTCWakeTime;

    ui32Status = HibernateIntStatus(1);
    HibernateIntClear(ui32Status);

    RTCWakeTime = HibernateRTCGet();

    // Set up the display again


    // Set up the ADC
    while(!ADCSetup());

    // Set up the Timer interrupt shit
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);       // Enable timer 0
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);    // Configure as full-width
    TimerLoadSet( TIMER0_BASE, TIMER_A, ConfigState.MeasPeriod);    // sets the timer interrupt period
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);


    ConfigState.RTCMatchCount++;    // Count which sample this is
    if(RTCIntCounter <= ConfigState.SamplesPerDay)
    {
        HibernateRTCMatchSet(0, (ConfigState.RTCMatchPeriod[ConfigState.RTCMatchCount] + RTCWakeTime));
    }
    else
    {
        // something to handle roll-over to the next day
    }
}
