//piezo.c
#include <avr/io.h>
#include <util/delay.h>
#include "display.h"
#include <avr/interrupt.h>
#include "motor.h"
#include "piezo.h"

// Time interval to be considered a tap in milliseconds
#define tapBottom 50
#define tapTop 500

// Tap states
// Last tap was either the beginning of a potential double tap or the end of a double tap
#define start 0
#define doubletap 1

volatile unsigned int tapCounter; // Double tap timer
volatile unsigned char tapState = doubletap; // Double tap state

void tapCounterUpdate(void){ // Call this functions at 1ms to make sure double tap can be detected
	if(tapCounter<tapTop) tapCounter++;
}

void clearTap(void){ // Can be used to externally reset the double tap state
	tapState = doubletap;
	tapCounter = 0; 
}

unsigned char double_tap(void){ // Attempt to detect a double tap
	unsigned char done = 0;
	if(tapState != start){ // If already double tapped, reset to a start state
		tapState = start;
	} else if ((tapState == start)&&((tapCounter<tapTop)&&(tapCounter>tapBottom))){
		// A second tap occurred fast enough to be considered a double tap
		done = 1;
		tapState = doubletap;
	}
	tapCounter = 0; // Always reset the counter
	PCIFR = 1; // Make sure flag is cleared
	return done;
}

void init_piezo(void){
	PCICR |= 1<<PCIE0; // enable external pin change interrupts
	PCMSK0 |= 1<<PCINT7; // enable pin 7 (B7) for interrupts
}
