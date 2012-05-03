/**
display.c
tictoctrac.com
*/

#include "display.h"
#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "rtc.h"

#define threshold 250 // 250ms pulse

// Variables for the asynchronous time display
volatile unsigned char hr, min;
volatile unsigned char minCounter;
volatile unsigned char toggle;

ISR (TIMER0_COMPA_vect){ // Shows the current time asynchronously when enabled
	
	// Switches between showing the hour & showing the minute
	// This is necessary since sometimes the minute falls on an hour mark
	if (toggle&0x01){
		setHour(0); // clear hour
		if (toggle&0x02) setMinuteAbsolute(min); //show minute if needed (not off blink cycle)
		toggle&=~0x01;
	}else{
		setHour(hr);
		setMinute(0,0); // Clear minute to maintain brightness compared to hour
		toggle|=0x01;
	}

	minCounter++;

	if(minCounter>threshold){ // Causes the minute LED to blink at 2Hz
		minCounter=0;
		if (toggle & 0x02){
			setHour(0);		 // clr hour
			setMinute(0,0);  // clr minute
			toggle &= ~0x02; // Don't run the set minute code, clr hour instead
		}else{
			toggle |= 0x02;  // Reenable set minute code
		}
	}
}

void showTime(unsigned char hour, unsigned char minute){
	//set hour, min and turn on timer to show the time passed to it asynchronously
	hr = hour;
	min = minute;
	TIMSK0 = 2;
}

void hideTime(void){
	// Turn off the display
	TIMSK0 = 0;		// Turn off timer
	setHour(0);		// Clear hour
	setMinute(0,0); // Clear minute
}

void init_display(void){
	DDRD = 3;//all input except two of interest

	//Timer0 setup for 1ms
	TIMSK0 = 0;			//turn on timer 0 cmp match ISR, change to 2 when timer needed 
	OCR0A = 249;  		//set the compare reg to 250 time ticks
	TCCR0A = 0b00000010; // turn on clear-on-match
	TCCR0B = 0b00000011; // clock prescalar to 64
}

void setMinute(unsigned char hr, unsigned char min){
	// This function allows us to easily set the charliplexed leds
	// hr is 1-12 based and min is 1-4
	if ((min != 0)&&(hr != 0)) {
		if (hr == 12) hr = 0;
		unsigned char led = hr*4+(min-1); // Find out the charlieplex LED of interest (0-47)
		// Two outputs for the single charlieplex LED One is simple to compute
		// The other is based upon the remainder, but skipping the diagonal LED's in the matrix
		// Basically this assures the two output pins are unique
		DDRD  = (1<<(led/6)) | (1<<((led%6)+((led%6)>=(led/6))));
		PORTD = (1<<(led/6)); // Output High on the proper charlieplex line
	}else{ // Reset the display
		DDRD = 0;
		PORTD = 0;
	}
}

void setMinuteAbsolute(unsigned char min){
	// This function accepts min 0-59 to set the absolute minute even if it falls on an hour LED
	unsigned char hr = 0;
	if (min>59){ 				// Reset display if invalid min passed
		setMinute(0,0);
		setHour(0);
	}else{ 						// Valid minute, show it
		if (min%5 == 0){ 		// Minute is actually on an hour LED
			if (min == 0){ 		// At 12 o'clock
				hr = 12;
			}else{
				hr = min/5; 	// Calculate the hour
			}
			setHour(hr); 		// Show it
		}else{
			hr = min/5; 		// Calculate which hour
			if (hr==0) hr = 12; // Convert 0-11 to 1-12
			min = min%5; 		// How many minutes past the hough
			setMinute(hr, min); // Show it
		}
	}
}

void setHour(unsigned char hr){
	// This function sets the hour mark 
	if (hr != 0){
		if (hr == 12) hr = 0;
		DDRC = (1<<7)|(1<<6);    // SENSE_A, SENSE_B outputs
		DDRF = 0b11110011;       // Output lines
		PORTF = 0b11110011;      // All high during transition
		PORTC = (1<<(6+(hr%2))); // Alternates: sense_a high on evens
		PORTF =  ~(1<<(7-((hr/2)+2*((hr/2)>=4))));
	}else{ // Reset the hour mark
		PORTC = 0;
		PORTF = 0;
	}
}


void circle(void){ 
	// Makes a full circle once!
	for(unsigned char i=0;i<60;i++){ // Loop setting minutes 0-59
		setMinute(0,0); // clear face 
		setHour(0);
		setMinuteAbsolute(i); // show minute
		_delay_ms(10);
	}
	hideTime(); // Turn the display back off
}

void displayCurrentTime(void){ 
	// Load's the current time from the RTC, then displays it via timer
	loadTime();
	char hr = getHour();
	if (hr>12) hr-=12; 			// Convert from 24 hours to 12 hours
	if (hr==0) hr=12;  			// Change from 0-23 to 1-12
	showTime(hr, getMinute());  // Enable the asynchronous timer routine
}
