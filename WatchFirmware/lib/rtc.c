/***********************************************************/
/* rtc.c - used for all DS3234 real time clock interfacing */
/* Based off of the arduino SPI library                    */
/* as well as the DS3234 arduino library by:               */
/* Petre Rodan <petre.rodan@simplex.ro>                    */
/* https://github.com/rodan/ds3234                         */
/*                                                         */
/* Released under GPLv3 since the libraries it is based on */
/* used the same license                                   */
/***********************************************************/

#include "rtc.h"
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "ff.h"
#include "diskio.h"

// Simple macros for going between decimal and BCD format as used by the RTC
#define dectobcd(val) ((val / 10 * 16) + (val % 10))
#define bcdtodec(val) ((val / 16 * 10) + (val % 16))

unsigned char spiSettings; // Temporarily holds the SPI settings so they can be restored

unsigned char currTimeDate[7]; // Global private variables storing the most recently loaded datetime
unsigned int currYear; // Used to hold the current year since it is larger than a char

// Helper CS function
// We modified the CS line to be connected to the RTC Interrupt line
void setCS(unsigned char state){
	if(state){
		// Input with pullup
		PORTB |= (1<<CS);
		DDRB &= ~(1<<CS);
		// Alarm interrupts may now be eenabled again
		PCMSK0 |= 1<<PCINT4;

		// Force interrupt if alarm occurred by toggling the pin then restoring to desired state
		if (checkAlarm()){
			DDRB |= (1<<CS);
			PORTB &= ~(1<<CS);
			PORTB |= (1<<CS);
			DDRB &= ~(1<<CS);
		}
	}else{
		// Disable interrupt while we are trying to talk to the chip
		PCMSK0 &= ~(1<<PCINT4);
		// Output pulled low
		DDRB |= (1<<CS);
		PORTB &= ~(1<<CS);
	}
}

void initSPI(void){
	DDRB |= (1<<CLK)|(1<<MOSI);
	// CLK & MOSI low, CS high
	PORTB &= ~((1<<CLK)|(1<<MOSI));
	setCS(1);

 	//Temporarily store it the SPI settings
	spiSettings = SPCR;

	// Initialize for the RTC
	SPCR = (1<<MSTR)|(1<<SPE)|(0x04);
}

void restoreSPI(void){
	setCS(1); 			// double check that it is high and/or an interrupt has occurred
	SPCR = spiSettings; // restore previous settings
}

// Transfer SPI data/read back result
unsigned char transfer(unsigned char data){
	SPDR = data;				// Load send buffer
	while(!(SPSR & (1<<SPIF))); // Wait until flag set to make sure data was sent
	return SPDR;				// Return the data shifted back in
}

void initRTC(void){
	initSPI();
	
	PCICR |= 1<<PCIE0; // make sure pin change interrupts are enabled for alarms
	// Setup control register
	setCS(0);
    transfer(0x8E);
	// Oscillator on, Square wave off, temp conversion off, 2 square wave bits don't matter
	// Interrupt generation enabled, alarm 2 disabled, alarm 1 disabled
    transfer(0b00000100);
    setCS(1);
	_delay_ms(10); // Wait for it to settle
	restoreSPI();
}

void setTime(unsigned char sec, unsigned char min, unsigned char hr, unsigned char dow, unsigned char day, unsigned char month, unsigned int year){
    // Load the given time into the RTC
	unsigned char i, century, short_y;

    century = 0b10000000;
    short_y = year - 2000;

    unsigned char TimeDate[7] = {sec, min, hr, dow, day, month, short_y};
	
	initSPI(); // Send all the data
    for (i = 0; i <= 6; i++) {
        setCS(0);
        transfer(i + 0x80);
        if (i == 5)
            transfer(dectobcd(TimeDate[5]) + century);
        else
            transfer(dectobcd(TimeDate[i]));
        setCS(1);
    }
	restoreSPI();
}

void getSQL(char *buf){  //Returns as YYYY-MM-DD hh:mm:ss, must get array of length 20!
    unsigned char TimeDate[7];        //second,minute,hour,dow,day,month,year
    unsigned char century = 0;
    unsigned char i, n;
    uint16_t year_full;

	initSPI();
    for (i = 0; i <= 6; i++) {
        setCS(0);
        transfer(i + 0x00);
        n = transfer(0x00);
        setCS(1);
        if (i == 5) {           // month address also contains the century on bit7
            TimeDate[5] = bcdtodec(n & 0x1F);
            century = (n & 0b0000000) >> 7;
        } else {
            TimeDate[i] = bcdtodec(n);
        }
    }
	restoreSPI();

    year_full = 2000 + TimeDate[6];

    snprintf(buf,20, "%d-%02d-%02d %02d:%02d:%02d", year_full, TimeDate[5], TimeDate[4], TimeDate[2], TimeDate[1],TimeDate[0]);
}

