//*****************************************************************************
//
// Main.c - A program for logging data from an analog channel to an SD
//          card (PC via serial for debugging)

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
//#include <time.h>
#include "FP_acquire.h"
#include "sd_card.h"
#include "inc/hw_hibernate.h"
#include "inc/hw_memmap.h"
#include "driverlib/adc.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "drivers/cfal96x64x16.h"
#include "utils/uartstdio.h"
//#include "utils/ustdlib.h"
//#include "datetimeset.h"
#include "grlib/grlib.h"


// More in-depth description should go here.

uint64_t intRTCtime;

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
RTCSetup(void)
{
    HibernateEnableExpClk(SysCtlClockGet());
    HibernateRTCEnable();
    return true;
}

// Displays the parameters for the serial connection on the on-board display
void
BrdConnConfigDisplay(void)
{
    tRectangle sRect;

    // Initialize the display driver.
    CFAL96x64x16Init();

    // Initialize the graphics context.
    GrContextInit(&g_sContext, &g_sCFAL96x64x16);

    // Fill the top part of the screen with blue to create the banner.
    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&g_sContext) - 1;
    sRect.i16YMax = 9;
    GrContextForegroundSet(&g_sContext, ClrDarkBlue);
    GrRectFill(&g_sContext, &sRect);

    // Change foreground for white text.
    GrContextForegroundSet(&g_sContext, ClrWhite);

    // Put the application name in the middle of the banner.
    GrContextFontSet(&g_sContext, g_psFontFixed6x8);
    GrStringDrawCentered(&g_sContext, "Logger", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 4, 0);

    // Show some instructions on the display
    GrContextFontSet(&g_sContext, g_psFontFixed6x8);
    GrStringDrawCentered(&g_sContext, "Connect a", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 20, false);
    GrStringDrawCentered(&g_sContext, "terminal", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 30, false);
    GrStringDrawCentered(&g_sContext, "to UART0.", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 40, false);
    GrStringDrawCentered(&g_sContext, "115200,N,8,1", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 50, false);
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

    BrdConnConfigDisplay(); // Print serial connection parameters to on-board display

    // Setup the RTC for hibernate and acquisition time
    while(!RTCSetup())
    {
    }

    // Serial user interface for navigating SD card files and configuring sampling
    while(!SerialUI())
    {
    }

    // for debugging
    while(!RTCHandler())
    {
    }
    while(!SerialUI())
    {
    }

    // Ok, this isn't for debugging
    while(!StartLogging())
    {
    }

}
