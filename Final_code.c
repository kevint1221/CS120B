#include <avr/io.h>

#define SHIFT_REGISTER DDRB
#define SHIFT_PORT PORTB
#define BUTTON_INPUT DDRA
#define BUTTON_PORT  PORTA
#define DATA (1 << PB5)           //MOSI (SI)
#define LATCH (1 << PB4)          //SS   (RCK)
#define CLOCK (1 << PB7)          //SCK  (SCK)
#define BUTTON1 (1 << PA0)       
//unsigned char button = 0x00;
unsigned char tempA = 0x00;
unsigned char tempD = 0x00;
unsigned char button1 = 0x01;
unsigned char button2 = 0x02;
unsigned char button3 = 0x04;
unsigned char button4 = 0x08;
unsigned char button5 = 0x10;
unsigned char led1 = 0x01;
unsigned char led2 = 0x02;
unsigned char led3 = 0x04;
unsigned char led4 = 0x08;
unsigned char led5 = 0x10;
unsigned char led0 = 0x00;
unsigned char tempLED = 0x00;
void register_setup()
{
		SHIFT_REGISTER |= (DATA | LATCH | CLOCK ); //Set control pins as outputs
		SHIFT_PORT &= ~(DATA | LATCH | CLOCK);        //Set control pins low
		
		
		//Setup SPI
		SPCR = (1<<SPE) | (1<<MSTR);  //Start SPI as Master
		
		//Pull LATCH low (Important: this is necessary to start the SPI transfer!)
		SHIFT_PORT &= ~LATCH;
		
		
		//Shift in some data
		SPDR =0xFF;        //This should light alternating LEDs
		//SPDR = 0xAA;
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

void portD_setup()
{
	DDRD = 0xC0; PORTD = 0x3F; //for lcd D7D6, D4-D0 for button
	
}
void set_register()
{
	//Shift in some data
	SPDR =0x00;        //This should light alternating LEDs
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
	switch(led_state) //combine action and transition
	{
		case led_init:
			led_state = wait;
			break;
		case wait:
			if (tempA == button1 || tempD == button1)
			{
				tempLED = led1;
				led_state = hold;
			}
			else if (tempA == button2 || tempD == button2)
			{
				tempLED = led2;
				led_state = hold;
			}
			else if(tempA == button3 || tempD == button3)
			{
				tempLED = led3;
				led_state = hold;
			}
			else if(tempA == button4 || tempD == button4)
			{
				tempLED = led4;
				led_state = hold;
			}
			else if(tempA == button5 || tempD == button5)
			{
				tempLED = led5;
				led_state = hold;
			}
			break;
		case hold:
			if ( (tempA == 0x00) && (tempD == 0x00))
			{
				tempLED = led0;
				led_state = wait;
			}
			break;
		default:
			break;
	}
}

enum light_states {light_init, on} light_state;
void light_led()
{
	switch(light_state)
	{
		case light_init:
			light_state = on;
			break;
		case on:
			set_register();
			break;
		default: 
			break;
	}		
	
}




int main(void)
{
	//Setup IO
	portD_setup();
	register_setup();
	BUTTON_INPUT |= ~BUTTON1; //Set input
	BUTTON_PORT = BUTTON1; //SET high
	led_state = led_init;
	light_state = light_init;
	TimerSet(100);
	TimerOn();
	while(1)
	{

		//Loop forever
		tempA = ~PINA;
		tempD = ~PIND & 0x1F; 
		
		set_led();
		light_led();
	
	
	}
}