void loadTime(void){ // Loads the current time into memory to be easily read atomically with getter functions
    unsigned char century = 0;
    unsigned char i, n;

	initSPI();
    for (i = 0; i <= 6; i++) {
        setCS(0);
        transfer(i + 0x00);
        n = transfer(0x00);
        setCS(1);
        if (i == 5) {           // month address also contains the century on bit7
            currTimeDate[5] = bcdtodec(n & 0x1F); // Store current field
            century = (n & 0b0000000) >> 7;
        } else {
            currTimeDate[i] = bcdtodec(n); // Store current field
        }
    }
	restoreSPI();

    currYear = 2000 + currTimeDate[6]; // Store current full year
}

unsigned char getYear(void){
	return currYear;
}

unsigned char getMonth(void){
	return currTimeDate[5];
}

unsigned char getDay(void){
	return currTimeDate[4];
}

unsigned char getDOW(void){
	return currTimeDate[3];
}

unsigned char getHour(void){
	return currTimeDate[2];
}

unsigned char getMinute(void){
	return currTimeDate[1];
}

unsigned char getSecond(void){
	return currTimeDate[0];
}

// Set alarm 1 Hour:Minute:Second
void setAlarm(unsigned char hour, unsigned char min, unsigned char sec){
    unsigned char alarm[4] = {sec, min, hour}; // Currently don't care about the day
    unsigned char i;
	alarmHandled(); // Make sure the alarm was handled before continuing
	initSPI();
	// Turn on alarm interrupts
	setCS(0);
    transfer(0x8E);
	// Oscillator on, Square wave off, temp conversion off, 2 square wave bits don't matter
	// Interrupt generation enabled, alarm 2 disabled, alarm 1 enabled
    transfer(0b00000101);
    setCS(1);
	_delay_ms(10);
    for (i = 0; i <= 3; i++) {
        setCS(0);
        transfer(i + 0x87);
        if (i == 3) {
            transfer(1 << 7); //Disable day match
        }else{
            transfer(dectobcd(alarm[i]));
		}
        setCS(1);
    }
	restoreSPI();
}

unsigned char checkAlarm(void){ // Check if the RTC interrupt has ocurred
	// Input with pullup
	PORTB |= (1<<CS);
	DDRB &= ~(1<<CS);
	_delay_ms(10);
	// Return if it is pulled low
	return ((PINB & (1<<CS))==0);
}

// Call this to disable the alarm after it has gone off
void alarmHandled(void){
	unsigned char val;
	cli();// Make sure interrupts can't happen while disabling

	// Forced SPI setup
	DDRB |= (1<<CLK)|(1<<MOSI)|(1<<CS);
	// CLK & MOSI low, CS high
	PORTB &= ~((1<<CLK)|(1<<MOSI));
	PORTB |= (1<<CS);

	// Initialize for the RTC
	SPCR = (1<<MSTR)|(1<<SPE)|(0x04);

	// First read in the current settings
    PORTB &= ~(1<<CS);
    transfer(0x0F);
    val = transfer(0x00);
    PORTB |= (1<<CS);

	// Now set the alarm flags to 0
	val &= ~(0b00000011);
	PORTB &= ~(1<<CS);
    transfer(0x8F);
    transfer(val);
    PORTB |= (1<<CS);

	// Turn off alarm interrupts, and reset to the initial RTC settings
	// Setup control register
	PORTB &= ~(1<<CS);
    transfer(0x8E);
	// Oscillator on, Square wave off, temp conversion off, 2 square wave bits don't matter
	// Interrupt generation enabled, alarm 2 disabled, alarm 1 disabled
    transfer(0b00000100);
    PORTB |= (1<<CS);
	_delay_ms(10); // Wait for it to settle
	sei(); //Interrupts may now happen
	PCIFR = 1; // Make sure flag is cleared
}

DWORD get_fattime (void) { // Used by fatfs to retrieve the current time
    /*
    bit31:25
    Year from 1980 (0..127)
   
    bit24:21
    Month (1..12)
   
    bit20:16
    Day in month(1..31)
   
    bit15:11
    Hour (0..23)
   
    bit10:5
    Minute (0..59)
   
    bit4:0
    Second / 2 (0..29)
    */
	uint8_t TimeDate[7];        //second,minute,hour,dow,day,month,year
    uint8_t century = 0;
    uint8_t i, n;
    uint16_t year_full;

	initSPI();
    for (i = 0; i <= 6; i++) {
        setCS(0);
        transfer(i + 0x00);
        n = transfer(0x00);
        setCS(1);
        if (i == 5) {           // month address also contains the century on bit7
            TimeDate[5] = bcdtodec(n & 0x1F);
            century = (n & 0b0000000) >> 7;
        } else {
            TimeDate[i] = bcdtodec(n);
        }
    }
	restoreSPI();

    year_full = 2000 + TimeDate[6];

    return      ((DWORD)(year_full - 1980) << 25)
            | ((DWORD)TimeDate[5] << 21)
            | ((DWORD)(TimeDate[4]+1) << 16)
            | ((DWORD)TimeDate[2] << 11)
            | ((DWORD)TimeDate[1] << 5)
            | ((DWORD)(TimeDate[0] >> 10) >> 1); 
}

