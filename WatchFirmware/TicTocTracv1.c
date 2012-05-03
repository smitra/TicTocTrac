#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "display.h"
#include "motor.h"
#include "piezo.h"
#include "rtc.h"
#include "sd.h"
#include "diskio.h"

#define timeout 2000 // Timeout for display in ms

volatile unsigned char fatCounter = 0; 	// Used to run the timerproc function for fatfs every 10ms
volatile unsigned int intervalMode = 0; // Whether we are currently measuring an interval test
volatile unsigned char alarmFlag = 0;	// Flag indicating an alarm has gone off on the RTC
volatile unsigned char tapFlag = 0;		// Flag indicating a double tap has occurred
volatile unsigned int timeOutCounter;	// Used to run the display timeout when necessary

unsigned int intervalStart;		// Stores when the interval test began in seconds since last hour
unsigned int expectedInterval;	// Stores how long the interval should be in seconds
unsigned int maxInterval;		// Max allowed interval to test for
unsigned int minInterval;		// Min allowed interval to test for

// Values for PsuedoNumber generator
#define bit30 0x4000
#define bit27 0x0800

// Variables for PsuedoNumber generator
char bit0, bit1;
unsigned long noise_gen = 0b10111001010101010110100001;// Initial seed

ISR(TIMER1_COMPA_vect){ // 1ms timer for running processes
	tapCounterUpdate();	// Let the piezo code know a ms has passed
	fatCounter++;
	if(fatCounter>10){
		disk_timerproc(); // Run every 10ms
		fatCounter = 0;
	}
	timeOutCounter++; // Always increment the timeout
}

// Alarm interrupt
ISR (PCINT0_vect) {
	//Check that it was an alarm interrupt (input high aka pullup) and currently low
	if (checkAlarm()){
		alarmFlag = 1;
	}else if(double_tap()){
		// Flag a double tap
		tapFlag = 1;
	}	
}

void clearTapFlag(void){ // Used to clear the flag and reset the tap code
	tapFlag = 0;
	clearTap();
}

//Random Number generator
unsigned char random_num(void){
	bit0 = (noise_gen & bit27)>0;
	bit1 = (noise_gen & bit30)>0;
	noise_gen <<= 1;
	noise_gen += (bit0 ^ bit1);
	return (char)(noise_gen & 0x7F);
}

