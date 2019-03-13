
#include <avr/io.h>
#include "timer.h"
#include "io.c"

#define SHIFT_REGISTER DDRB
#define SHIFT_PORT PORTB
#define DATA (1 << PB5)           //MOSI (SI)
#define LATCH (1 << PB4)          //SS   (RCK)
#define CLOCK (1 << PB7)          //SCK  (SCK)
#define RED_LED (1 << PB0)
#define touch ( 1 << PA5)

unsigned char tempA = 0x00; //PINA
unsigned char tempD = 0x00; // PINB
unsigned char tempLED = 0x00; //send to led
unsigned char record = 0x00; //check if it is recording
vector <unsigned char> sequence;  //use for recording

void register_setup()
{
	SHIFT_REGISTER |= (DATA | LATCH | CLOCK | RED_LED); //Set control pins as outputs, and red led
	SHIFT_PORT &= ~(DATA | LATCH | CLOCK | RED_LED);        //Set control pins low, and red  led
	
	
	//Setup SPI
	SPCR = (1<<SPE) | (1<<MSTR);  //Start SPI as Master
	
	//Pull LATCH low (Important: this is necessary to start the SPI transfer!)
	SHIFT_PORT &= ~LATCH;
	
	
	//Shift in some data
	//	SPDR = 0b01010101;        //This should light alternating LEDs
	SPDR = 0xFF;
	//Wait for SPI process to finish
	while(!(SPSR & (1<<SPIF)));
	
	//Shift in some more data since I have two shift registers hooked up
	SPDR = 0x00;        //This should light alternating LEDs
	//Wait for SPI process to finish
	while(!(SPSR & (1<<SPIF)));
	
	//Toggle latch to copy data to the storage register
	SHIFT_PORT |= LATCH;
	SHIFT_PORT &= ~LATCH;
}

void set_register()
{
	//Shift in some data
	SPDR =0xFF;        //This should light alternating LEDs
	//SPDR = 0xAA;
	//Wait for SPI process to finish
	while(!(SPSR & (1<<SPIF)));
	
	//Shift in some more data since I have two shift registers hooked up
	SPDR = tempLED;        //This should light alternating LEDs
	//Wait for SPI process to finish
	while(!(SPSR & (1<<SPIF)));
	
	//Toggle latch to copy data to the storage register
	SHIFT_PORT |= LATCH;
	SHIFT_PORT &= ~LATCH;
}




enum led_states {led_init, wait, hold} led_state;
void set_led()
{
	tempA &= ~touch; //ignore touch snesor
	switch(led_state) //combine action and transition
	{
		case led_init:
		led_state = wait;
		break;
		case wait:
		if (tempA == 0x01 || tempD == 0x01)
		{
			tempLED = 0x01;
			led_state = hold;
		}
		else if (tempA == 0x02 || tempD == 0x02)
		{
			tempLED = 0x02;
			led_state = hold;
		}
		else if(tempA == 0x04 || tempD == 0x04)
		{
			tempLED = 0x04;
			led_state = hold;
		}
		else if(tempA == 0x08 || tempD == 0x08)
		{
			tempLED = 0x08;
			led_state = hold;
		}
		else if(tempA == 0x10 || tempD == 0x10)
		{
			tempLED = 0x10;
			led_state = hold;
		}
		break;
		case hold:
		if ( (tempA == 0x00) && (tempD == 0x00))
		{
			
			tempLED = 0x00;
			led_state = wait;
		}
		break;
		default:
		break;
	}
}

enum sensor_states {sensor_init, off, on} sensor_state;
void set_sensor()
{
	switch(sensor_state)
	{
		case sensor_init:
		sensor_state = off;
		break;
		case off:
		break;
		case on:
		break;
		
	}
	switch(sensor_state)
	{
		case sensor_init:
		break;
		case off:
		break;
		case on:
		break;
		
	}
}



int main(void)
{
	//Setup IO
	DDRD = 0xC0; PORTD = 0x3F; //for lcd D7D6, D4-D0 for button
	DDRA = 0x00; PORTA  = 0xFF; //A7 for output
	register_setup();
	//light_state = light_init;
	//led_state = led_init;
	while(1)
	{
		PORTB |= RED_LED; // MAKE red RECORD SHINE
		//Loop forever
		tempA = ~PINA;
		tempD = ~PIND & 0x1F;
		set_led();
		set_register();
		
	}
}
