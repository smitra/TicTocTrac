/**
display.h
tictoctrac.com
*/

void init_display(void);
void setMinute(unsigned char, unsigned char);
void setMinuteAbsolute(unsigned char min);
void setHour(unsigned char);
void circle(void);
void showTime(unsigned char hr, unsigned char min);
void hideTime(void);
void displayCurrentTime(void);