int main(void){
	// Make sure watch dog timer is disabled
	MCUSR &= ~(1 << WDRF); 
   	wdt_disable();
	// Init libraries
	init_display();
	init_motor();
	init_piezo();
	initRTC();
	// 1ms process timer
	TIMSK1 = 2;				//turn on timer 0 cmp match ISR 
	OCR1A = 249;  			//set the compare reg to 250 time ticks
	TCCR1A = 0b00000010; 	// turn on clear-on-match
	TCCR1B = 0b00000011;	// clock prescalar to 64
	// Setup sleep mode and interrupts
	set_sleep_mode(SLEEP_MODE_PWR_DOWN); // setup power down sleep mode
	sei();
	sleep_enable();
	// Indicate program execution has begun (also useful during bootloading to indicated different programs)
	setHour(12);
	_delay_ms(2000);
	hideTime();
	while(1){
		hideTime(); 		// Make sure display is off before sleeping
		if (!tapFlag){
			sleep_cpu(); 	// Wait at this instruction for external int, pin change, RTC alarm
		}
		if(alarmFlag){ 			// Came out of sleep because of alarm
			alarmFlag = 0;
			alarmHandled();		// First tell the RTC it is being handled
			intervalMode = 0; 	// Void the current time interval estimate
			hideTime();
			// Record failure to respond within 60min
			char test[] = "timeFail\0";
			writeResult(test, 1);
			for(int i=0;i<2;i++){ // Alert the User to the reset
				vibr(1);
				circle();
				vibr(0);
				_delay_ms(500);
			}
			clearTapFlag(); // Don't listen to any taps that occurred during execution
		}else if (tapFlag){ // Came out of sleep due to double tap
			clearTapFlag(); //Ack answers
			//Load the most resent settings from SD card
			unsigned int tempSettings = loadSettings(); //May bootload at this point if flag set
			// Load in the max and min intervals allowed
			maxInterval = tempSettings>>8;
			minInterval = tempSettings&0x0FF;
			//make sure mod 5
			maxInterval = maxInterval - (maxInterval%5);
			minInterval = minInterval - (minInterval%5);
			//bound the intervals
			if(minInterval<5) minInterval = 5;
			if(minInterval>55) minInterval = 55;
			if(maxInterval<5) maxInterval = 5;
			if(maxInterval>55) maxInterval = 55;
			if(minInterval > maxInterval){ //swap if necessary
				unsigned int tempInt = minInterval;
				minInterval = maxInterval;
				maxInterval = tempInt;
			}
			if(intervalMode){ // Was during a time estimation task, calculate difference
				intervalMode = 0;
				alarmHandled(); //turn off timeout alarm
				loadTime();
				unsigned int currentTime = getMinute()*60 + getSecond(); //In seconds since last hour
				unsigned int interval; //In seconds
				//Calculate the interval of time measured from start time to current time
				if(currentTime<=intervalStart){ //To account for wrap-around during an interval
					interval = currentTime + (3600-intervalStart); // current time plus time between the hour and interval start
				}else{ // Simply the current time minus when it began
					interval = currentTime - intervalStart;
				}
				int diffSeconds = interval - expectedInterval; //If negative, underestimate of interval
				int diffMinutes = diffSeconds/60; // Can only display in minutes
				// Round the minute difference
				if (diffSeconds<0){
					if((diffSeconds%60)>30) diffMinutes--;
				}else{
					if((diffSeconds%60)>30) diffMinutes++;
				}

				//display the difference between estimate and exact interval
				unsigned char startmin;
				unsigned char endmin;
				if(diffMinutes >= 0){ // Positive interval so minute 0 to the difference
					startmin = 0;
					endmin = diffMinutes;
				}else{					// Negative interval so go backwards
					startmin = 60 + diffMinutes;
					endmin = 59;
				}
				unsigned char j = startmin;
				timeOutCounter = 0;
				hideTime();
				while((!tapFlag) &&(timeOutCounter<timeout)){ // Display the difference until double tap or timeout
					setMinuteAbsolute(j);
					_delay_us(10);
					j++;
					if(j>endmin) j=startmin;
				}
				if(tapFlag){ // Double tap => show time and don't record
					clearTapFlag();
					displayCurrentTime();
					timeOutCounter = 0;
					while(timeOutCounter<timeout); //Wait for timeout
				}else{		// Timeout, record the event
					char test[] = "timeEst\0";
					writeResult(test, diffSeconds);
				}
			}else{ //Double tap but not interval mode
				//Record watch-checking event and show time
				char test[] = "timeCheck\0";
				alarmHandled();
				writeResult(test, 1);
				displayCurrentTime();
				timeOutCounter = 0;
				while((!tapFlag) && (timeOutCounter<timeout));
				if(tapFlag){ //If tapped again, begin interval estimation task
					clearTapFlag();
					intervalMode = 1;
					hideTime();
					loadTime();
					//set the intervalStart
					intervalStart = getMinute()*60 + getSecond(); // Starttime in seconds since last hour
					//Determine the random interval to be estimated
					if(minInterval != maxInterval){
						//randomly choose an interval between min and max by splitting the random range into partitions
						unsigned char partitions = (maxInterval - minInterval)/5 + 1;
						unsigned char randNum = random_num();
						unsigned char partitionLength = 255/partitions;
						expectedInterval = minInterval + (randNum/partitionLength)*5;
						if(expectedInterval > maxInterval) expectedInterval = maxInterval;
					}else{
						expectedInterval = minInterval;
					}
					unsigned char expIntMin = expectedInterval;
					expectedInterval *= 60; // Convert to Seconds

					//set up 60 minute time out for perception measurement
					unsigned char hour = getHour()+1;
					if(hour>23) hour = 0;
					unsigned char minute = (getMinute() + expIntMin)%60;
					setAlarm(hour, minute, getSecond());

					// Show duration to be guessed, timeout
					timeOutCounter = 0;
					unsigned char k = 0;
					while(timeOutCounter<timeout){
						setMinuteAbsolute(k);
						_delay_us(10);
						k++;
						if(k>expIntMin) k=0;
					}
				}
			}
			clearTapFlag();
		}else{ // Must've woken up from a single tap
			setHour(12);
			unsigned int i = 0;
			while ((i<2000) && (tapFlag == 0)){ // Give the user a chance to double tap
				_delay_ms(1);
				i++;
			}
			hideTime();
		}
	}
}
