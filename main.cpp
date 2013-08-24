/* File: main.cpp
 * Contains base main function and usually all the other stuff that avr does...
 */
/* Copyright (c) 2012-2013 Domen Ipavec (domen.ipavec@z-v.si)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 
 #define F_CPU 8000000UL  // 8 MHz
//#include <util/delay.h>

#include <avr/io.h>
#include <avr/interrupt.h>
//#include <avr/pgmspace.h>
//#include <avr/eeprom.h> 

#include <stdint.h>

#include "bitop.h"
#include "shiftOut.h"
#include "io.h"

volatile uint8_t flags = 0;
#define MS_FLAG 0
#define VOLTAGE_WARNING 1
#define VOLTAGE_CUTOFF 2
#define IR_CONNECTED 3

volatile uint8_t ir_count = 0;

#define DISPLAY_DOT_0 0
#define DISPLAY_DOT_1 1
#define DISPLAY_DOT_2 2
#define DISPLAY_DOT_3 3
#define DISPLAY_ZERO_0 4
#define DISPLAY_ZERO_1 5
#define DISPLAY_ZERO_2 6
#define DISPLAY_ZERO_3 7

// global buffers
volatile uint8_t parts[4];

void displayWrite(uint8_t part0, uint8_t part1, uint8_t flags) {
	*((uint32_t *)&parts) = 0;
	switch (part1 % 10) {
		case 0:
			if (BITSET(flags, DISPLAY_ZERO_3)) {
				SETBITS(parts[0], BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7));
				SETBIT(parts[1], 1);
			}
			break;
		case 2:
			SETBITS(parts[0], BIT(3) | BIT(4) | BIT(6) | BIT(7));
			SETBIT(parts[1], 2);
			break;
		case 4:
			SETBITS(parts[0], BIT(5) | BIT(6));
			SETBITS(parts[1], BIT(1) | BIT(2));
			break;
		case 6:
			SETBIT(parts[0], 3);
		case 5:
			SETBITS(parts[0], BIT(4) | BIT(5) | BIT(7));
			SETBITS(parts[1], BIT(1) | BIT(2));
			break;
		case 8:
			SETBIT(parts[0], 3);
		case 9:
			SETBIT(parts[1], 1);
		case 3:
			SETBIT(parts[1], 2);
			SETBIT(parts[0], 4);
		case 7:
			SETBIT(parts[0], 7);
		case 1:
			SETBITS(parts[0], BIT(5) | BIT(6));
			break;
	}
	
	switch ((part1 / 10) % 10) {
		case 0:
			if (BITSET(flags, DISPLAY_ZERO_2)) {
				SETBITS(parts[0], BIT(0) | BIT(1) | BIT(2));
				SETBITS(parts[1], BIT(3) | BIT(4) | BIT(6));
			}
			break;
		case 2:
			SETBITS(parts[0], BIT(0) | BIT(1));
			SETBITS(parts[1], BIT(3) | BIT(4) | BIT(7));
			break;
		case 4:
			SETBIT(parts[0], 2);
			SETBITS(parts[1], BIT(3) | BIT(6) | BIT(7));
			break;
		case 6:
			SETBIT(parts[0], 0);
		case 5:
			SETBITS(parts[0], BIT(1) | BIT(2));
			SETBITS(parts[1], BIT(4) | BIT(6) | BIT(7));
			break;
		case 8:
			SETBIT(parts[0], 0);
		case 9:
			SETBIT(parts[1], 6);
		case 3:
			SETBIT(parts[1], 7);
			SETBIT(parts[0], 1);
		case 7:
			SETBIT(parts[1], 4);
		case 1:
			SETBIT(parts[1], 3);
			SETBIT(parts[0], 2);
			break;
	}

	switch (part0 % 10) {
		case 0:
			if (BITSET(flags, DISPLAY_ZERO_1)) {
				SETBITS(parts[2], BIT(0) | BIT(1) | BIT(3));
				SETBITS(parts[3], BIT(5) | BIT(6) | BIT(7));
			}
			break;
		case 2:
			SETBITS(parts[2], BIT(1) | BIT(0) | BIT(4));
			SETBITS(parts[3], BIT(5) | BIT(6));
			break;
		case 4:
			SETBITS(parts[2], BIT(0) | BIT(3) | BIT(4));
			SETBIT(parts[3], 7);
			break;
		case 6:
			SETBIT(parts[3], 5);
		case 5:
			SETBITS(parts[2], BIT(1) | BIT(3) | BIT(4));
			SETBITS(parts[3], BIT(6) | BIT(7));
			break;
		case 8:
			SETBIT(parts[3], 5);
		case 9:
			SETBIT(parts[2], 3);
		case 3:
			SETBIT(parts[2], 4);
			SETBIT(parts[3], 6);
		case 7:
			SETBIT(parts[2], 1);
		case 1:
			SETBIT(parts[2], 0);
			SETBIT(parts[3], 7);
			break;
	}
	
	switch ((part0 / 10) % 10) {
		case 0:
			if (BITSET(flags, DISPLAY_ZERO_0)) {
				SETBITS(parts[2], BIT(5) | BIT(6));
				SETBITS(parts[3], BIT(0) | BIT(2) | BIT(3) | BIT(4));
			}
			break;
		case 2:
			SETBITS(parts[2], BIT(5) | BIT(6));
			SETBITS(parts[3], BIT(1) | BIT(2) | BIT(3));
			break;
		case 4:
			SETBIT(parts[2], 5);
			SETBITS(parts[3], BIT(0) | BIT(1) | BIT(4));
			break;
		case 6:
			SETBIT(parts[3], 2);
		case 5:
			SETBIT(parts[2], 6);
			SETBITS(parts[3], BIT(0) | BIT(1) | BIT(3) | BIT(4));
			break;
		case 8:
			SETBIT(parts[3], 2);
		case 9:
			SETBIT(parts[3], 0);
		case 3:
			SETBITS(parts[3], BIT(1) | BIT(3));
		case 7:
			SETBIT(parts[2], 6);
		case 1:
			SETBIT(parts[2], 5);
			SETBIT(parts[3], 4);
			break;
	}
	
	if (BITSET(flags, DISPLAY_DOT_0)) {
		SETBIT(parts[2], 7);
	}
	if (BITSET(flags, DISPLAY_DOT_1)) {
		SETBIT(parts[2], 2);
	}
	if (BITSET(flags, DISPLAY_DOT_2)) {
		SETBIT(parts[1], 5);
	}
	if (BITSET(flags, DISPLAY_DOT_3)) {
		SETBIT(parts[1], 0);
	}
}

int main() {
	// INIT
	// soft power button
	SETBIT(DDRC, PC0);
	SETBIT(PORTC, PC0);
	SETBIT(PORTC, PC1);
	
	// timer 1 for 1 ms interrupts
	TCCR1A = 0;
	TCCR1B = 0b00001010; // divider 8, CTC mode 
	OCR1A = 1000; // top is 1000
	TIMSK1 = 0b00000010; // ocie1a interrupt
	
	// pwm for leds
	//SETBIT(DDRD, PD5);
	//SETBIT(DDRD, PD6);
	TCCR0A = 0b01010010;
	TCCR0B = 0b00000001;
	OCR0A = 105;
	OCR0B = 105;
	TIMSK0 = 0b00000010; // interrupt for ir count

	// adc measurement
	ADMUX = 0b11000111;
	ADCSRA = 0b10001111;

	// display output pins
	avr_cpp_lib::OutputPin displayData(&DDRD, &PORTD, PD2);
	avr_cpp_lib::OutputPin displayClock(&DDRD, &PORTD, PD3);
	avr_cpp_lib::OutputPin displayLatch(&DDRD, &PORTD, PD4);	

	// led output pin
	avr_cpp_lib::OutputPin ledOut(&DDRC, &PORTC, PC2);

	// enable interrupts
	sei();

	uint16_t button_state = 0;
	uint8_t pwm_state = 0;
	uint8_t pwm_bit = 0;
	uint16_t adc_state = 0;
	uint16_t led_state = 0;
	uint8_t ir_state = 0;

	for (;;) {

		// ir state machine
		switch (ir_state) {
			case 0: // not connected, wait
				CLEARBIT(flags, IR_CONNECTED);
				if (BITCLEAR(PIND, PD7)) {
					ir_state = 1;
				}
				break;
			case 1: // wait till end of signal
				if (BITSET(PIND, PD7)) {
					ir_state = 2;
					ir_count = 0;
				}
				break;
			case 2: // wait for required time and transmit
				if (ir_count >= 24) {
					ir_state = 3;
					SETBIT(DDRD, PD5);
					ir_count = 0;
				}
				break;
			case 3: // stop led
				if (ir_count >= 20) {
					CLEARBIT(DDRD, PD5);
					ir_state = 4;
				}
				break;
			case 4: // wait till after possible beginning (just)
				if (ir_count >= 64) {
					ir_state = 5;
				}
				break;
			case 5: // check for signal and timeout
				if (BITCLEAR(PINB, PB6)) {
					ir_state = 6;
				}
				if (ir_count >= 130) {
					ir_state = 0;
				}
				break;
			case 6: // wait till end of signal
				if (BITSET(PINB, PB6)) {
					ir_state = 7;
					ir_count = 0;
				}
				break;
			case 7: // wait required time, transmit
				ledOut.clear();
				if (ir_count >= 24) {
					ir_state = 8;
					SETBIT(DDRD, PD6);
					ir_count = 0;
				}
				break;
			case 8:
				if (ir_count >= 20) { // stop led
					CLEARBIT(DDRD, PD6);
					ir_state = 9;
					ir_count = 0;
				}
				break;
			case 9: // wait till after possible beginning (just)
				if (ir_count >= 64) {
					ir_state = 10;
				}
				break;
			case 10: // check for signal and timeout
				if (BITCLEAR(PINB, PB7)) {
					ir_state = 11;
				}
				if (ir_count >= 130) {
					ir_state = 0;
				}
				break;
			case 11: // wait till end of signal
				if (BITSET(PINB, PB7)) {
					ir_state = 12;
					ir_count = 0;
				}
				break;
			case 12: // wait till after possible beginning (just)
				if (ir_count >= 18) {
					ir_state = 13;
				}
				break;
			case 13: // check for signal and timeout
				if (BITCLEAR(PIND, PD7)) {
					ir_state = 1;
					SETBIT(flags, IR_CONNECTED);
								ledOut.set();

				}
				if (ir_count >= 85) {
					ir_state = 0;
				}
				break;
		}

			
		// pwm of leds
		if (pwm_state < 4) {
			if (pwm_bit == 16) {
				pwm_state++;
				pwm_bit = 0;	
				displayData.clear();
				displayLatch.set();
				displayLatch.clear();
				/*ledOut.clear();*/
			}
			
			if (pwm_state == 0) {
				if (BITSET(parts[pwm_bit/8], pwm_bit%8)) {
					displayData.set();
				} else {
					displayData.clear();
				}
				displayClock.set();
				displayClock.clear();
			} else if (pwm_state == 1) {
				displayClock.set();
				displayClock.clear();
			}
			pwm_bit++;
		} else {
			/*if (BITSET(flags, VOLTAGE_WARNING)) {
			  if (led_state > 300) {
			  ledOut.set();
			  if (led_state > 400) {
			  led_state = 0;
			  }
			  }
			  } else {
			  ledOut.set();
			  }*/
			pwm_state = 0;
		}
		
		if (BITSET(flags, MS_FLAG)) {
			CLEARBIT(flags, MS_FLAG);
			
			// test of ir
			uint8_t a = 0;
			uint8_t b = 0;
			if (BITSET(flags, IR_CONNECTED)) {
				a += 1;
			}
			displayWrite(b, a, 0);

			// led blinking
			led_state++;
			
			// adc
			if (adc_state == 2000) {
				adc_state = 0;
				SETBIT(ADCSRA, ADSC);
			} else {
				adc_state++;
			}
			
			// voltage cutoff
			if (BITSET(flags, VOLTAGE_CUTOFF)) {
				break;
			}
			
			// button
			if (BITSET(PINC, PC1)) {
				button_state = 0;
			} else {
				// shutdown if pressed for 1s				
				if (button_state >= 1000) {
					break;
				}

				// button pressed
				button_state++;
			}
		}
	}
	
	// going to shutdown, turn off display
	avr_cpp_lib::shiftOut(&displayData, &displayClock, 0);
	avr_cpp_lib::shiftOut(&displayData, &displayClock, 0);
	avr_cpp_lib::shiftOut(&displayData, &displayClock, 0);
	avr_cpp_lib::shiftOut(&displayData, &displayClock, 0);
	displayLatch.set();
	displayLatch.clear();
	ledOut.clear();
	
	// wait for button release
	while (BITCLEAR(PINC, PC1));
	CLEARBIT(PORTC, PC0);
}

ISR(TIMER1_COMPA_vect) {
	SETBIT(flags, MS_FLAG);
}

ISR(TIMER0_COMPA_vect) {
	ir_count++;
}

ISR(ADC_vect) {
	static uint8_t count = 0;
	static uint16_t accumulator = 0;

	accumulator += ADC;
	count++;
	if (count > 59) {
		accumulator /= 60;
		
		if (accumulator < 900) {
			SETBIT(flags, VOLTAGE_WARNING);
			if (accumulator < 850) {
				SETBIT(flags, VOLTAGE_CUTOFF);
			}
		}
		
		count = 0;
		accumulator = 0;
	} else {
		SETBIT(ADCSRA, ADSC);
	}
}
