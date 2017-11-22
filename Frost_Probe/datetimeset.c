/*
 * TimeSet.c
 *
 *  Created on: Nov 14, 2017
 *      Author: Dirk
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "datetimeset.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "driverlib/hibernate.h"

//uint32_t MaxInputLen = 10;
//char uartBuf[10];
//const char *endptr;

// Date change
bool
DateSet(struct tm RealTime)
{


    uint32_t MaxInputLen = 10;
    const char** endptr = 0;

    DateQuery:      // if any of the entered values are illogical then the following block starts over here
    UARTprintf("To change date, enter the new date (DD/MM/YY) or press ENTER"
            " to skip.\n");
    char uartIN[10];
    UARTgets(uartIN, MaxInputLen);

    if(ustrtoul( uartIN, endptr, 10))
    {
        char* token = strtok(uartIN, "/");
        RealTime.tm_mday = ustrtoul( token, endptr, 10);        // day of the month
        token = strtok(NULL, "/");
        RealTime.tm_mon = ustrtoul( token, endptr, 10) - 1;     // months since Jan
        token = strtok(NULL, "/");
        RealTime.tm_year = ustrtoul( token, endptr, 10) + 100;  // years since 1900
        token = strtok(NULL, "/");
        if( umktime(&RealTime) == -1)
        {
            UARTprintf("Invalid date\n");
            goto DateQuery;
        }
        HibernateRTCSet(umktime(&RealTime));
        ulocaltime(HibernateRTCGet(), &RealTime);
        UARTprintf("New date: %i/%i/%i\n", RealTime.tm_mday, (RealTime.tm_mon + 1),
                       (RealTime.tm_year % 100));
    }
    return 1;
}

// Time change
bool
TimeSet(struct tm RealTime)
{
    uint32_t MaxInputLen = 10;
    char uartBuf[10];
    const char** endptr = 0;

    TimeQuery:      // returns here if time entered is invalid
    UARTprintf("To change time, enter the new time (HH:MM:SS) or press ENTER"
                    " to skip.\n");
    UARTgets(uartBuf, MaxInputLen);


    if(ustrtoul(uartBuf, endptr, 10))
    {

        char *token2 = strtok(uartBuf, ":");
        RealTime.tm_hour = ustrtoul( token2, endptr, 10);
        token2 = strtok(NULL, ":");
        RealTime.tm_min = ustrtoul( token2, endptr, 10);
        token2 = strtok(NULL, ":");
        RealTime.tm_sec = ustrtoul( token2, endptr, 10);
        token2 = strtok(NULL, ":");
        HibernateRTCSet(umktime(&RealTime));
        if( HibernateRTCGet() == -1)
                {
                    UARTprintf("Invalid time\n");
                    goto TimeQuery;
                }
        ulocaltime(HibernateRTCGet(), &RealTime);
        UARTprintf("New time: %i:%02i:%02i\n", RealTime.tm_hour, RealTime.tm_min,
                           RealTime.tm_sec);
    }

    return 1;
}
