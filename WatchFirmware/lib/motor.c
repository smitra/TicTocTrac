//motor.c

#include <avr/io.h>
#include "motor.h"


void vibr(unsigned char set){ // Turn the motor on (high value) or off (low value)
	if(set){
		PORTB |= (1<<6);
	}else{
		PORTB &= ~(1<<6);
	}
}


	

void init_motor(void){
	DDRB |= (1<<6); // Make the motor pin an output
}
