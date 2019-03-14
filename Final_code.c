#include <avr/io.h>
#include "timer.h"
#include "io.c"
#include "bit.h"
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>

#define SHIFT_REGISTER DDRB
#define SHIFT_PORT PORTB
#define DATA (1 << PB5)           //MOSI (SI)
#define LATCH (1 << PB4)          //SS   (RCK)
#define CLOCK (1 << PB7)          //SCK  (SCK)
#define RED_LED (1 << PB0)		  //red led
#define TOUCH ( 1 << PA5)		  //touch sensor

unsigned char tempA = 0x00; //PINA
unsigned char tempD = 0x00; // PINB
unsigned char tempLED = 0x00; //send to led
int record = 0; //check if it is recording
unsigned char press = 0x00; //what button was press
unsigned char start_game = 0x00;
unsigned char mark = 0x00; //when a button is press and release
int counter = 0; //check notes in the level
int play_counter = 0; //counting for playing note
int play_level = 0;// count how many note in the melody
unsigned char level = 1;
unsigned char win = 0;
unsigned char lose = 0;
unsigned char play = 0;
unsigned char level1[3] = { 0x01, 0x04,0x02 };// 3 note									1 3 2 
unsigned char level2[5] = { 0x08, 0x01, 0x08, 0x10, 0x02 }; // 5 note						4 1 4 5 2
unsigned char level3[7] = { 0x02, 0x10, 0x08, 0x10, 0x01, 0x04, 0x02 };// 7 note			2 5 4 5 1 3 2 
unsigned char level4[5] = { 0x08, 0x04, 0x01, 0x02, 0x01 };// 5 note, faster				4 3 1 2 1 
unsigned char level5[7] = { 0x01, 0x10, 0x04, 0x02, 0x08, 0x02, 0x01 };// 7 note faster	1 5 3 2 4 2 1 
double correct[2] = { 1174.659, 987.767 };
double winner[10] = { 329.628, 0.00,329.628,0.00,329.628,261.626, 293.665, 329.628, 293.665, 329.628 };
unsigned long change_time = 10;
int speaker_count = 0;
unsigned char game_start = 0;
int k = 0;
int play_sound; //don't interrupt 
unsigned char *begin_words;
unsigned char i;
unsigned char len;


//light up led in game


void reset(); //prototype
void speaker_counter();
void win_sound();
/////////////////speaker function//////////////////////////////////////////////////////////////////

// 0.954 hz is lowest frequency possible with this function,
// based on settings in PWM_on()
// Passing in 0 as the frequency will stop the speaker from generating sound
void set_PWM(double frequency) {
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR0B &= 0x08; } //stops timer/counter
		else { TCCR0B |= 0x03; } // resumes/continues timer/counter

		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR0A = 0xFFFF; }

		// prevents OCR0A from underflowing, using prescaler 64					// 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR0A = 0x0000; }

		// set OCR3A based on desired frequency
		else { OCR0A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT0 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}

void PWM_on() {
	TCCR0A = (1 << COM0A0) | (1 << WGM00);
	// COM3A0: Toggle PB3 on compare match between counter and OCR0A
	TCCR0B = (1 << WGM02) | (1 << CS01) | (1 << CS00);
	// WGM02: When counter (TCNT0) matches OCR0A, reset counter
	// CS01 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR0A = 0x00;
	TCCR0B = 0x00;
}
////////////////speaker function end//////////////////////////////////////////////////////////////



