// Wrapper file for sd card manipulations (logging & settings)
// Depends on the RTC library ds3234

#include "ff.h"

FRESULT writeResult(char test[],int result);
unsigned int loadSettings(void);

