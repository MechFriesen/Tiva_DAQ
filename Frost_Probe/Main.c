//*****************************************************************************
//
// Main.c - A program for logging data from an analog channel to an SD
//          card (PC via serial for debugging)

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/adc.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include <time.h>
#include "driverlib/fpu.h"
#include "driverlib/hibernate.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/rom.h"
#include "utils/ustdlib.h"
#include "inc/hw_hibernate.h"
#include "datetimeset.h"
#include "grlib/grlib.h"
#include "FP_acquire.h"
#include "sd_card.h"

// More in-depth description should go here.

uint64_t intRTCtime;
struct tm RealTime;     // time struct

// This function sets up UART0 to be used for a console to display information
// as the example is running.
void
InitConsole(void)
{
    //
    // Enable GPIO port A which is used for UART0 pins.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Configure the pin muxing for UART0 functions on port A0 and A1.
    // This step is not necessary if your part does not support pin muxing.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    //
    // Enable UART0 so that we can configure the clock.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    ROM_UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Select the alternate (UART) function for these pins.
    //
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}

bool
SysTimeSet(void)
{

    UARTprintf("\n\nSystem Date (DD/MM/YY) & Time (HH:MM:SS)\n");
    ulocaltime(HibernateRTCGet(), &RealTime);
    UARTprintf("%i/%i/%i %i:%02i:%02i\n", RealTime.tm_mday, (RealTime.tm_mon + 1),
               (RealTime.tm_year % 100), RealTime.tm_hour, RealTime.tm_min, RealTime.tm_sec);

    // Date change
    while(!DateSet(RealTime))
    {
    }

    // Time change
    while(!TimeSet(RealTime))
    {
    }

    return 1;
}

bool
RTCSetup(void)
{
    HibernateEnableExpClk(SysCtlClockGet());
    HibernateRTCEnable();
    return true;
}


// Configure ADC0 for a single-ended input and a single sample.  Once the
// sample is ready, an interrupt flag will be set.  Using a polling method,
// the data will be read then displayed on the console via UART0.
int
main(void)
{
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    ROM_FPULazyStackingEnable();

    // Set the system clock to run at 50 MHz (200 MHz from PLL / 4)
    SysCtlClockSet( SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

    // Set up the serial console to use for displaying messages. This is used
    // for setup and currently data output (SD card saving is WIP)
    InitConsole();

    // Enable the peripherals used by this example.
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);

    // Configure SysTick for a 100Hz interrupt.  The FatFs driver wants a 10 ms tick
    ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / 100);
    ROM_SysTickEnable();
    ROM_SysTickIntEnable();

    // Enable Interrupts
    ROM_IntMasterEnable();

    // Checks if the the wake-up is due to an RTC match.
    // If so, we go to the RTC handler in FP_acquire
    uint32_t ui32Status = HibernateIntStatus(0);

    if(ui32Status & 0x00000001)
    {
        while(!RTCHandler())
        {
        }
        UARTprintf("Goodnight...");
        HibernateRequest();
        while(1)    // loop until it falls asleep
        {
        }
    }

    // Setup the RTC for hibernate and acquisition time
    while(!RTCSetup())
    {
    }

    // Display the RTC time and let user set current time
    while(!SysTimeSet())
    {
    }

    while(!SerialUI())
    {
    }

    // Setup the ADC and RTC interrupts
    while(!AcquireSetup())
    {
    }

    while(!StartLogging())
    {
    }

}