//display/change in game led speed
enum game_led_states { game_init, game_off, game_on } game_led_state;
void game_led()
{
	switch (game_led_state)
	{
	case game_init:
		game_led_state = game_off;
		break;
	case game_off:
		if (play == 1)
		{
			game_led_state = game_on;
		}
		break;
	case game_on:
		if (play_counter == play_level)
		{
			game_led_state = game_off;
			play_counter = 0; //reset note counter
			play = 0; //stop playing
			tempLED = 0x00;
		}
		break;
	default:
		break;
	}
	switch (game_led_state)
	{
	case game_init:
		break;
	case game_off:
		break;
	case game_on:
		if (level == 1)
		{
			change_time = 10; //keep same speed
			tempLED = level1[play_counter];
			play_counter++;
		}
		else if (level == 2)
		{
			change_time = 10; //keep same speed
			tempLED = level2[play_counter];
			play_counter++;
		}
		else if (level == 3)
		{
			change_time = 10; //keep same speed
			tempLED = level3[play_counter];
			play_counter++;
		}
		else if (level == 4)
		{
			change_time = 8; //change speed
			tempLED = level4[play_counter];
			play_counter++;
		}
		else if (level == 5)
		{
			change_time = 8; //change speed
			tempLED = level5[play_counter];
			play_counter++;
		}
		break;
	default:
		break;
	}
}



