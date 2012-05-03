// TicTocTrac wrapper file for sd card manipulations (logging & settings)
// Depends on the RTC library ds3234
#include "sd.h"
#include "ff.h"
#include <string.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdio.h>
#include "rtc.h"

#define f_size(fp) ((fp)->fsize)



FATFS FileSystemObject;
FIL logFile;
FIL settingsFile;

FRESULT writeResult(char test[], int result){
	char timeString[20];
	getSQL(timeString); // Load time at which the test occurred
	f_mount(0, &FileSystemObject);
	if (f_open(&logFile, "/tictoc.csv", FA_WRITE | FA_OPEN_EXISTING)){
		// File does not exist yet, try to create it and write the header
		FRESULT res = f_open(&logFile, "/tictoc.csv", FA_WRITE | FA_CREATE_NEW);
		if(res)	return res; // File opening failed!
		f_printf(&logFile, "time,device,test,result\n"); // Write header
	}
	f_lseek(&logFile, f_size(&logFile)); // Make sure we are at the end of the file
	f_printf(&logFile, "%s,watch,%s,%d\n",timeString,test,result); // Write line of data
	f_sync(&logFile); // Make sure file is synched
	f_close(&logFile);
	return f_mount(0,0);
}

unsigned int loadSettings(void){
	char bl_ct[5];
	char time[25];
	char MinInterval[10];
	char MaxInterval[10];


	f_mount(0, &FileSystemObject);
	FRESULT res = f_open(&settingsFile, "/settings.dat", FA_WRITE | FA_READ | FA_OPEN_EXISTING);
	if(res)	return res; // File opening failed!
	//loop through file 
	f_gets(bl_ct, 5, &settingsFile);	
	f_gets(time, 25, &settingsFile);
	f_gets(MinInterval, 10, &settingsFile);
	f_gets(MaxInterval, 10, &settingsFile);
	
	
	if (bl_ct[0] == 'B'){ 			// Bootload if proper flag is set
		f_lseek(&settingsFile, 0); 	// Jump to beginning of file
		f_putc('*', &settingsFile); // Disable bootload flag
		f_sync(&settingsFile);		// Make sure the file is saved
		f_close(&settingsFile);
		f_mount(0,0);
		// Bootload by forcing reset using the watchdog timer
		cli();
		wdt_enable(WDTO_4S);
		while(1);
	}
	 
	if (bl_ct[1] == 'T'){			// Set the current time if proper flag is set
		f_lseek(&settingsFile, 1); 	// Jump proper position in file
		f_putc('*', &settingsFile);	// Disable set time flag
 		//set time on RTC		
		unsigned int sec;
		unsigned int min;
		unsigned int hr;
		unsigned int day;
		unsigned int month;
		unsigned int year;
		//YYYY-MM-DD hh:mm:ss
		sscanf(time, "CT %4u-%2u-%2u %2u:%2u:%2u", &year, &month, &day, &hr, &min, &sec);
		setTime((unsigned char)sec, (unsigned char)min, (unsigned char)hr, 0, (unsigned char)day, (unsigned char)month, year);
		f_sync(&settingsFile); // Make sure file is synched
	}
	// Load in the max and min duration creation test settings
	unsigned int min;
	unsigned int max;
	sscanf(MinInterval, "min %2u", &min);
	sscanf(MaxInterval, "max %2u", &max);

	// Close out and return the max and min duration creation test settings
	f_close(&settingsFile);
	f_mount(0,0);
	return (max<<8)|min;
}
