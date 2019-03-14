#include <avr/io.h>
#include "timer.h"
#include "io.c"
#include "bit.h"
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>

//define variable
#define SHIFT_REGISTER DDRB
#define SHIFT_PORT PORTB
#define DATA (1 << PB5)           //MOSI (SI)
#define LATCH (1 << PB4)          //SS   (RCK)
#define CLOCK (1 << PB7)          //SCK  (SCK)
#define RED_LED (1 << PB0)		  //red led
#define TOUCH ( 1 << PA5)		  //touch sensor

//input variable
unsigned char tempA = 0x00; //PINA
unsigned char tempD = 0x00; // PINB
//output variable
unsigned char tempLED = 0x00; //send to led
unsigned char tempLED2 = 0xAA;
//sensor
int record = 0; //check if it is recording
//button
unsigned char press = 0x00; //what button was press
unsigned char mark = 0x00; //when a button is press and release
//game level
unsigned char start_game = 0x00;
int counter = 0; //check notes in the level
unsigned char level = 1;
unsigned char win = 0;
unsigned char lose = 0;
unsigned long change_time = 10;
unsigned char game_start = 0;
unsigned char correct = 0;
unsigned char wrong = 0;

//songs
int play_counter = 0; //counting for playing note
int play_level = 0;// count how many note in the melody
unsigned char play = 0;
unsigned char level1[3] = { 0x01, 0x04,0x02 };// 3 note									1 3 2 
unsigned char level2[5] = { 0x08, 0x01, 0x08, 0x10, 0x02 }; // 5 note						4 1 4 5 2
unsigned char level3[7] = { 0x02, 0x10, 0x08, 0x10, 0x01, 0x04, 0x02 };// 7 note			2 5 4 5 1 3 2  
unsigned char level4[5] = { 0x01, 0x10, 0x02, 0x04, 0x01 };// 5 note, faster				1 5 2 3 1 
unsigned char level5[8] = { 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x02, 0x01 };// 8 note faster 5 4 3 2 3 4 2 1
unsigned char level6[11] = { 0x10, 0x04, 0x01, 0x04, 0x01 , 0x04, 0x01, 0x04, 0x01, 0x04, 0x10 };


//speaker
double wrong_sound[4] = { 146.832, 0.00, 146.832, 0.00 };
double correct_sound[4] = { 1174.659,0.00, 987.767, 0.00 };
//						0      1    2      3     4       5     6       7          8       9          10      11
double winner[16] = { 329.628, 0.00,329.628,0.00,329.628,0.00, 329.628, 261.626,0.00, 293.665, 0.00,329.628,0.00, 293.665,0.00, 329.628 };
int speaker_count = 0;
int j = 0; //counting pulse
int interrupt = 0; //no interrupt
//lcd_text
unsigned char *begin_words;
unsigned char i;
unsigned char len;
unsigned char *leveling; //size need to be n+1
unsigned char lcd_period = 9; //use to change tick time
unsigned char lcd_count = 0;
unsigned char lcd_flash = 0; //flashing counter
unsigned long shift = 0; //shift led in register
unsigned char shift_speed = 10;
//multiplayer
unsigned char mult = 0;
unsigned char thewinner = 0;

struct players
{
	unsigned char score;
	unsigned char on;


};

struct players p1;
struct players p2;


void reset(); //prototype
void speaker_counter();
void win_sound();


/////////////////speaker function//////////////////////////////////////////////////////////////////
void set_PWM(double frequency) {
	// 0.954 hz is lowest frequency possible with this function,
	// based on settings in PWM_on()
	// Passing in 0 as the frequency will stop the speaker from generating sound
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
		if (play == 1 && interrupt == 0)
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
			change_time = 5; //change speed
			tempLED = level4[play_counter];
			play_counter++;
		}
		else if (level == 5)
		{
			change_time = 8; //change speed
			tempLED = level5[play_counter];
			play_counter++;
		}
		else if (level == 6)
		{
			change_time = 7; //change speed
			tempLED = level6[play_counter];
			play_counter++;
		}
		break;
	default:
		break;
	}
}



