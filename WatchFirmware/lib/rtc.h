/***********************************************************/
/* rtc.h - used for all DS3234 real time clock interfacing */
/* Based off of the arduino SPI library                    */
/* as well as the DS3234 arduino library by:               */
/* Petre Rodan <petre.rodan@simplex.ro>                    */
/* https://github.com/rodan/ds3234                         */
/*                                                         */
/* Released under GPLv3 since the libraries it is based on */
/* used the same license                                   */
/***********************************************************/


#define CS 4
#define CLK 1
#define MOSI 2
#define MISO 3

void initRTC(void);

void setTime(unsigned char sec, unsigned char min, unsigned char hr, unsigned char dow, unsigned char day, unsigned char month, unsigned int year);

void getSQL(char *buf);

void loadTime(void);
unsigned char getYear(void);
unsigned char getMonth(void);
unsigned char getDay(void);
unsigned char getDOW(void); // Day of Week
unsigned char getHour(void);
unsigned char getMinute(void);
unsigned char getSecond(void);

// Alarm functions
void setAlarm(unsigned char hour, unsigned char min, unsigned char sec);
void alarmHandled(void);
unsigned char checkAlarm(void);
