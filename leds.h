/* 
 * File:   Leds.h
 * Author: amshunda
 * lab 6
 * 
 */

#ifndef LEDS_H
#define	LEDS_H


#include <stdio.h>
#include "BOARD.h"
#include <xc.h>


/*Macros*/
//Sets TRISE and LATE registers to 0
#define LEDS_INIT() do{ \
    TRISE = 0x00; \
    LATE = 0x00; \
    } while(0)

//set latch register value to x     
#define LEDS_SET(x)  do{ \
LATE = (x); \
} while(0)


//Return value of latch register
#define LEDS_GET()  LATE









#endif	/* LEDS_H */