//game level up
enum level_states { level_init, levelone, leveltwo, levelthree, levelfour, levelfive } level_state;
void game_level()
{
	switch (level_state) //transition
	{
	case level_init:
		if (level == 1 && start_game == 1)
		{
			level = 4;
			play = 1;
			level_state = levelfour;				//testing
			play_level = 5;
			//level_state = levelone;
			//play_level = 3;
			start_game = 0;
		}
		break;
	case levelone:
		if (level == 2)
		{
			play = 1;
			level_state = leveltwo;
			play_level = 5;
		}
		break;
	case leveltwo:
		if (level == 3)
		{
			play = 1;
			level_state = levelthree;
			play_level = 7;
		}
		break;
	case levelthree:
		if (level == 4)
		{
			play = 1;
			level_state = levelfour;
			play_level = 5;
		}
		break;
	case levelfour:
		if (level == 5)
		{
			play = 1;
			level_state = levelfive;
			play_level = 7;
		}
		break;
	case levelfive:
		break;
	default:
		break;
	}
	switch (level_state) //action
	{
	case level_init:
		break;
	case levelone:

		if (mark == 1 && GetBit(PORTB, 0) == 1)
		{

			mark = 0;

			if (level1[counter] == press)
			{

				counter += 1;
				if (counter == 3)
				{
					level = 2;
					counter = 0; //reset counter
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0


				}
			}
			else if (press != 0x00)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
		}
		break;
	case leveltwo:
		if (mark == 1 && GetBit(PORTB, 0) == 1)
		{
			mark = 0;
			if (level2[counter] == press)
			{
				counter += 1;
				if (counter == 5)
				{
					level = 3;
					counter = 0;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
			}
			else if (press != 0x00)
			{
				lose = 1;
				reset();
			}
		}
		break;
	case levelthree:
		if (mark == 1 && GetBit(PORTB, 0) == 1)
		{
			mark = 0;
			if (level3[counter] == press)
			{
				counter += 1;
				if (counter == 7)
				{
					level = 4;
					counter = 0;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
			}
			else if (press != 0x00)
			{
				lose = 1;
				reset();
			}
		}
		break;
	case levelfour:
		if (mark == 1 && GetBit(PORTB, 0) == 1)
		{
			mark = 0;
			if (level4[counter] == press)
			{
				counter += 1;
				if (counter == 5)
				{
					level = 5;
					counter = 0; //reset counter
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
			}
			else if (press != 0x00)
			{
				lose = 1;
				reset();
			}
		}
		break;
	case levelfive:
		if (mark == 1 && GetBit(PORTB, 0) == 1)
		{
			mark = 0;
			if (level5[counter] == press)
			{
				counter += 1;
				if (counter == 7)
				{
					win = 1;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0

					//win_sound();
				}
			}
			else if (press != 0x00)
			{
				lose = 1;
				reset();
			}
		}
		break;
	default:
		break;
	}

}

void testing()
{
	if (k == 0)
	{
		tempLED = level1[0];
	}
	else if (k == 1)
	{
		tempLED = level1[1];
	}
	else if (k == 2)
	{
		tempLED = level1[2];
	}

	k++;


}

//set up register
void register_setup()
{
	SHIFT_REGISTER |= (DATA | LATCH | CLOCK | RED_LED | 0x02 | 0x04 | 0x08); //Set control pins as outputs, and red led
	SHIFT_PORT &= ~(DATA | LATCH | CLOCK | RED_LED | 0x02 | 0x04 | 0x08);        //Set control pins low, and red  led


	//Setup SPI
	SPCR = (1 << SPE) | (1 << MSTR);  //Start SPI as Master

	//Pull LATCH low (Important: this is necessary to start the SPI transfer!)
	SHIFT_PORT &= ~LATCH;


	//Shift in some data
	//	SPDR = 0b01010101;        //This should light alternating LEDs
	SPDR = 0xFF;
	//Wait for SPI process to finish
	while (!(SPSR & (1 << SPIF)));

	//Shift in some more data since I have two shift registers hooked up
	SPDR = 0x00;        //This should light alternating LEDs
	//Wait for SPI process to finish
	while (!(SPSR & (1 << SPIF)));

	//Toggle latch to copy data to the storage register
	SHIFT_PORT |= LATCH;
	SHIFT_PORT &= ~LATCH;
}


//change led in the register
void set_register()
{
	//Shift in some data
	SPDR = 0xFF;        //This should light alternating LEDs
	//SPDR = 0xAA;
	//Wait for SPI process to finish
	while (!(SPSR & (1 << SPIF)));

	//Shift in some more data since I have two shift registers hooked up
	SPDR = tempLED;        //This should light alternating LEDs
	speaker_counter();
	//Wait for SPI process to finish
	while (!(SPSR & (1 << SPIF)));

	//Toggle latch to copy data to the storage register
	SHIFT_PORT |= LATCH;
	SHIFT_PORT &= ~LATCH;
}


enum led_states { led_init, wait, hold } led_state;
void set_led()
{
	tempA &= ~TOUCH; //ignore touch snesor
	switch (led_state) //combine action and transition
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
		else if (tempA == 0x04 || tempD == 0x04)
		{
			tempLED = 0x04;
			led_state = hold;

		}
		else if (tempA == 0x08 || tempD == 0x08)
		{
			tempLED = 0x08;
			led_state = hold;

		}
		else if (tempA == 0x10 || tempD == 0x10)
		{
			tempLED = 0x10;
			led_state = hold;

		}
		else if (tempA == 0x40)
		{
			level = 1;
			game_led_state = game_init;
			level_state = level_init;
			start_game = 1;
			play = 1;
		}

		break;
	case hold:
		if ((tempA == 0x00) && (tempD == 0x00))
		{
			mark = 1;
			press = tempLED;
			tempLED = 0x00;
			led_state = wait;
		}
		break;
	default:
		break;
	}
}

enum sensor_states { sensor_init, off, on } sensor_state;
void set_sensor()
{
	switch (sensor_state)
	{
	case sensor_init:
		sensor_state = off;
		break;
	case off:
		if (GetBit(PINA, 5)) //if sensor is on
		{
			sensor_state = on;
		}
		break;
	case on:
		if (~GetBit(PINA, 5)) //if sensor is off
		{
			sensor_state = off;
		}
		break;
	default:
		break;

	}
	switch (sensor_state)
	{
	case sensor_init:
		break;
	case off:
		//PORTB = SetBit(PORTB,0, 0 ); //set PB0 to 0
		record = 0;
		break;
	case on:
		PORTB = SetBit(PORTB, 0, 1); //set PB0 to 1
		record = 1;
		press = 0x00;
		break;
	default:
		break;

	}
}

//lose or win
void reset()
{
	if (lose == 1)
	{
		PORTB = SetBit(PORTB, 2, 1);
		tempLED = 0x00; //send to led
		record = 0; //check if it is recording
		press = 0x00; //what button was press
		start_game = 0x00;
		mark = 0x00; //when a button is press and release
		counter = 0; //check notes in the level
		play_counter = 0; //counting for playing note
		play_level = 0;// count how many note in the melody
		level = 1;
		win = 0;
		lose = 0;
		play = 0;
		led_state = led_init;
		sensor_state = sensor_init;
		game_led_state = game_init;
		level_state = level_init;
		PORTB = SetBit(PORTB, 0, 0); //reset recording button
	}

}


//change hex into value, and play
void speaker_counter()
{
	if (tempLED == 0x01)
	{
		set_PWM(261.63);
	}
	else if (tempLED == 0x02)
	{
		set_PWM(293.66);
	}
	else if (tempLED == 0x04)
	{
		set_PWM(329.63);
	}
	else if (tempLED == 0x08)
	{
		set_PWM(349.23);
	}
	else if (tempLED == 0x10)
	{
		set_PWM(392.00);
	}
	else if (tempLED == 0x00 && win == 0)
	{
		set_PWM(0.00);
	}
}

void win_sound()
{
	if (win == 1)
	{
		if (speaker_count == 10)
		{
			win = 0;
		}
		else
		{
			PORTB = SetBit(PORTB, 1, 1);
			set_PWM(winner[speaker_count]);
			speaker_count++;
		}


	}
}

enum lcd_states { lcd_init, lcd_begin, lcd_on, lcd_single, lcd_two, lcd_lose, lcd_win } lcd_state;
void lcd_game()
{
	switch (lcd_state)
	{
	case lcd_init:
		lcd_state = lcd_begin;
		break;
	case lcd_begin:

		break;
	case lcd_on:
		break;
	case lcd_single:
		break;
	case lcd_two:
		break;
	case lcd_lose:
		break;
	case lcd_win:
		break;
	default:
		break;

	}
	switch (lcd_state) //action
	{
	case lcd_init:
		break;
	case lcd_begin:
		LCD_DisplayString(1, begin_words);
		if (i <= len) { begin_words++; i++; }
		else { begin_words -= len; i = 1; }
		break;
	case lcd_on:

		break;
	case lcd_single:
		break;
	case lcd_two:
		break;
	case lcd_lose:
		break;
	case lcd_win:
		break;
	default:
		break;
	}
}


int main(void)
{

	//need double tap of sensor to make it work
	//Setup IO
	DDRD = 0xC0; PORTD = 0x3F; //for lcd D7D6, D4-D0 for button
	DDRC = 0xFF; PORTC = 0x00; //for lcd 
	DDRA = 0x00; PORTA = 0xFF; //A for output

	unsigned long game_led_elapsed = 0;

	PWM_on();
	TimerSet(50);
	TimerOn();

	register_setup();

	//unsigned long period = 30000;
	unsigned char win_elapsed = 0;
	unsigned char lcd_elapsed = 0;
	led_state = led_init;
	sensor_state = sensor_init;
	game_led_state = game_init;
	level_state = level_init;
	lcd_state = lcd_init;
	game_level();	 //initialize the game first
	lose = 0;
	win = 0;


	//lcd variable initiation////////////////////////
	begin_words = "         press reset to start the game";
	i = 1;
	len = strlen(begin_words);
	////////////////////////////

	LCD_init();


	while (1)
	{
		if (lcd_elapsed >= 10)
		{
			lcd_game();
			lcd_elapsed = 0;
		}



		//PORTB |= RED_LED; // MAKE red RECORD SHINE
		//Loop forever
		tempA = ~PINA;
		tempD = ~PIND & 0x1F;
		if (win_elapsed >= 10)
		{
			win_sound();
			win_elapsed = 0;
		}
		set_led();
		set_register();
		set_sensor();
		game_level();
		if (game_led_elapsed >= change_time)
		{
			game_led();
			game_led_elapsed = 0;
		}
		while (!TimerFlag) {}  // Wait for BL's period
		TimerFlag = 0;        // Lower flag
		game_led_elapsed += 1;
		win_elapsed += 1;
		lcd_elapsed += 1;
		//set_record();
	}
}