//game level up
enum level_states { level_init, levelone, leveltwo, levelthree, levelfour, levelfive, levelsix } level_state;
void game_level()
{
	switch (level_state) //transition
	{
	case level_init:
		if (level == 1 && start_game == 1)
		{

			/*
			//testing skip level
			level =5;
			play = 1;
			level_state = levelfive;				//testing
			play_level = 7;
			//level_state = levelone;
			//play_level = 3;
			start_game = 0;
			*/

			level = 1;
			play = 1;
			level_state = levelone;				//testing
			play_level = 3;
			//level_state = levelone;
			//play_level = 3;
			start_game = 0;
			if (mult == 1)
			{
				p1.score = 0;
				p2.score = 0;
				p1.on = 1; //p1 go first	
				p2.on = 0;
			}


		}
		break;
	case levelone:
		if (level == 2)
		{
			play = 1;
			level_state = leveltwo;
			play_level = 5;
			p1.on = 0; //p2 go first
			p2.on = 1;
		}
		break;
	case leveltwo:
		if (level == 3)
		{
			play = 1;
			level_state = levelthree;
			play_level = 7;
			p1.on = 1; //p1 go first
			p2.on = 0;
		}
		break;
	case levelthree:
		if (level == 4)
		{
			play = 1;
			level_state = levelfour;
			play_level = 5;
			p1.on = 0; //p2 go first
			p2.on = 1;
		}
		break;
	case levelfour:
		if (level == 5)
		{
			play = 1;
			level_state = levelfive;
			play_level = 8;
			p1.on = 1; //p1 go first
			p2.on = 0;
		}
		break;
	case levelfive:
		if (level == 6)
		{
			play = 1;
			level_state = levelsix;
			play_level = 11;
			p1.on = 0; //p2 go first
			p2.on = 1;
		}

		break;
	case levelsix:
		if (mult == 1)
		{
			if (p1.score > p2.score)
			{
				thewinner = 1; //p1 win
			}
			else if (p1.score < p2.score)
			{
				thewinner = 2; //p2 win
			}
			else
			{
				thewinner = 3; //fair
			}
		}
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
					if (p1.on == 1) //if p1 is correct
					{
						p1.score += 1;
					}
					else
					{
						p2.score += 1; //if p2 is correct
					}
					level = 2;
					counter = 0; //reset counter
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
					correct = 1;

				}
			}
			else if (press != 0x00 && mult == 0)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
			else if (press != 0x00 && mult == 1)
			{
				if (p1.on == 1)
				{
					p1.on = 0; //p1 lose first round
					play = 1; //play the music
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
				else //p1 and p2 all lose, go to next level
				{
					level = 2;
					counter = 0; //reset counter
					correct = 1;//enable interrupt/////////////////////////////////
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
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
					if (p1.on == 1) //if p1 is correct
					{
						p1.score += 1;
						p1.on = 0;
					}
					else
					{
						p2.score += 1; //if p2 is correct
					}
					level = 3;
					counter = 0;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
					correct = 1;
				}
			}
			else if (press != 0x00 && mult == 0)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
			else if (press != 0x00 && mult == 1)
			{
				if (p2.on == 1)
				{
					p2.on = 0; //p2 lose second round
					play = 1; //play the music
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
				else //p1 and p2 all lose, go to next level
				{
					level = 3;
					counter = 0; //reset counter
					correct = 1;//enable interrupt/////////////////////////////////
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}

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
					if (p1.on == 1) //if p1 is correct
					{
						p1.score += 1;
					}
					else
					{
						p2.score += 1; //if p2 is correct
					}
					level = 4;
					counter = 0;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
					correct = 1;
				}
			}
			else if (press != 0x00 && mult == 0)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
			else if (press != 0x00 && mult == 1)
			{
				if (p1.on == 1)
				{
					p1.on = 0; //p1 lose first round
					play = 1; //play the music
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
				else //p1 and p2 all lose, go to next level
				{
					level = 4;
					counter = 0; //reset counter
					correct = 1;//enable interrupt/////////////////////////////////
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
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
					if (p1.on == 1) //if p1 is correct
					{
						p1.score += 1;
						p1.on = 0;
					}
					else
					{
						p2.score += 1; //if p2 is correct
					}
					level = 5;
					counter = 0; //reset counter
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
					correct = 1;
				}
			}
			else if (press != 0x00 && mult == 0)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
			else if (press != 0x00 && mult == 1)
			{
				if (p2.on == 1)
				{
					p2.on = 0; //p2 lose second round
					play = 1; //play the music
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
				else //p1 and p2 all lose, go to next level
				{
					level = 5;
					counter = 0; //reset counter
					correct = 1;//enable interrupt/////////////////////////////////
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}

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
				if (counter == 8)
				{
					if (p1.on == 1) //if p1 is correct
					{
						p1.score += 1;
					}
					else
					{
						p2.score += 1; //if p2 is correct
					}
					level = 6;
					counter = 0;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
					correct = 1;
				}
			}
			else if (press != 0x00 && mult == 0)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
			else if (press != 0x00 && mult == 1)
			{
				if (p1.on == 1)
				{
					p1.on = 0; //p1 lose first round
					play = 1; //play the music
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
				else //p1 and p2 all lose, go to next level
				{
					level = 6;
					counter = 0; //reset counter
					correct = 1;//enable interrupt/////////////////////////////////
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
			}
		}
		break;
	case levelsix:
		if (mark == 1 && GetBit(PORTB, 0) == 1)
		{
			mark = 0;
			if (level6[counter] == press)
			{
				counter += 1;
				if (counter == 11)
				{
					if (p1.on == 1) //if p1 is correct
					{
						p1.score += 2;
					}
					else
					{
						p2.score += 2; //if p2 is correct
					}
					win = 1;
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0

					//win_sound();
				}
			}
			else if (press != 0x00 && mult == 0)
			{
				lose = 1;
				reset();
				//	PORTB = SetBit(PORTB, 1,1 );
			}
			else if (press != 0x00 && mult == 1)
			{
				if (p2.on == 1)
				{
					p2.on = 0; //p1 lose first round
					play = 1; //play the music
					PORTB = SetBit(PORTB, 0, 0); //set PB0 to 0
				}
				else //p1 and p2 all lose, go to next level
				{
					counter = 0; //reset counter


				}
			}
		}
		break;
	default:
		break;
	}

}

/*
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
	else if ( k == 2)
	{
		tempLED = level1[2];
	}

	k++;

}
*/

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
	if (shift >= shift_speed)
	{
		if (tempLED2 == 0)
		{
			tempLED2 = 0xAA;
		}
		tempLED2 = tempLED2 / 2;
		shift = 0;
	}
	shift++;
	SPDR = tempLED2;        //This should light alternating LEDs
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
		else if (tempA == 0x40 && GetBit(PORTB, 0) == 0)
		{
			lcd_period = 5; //change lcd_period when enter game
			level = 1;
			game_led_state = game_init;
			level_state = level_init;
			start_game = 1;
			play = 1;
			win = 0;
			lose = 0;
			PORTB = SetBit(PORTB, 2, 0);
			PORTB = SetBit(PORTB, 1, 0);
		}
		else if (tempA == 0x40 && GetBit(PORTB, 0) == 1)
		{
			lcd_period = 5; //change lcd_period when enter game
			level = 1;
			game_led_state = game_init;
			level_state = level_init;
			start_game = 1;
			play = 1;
			win = 0;
			lose = 0;
			mult = 1;
			PORTB = SetBit(PORTB, 1, 1);
			PORTB = SetBit(PORTB, 0, 0);
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
	else if (tempLED == 0x00 && win == 0 && correct == 0)
	{
		set_PWM(0.00);
	}


}

//all sound
void win_sound()
{
	if (win == 1)
	{
		if (speaker_count == 16)
		{
			win = 0;
			speaker_count = 0;
		}
		else
		{
			if (speaker_count == 7)
			{
				if (j == 3)
				{

					set_PWM(winner[speaker_count]);
					speaker_count++;
					j = 0;
				}
				else
				{
					j++;
				}
			}

			else
			{
				set_PWM(winner[speaker_count]);
				speaker_count++;
			}

			//PORTB = SetBit(PORTB, 1, 1);

		}


	}

}



void interrupt_fcn()
{

	if (correct == 1)
	{
		if (speaker_count == 4)
		{

			speaker_count = 0;
			correct = 0;
			interrupt = 0;
		}
		else
		{
			interrupt = 1;
			set_PWM(correct_sound[speaker_count]);
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
		if (tempA == 0x40)
		{
			//PORTB = SetBit(PORTB, 1,1);
			lcd_state = lcd_on;
			LCD_ClearScreen();
		}
		break;
	case lcd_on:
		if (lose == 1)
		{
			lcd_state = lcd_lose;
			LCD_ClearScreen();

		}
		else if (win == 1 && mult == 0)
		{
			lcd_state = lcd_win;
			LCD_ClearScreen();
		}
		else if (win == 1 && mult == 1)
		{
			lcd_state = lcd_two;
		}
		break;
	case lcd_single:
		break;
	case lcd_two:
		if (tempA == 0x40)
		{
			//PORTB = SetBit(PORTB, 1,1);
			lcd_state = lcd_on;
			LCD_ClearScreen();
			lcd_flash = 0;
		}
		break;
	case lcd_lose:
		if (tempA == 0x40)
		{
			//PORTB = SetBit(PORTB, 1,1);
			lcd_state = lcd_on;
			LCD_ClearScreen();
			lcd_flash = 0;
		}
		if (lcd_count == 50)
		{
			lcd_state = lcd_begin;
			lcd_count = 0;
		}
		else
		{
			lcd_count++;
		}
		break;
	case lcd_win:
		if (tempA == 0x40)
		{
			//PORTB = SetBit(PORTB, 1,1);
			lcd_state = lcd_on;
			LCD_ClearScreen();
			lcd_flash = 0;
		}
		break;
	default:
		break;

	}
	switch (lcd_state) //action
	{
	case lcd_init:
		break;
	case lcd_begin:
		shift_speed = 15;
		lcd_period = 9; //use to change tick time
		LCD_ClearScreen();
		LCD_DisplayString(1, begin_words);
		if (i <= len) { begin_words++; i++; }
		else { begin_words -= len; i = 1; }
		break;
	case lcd_on:
		shift_speed = 15;
		LCD_DisplayString(1, leveling);
		break;
	case lcd_single:
		break;
	case lcd_two:
		if (thewinner == 1)
		{
			if (lcd_flash >= 3)
			{
				LCD_ClearScreen();
				if (lcd_flash >= 6)
				{
					lcd_flash = 0;
				}
				lcd_flash++;
			}
			else
			{
				LCD_DisplayString(1, "    player1 is    the winner");
				lcd_flash++;
			}
		}
		else if (thewinner == 2)
		{
			if (lcd_flash >= 3)
			{
				LCD_ClearScreen();
				if (lcd_flash >= 6)
				{
					lcd_flash = 0;
				}
				lcd_flash++;
			}
			else
			{
				LCD_DisplayString(1, "    player2 is    the winner!!!");
				lcd_flash++;
			}
		}
		else if (thewinner == 3)
		{
			if (lcd_flash >= 3)
			{
				LCD_ClearScreen();
				if (lcd_flash >= 6)
				{
					lcd_flash = 0;
				}
				lcd_flash++;
			}
			else
			{
				LCD_DisplayString(1, "    Chill~ you guys are even!!!");
				lcd_flash++;
			}
		}
		break;
	case lcd_lose:
		shift_speed = 15;
		if (lcd_flash >= 3)
		{
			LCD_ClearScreen();
			LCD_DisplayString(1, " | | | | | | | || | | | | | | | ");
			if (lcd_flash >= 6)
			{
				lcd_flash = 0;
			}
			lcd_flash++;
		}
		else
		{
			LCD_DisplayString(1, "|  YOU   ARE  |  |  LOSER  !!! |");
			lcd_flash++;
		}

		break;
	case lcd_win:
		shift_speed = 1;
		if (lcd_flash >= 2)
		{
			LCD_ClearScreen();
			if (lcd_flash >= 4)
			{
				lcd_flash = 0;
			}
			lcd_flash++;
		}
		else
		{
			LCD_DisplayString(1, " Winner Winner   Chicken Dinner");
			lcd_flash++;
		}

		break;
	default:
		break;
	}
}

//convert variable into lcd text
void text()
{
	if (mult == 0)
	{
		if (level == 1)
		{
			leveling = "level 1";
		}
		else if (level == 2)
		{
			leveling = "level 2";
		}
		else if (level == 3)
		{
			leveling = "level 3";
		}
		else if (level == 4)
		{
			leveling = "level 4";
		}
		else if (level == 5)
		{
			leveling = "level 5";
		}
		else if (level == 6)
		{
			leveling = "level 6";
		}
	}
	else
	{
		if (level == 1)
		{
			if (p1.on == 1)
			{
				leveling = "level 1: player1";
			}
			else
			{
				leveling = "level 1: player2";
			}
		}
		else if (level == 2)
		{
			if (p2.on == 1)
			{
				leveling = "level 2: player2";
			}
			else
			{
				leveling = "level 2: player1";
			}
		}
		else if (level == 3)
		{
			if (p1.on == 1)
			{
				leveling = "level 3: player1";
			}
			else
			{
				leveling = "level 3: player2";
			}
		}
		else if (level == 4)
		{
			if (p2.on == 1)
			{
				leveling = "level 4: player2";
			}
			else
			{
				leveling = "level 4: player1";
			}
		}
		else if (level == 5)
		{
			if (p1.on == 1)
			{
				leveling = "level 4: player1";
			}
			else
			{
				leveling = "level 4: player2";
			}
		}
		else if (level == 6)
		{
			if (p2.on == 1)
			{
				leveling = "level 6: player2";
			}
			else
			{
				leveling = "level 6: player1";
			}
		}
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
	unsigned char interrupt_elapsed = 0;
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
	//lcd variable end///////////

	LCD_init();

	while (1)
	{
		text();
		//Loop forever
		tempA = ~PINA;
		tempD = ~PIND & 0x1F;

		set_led();
		set_register();
		set_sensor();
		game_level();
		if (interrupt_elapsed >= 2)
		{
			interrupt_fcn();
			interrupt_elapsed = 0;
		}
		if (lcd_elapsed >= lcd_period)
		{
			lcd_game();
			lcd_elapsed = 0;
		}
		if (win_elapsed >= 4)
		{
			win_sound();
			win_elapsed = 0;
		}
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
		interrupt_elapsed += 1;
	}
}
