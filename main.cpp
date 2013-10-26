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
#include <util/delay.h>

// times are multiplied by 50ms
#define ADC_TIMEOUT 200
#define ADC_STARTUP 10
#define ADC_CALIBRATION 1.005

#include <avr/io.h>
#include <avr/interrupt.h>
//#include <avr/eeprom.h> 

#include <stdint.h>

#include "bitop.h"
#include "shiftOut.h"
#include "io.h"

using namespace avr_cpp_lib;

uint8_t spi_transceive(const uint8_t send) {
	SPDR = send;
	while (BITCLEAR(SPSR, SPIF));
	return SPDR;
}

#include "cc1101.h"
InputPin gdo0(&DDRB, &PINB, PB1);
InputPin gdo1(&DDRD, &PINB, PB4);
CC1101 cc(&spi_transceive, OutputPin(&DDRB, &PORTB, PB0), gdo1);

static inline void ccinit() {
	uint8_t config[3];

	cc.reset();

	// GDO2 to high impedance
	config[0] = 0x2E;
	// GDO1 assert when CRC packet received
	config[1] = 0x07;
	// GDO0(PB1) assert when sync word is sent, deasserts at the end of packet
	config[2] = 0x06;
	cc.writeBurst(CC1101::IOCFG2, config, 3);
	
	// PKTLEN to 1
	config[0] = 1;
	// PKTCTRL1 no append status, crc autoflush
	config[1] = BIT(3);
	// PKTCTRL0 white data, normal mode, crc fixed length
	config[2] = BIT(6) | BIT(2);
	cc.writeBurst(CC1101::PKTLEN, config, 3);
	
	// set frequency in FREQ[0-2]
	static const uint8_t F_CRYSTAL = 27;
	static const uint32_t F_CARRIER = 433;
	static const uint32_t FREQ = (F_CARRIER << 16)/F_CRYSTAL;
	config[0] = FREQ>>16;
	config[1] = (FREQ>>8) & 0xff;
	config[2] = FREQ & 0xff;
	cc.writeBurst(CC1101::FREQ2, config, 3);
	
	// set manchester encoding, 2-fsk, 16/16 sync
	cc.write(CC1101::MDMCFG2, BIT(3) | 0b010);
	
	// cca mode always, rx to rx, tx to rx
	cc.write(CC1101::MCSM1, 0b00001111);

	// calibrate after setting frequency
	cc.command(CC1101::SCAL);
	_delay_ms(1);	
}

#define CC_A (6)
#define CC_B (5)
#define CC_IDLE_STATE (4)
#define CC_MAX_TRIES (100)
#define CC_TIMEOUT (200);
static uint8_t cc_packet = 0;
static uint8_t cc_state = CC_IDLE_STATE;
static uint8_t cc_tries = 0;
static uint8_t cc_timeout = CC_TIMEOUT;
static uint8_t cc_failures = 0;
static uint8_t ir_broken_setting = 3;
static uint8_t ir_delay_setting = 20;
static inline void sendPacket(bool button, bool ir) {
	static uint8_t i = 0;
	if (cc_state == CC_IDLE_STATE) {
		cc_state = 0;
		cc_tries = CC_MAX_TRIES;
		cc_timeout = CC_TIMEOUT;
		cc_packet = i;
		if (button) {
			SETBIT(cc_packet, CC_A);
		}
		if (ir) {
			SETBIT(cc_packet, CC_B);
		}
		i++;
		if (i > 3) {
			i = 0;
		}
	}
}

#define IR_ERROR 13

volatile bool voltage_warning = false;

uint8_t button_state = 0;

// function prototypes
static void shutdown();

// led output pin
OutputPin ledOut(&DDRC, &PORTC, PC2);

// timer counts
static volatile uint8_t ir_count = 0;
static volatile uint16_t ms50_count = 0;
static volatile uint8_t cc_count = 0;

// SHUTDOWN ROUTINE
static void shutdown() {
	// disable interrupts
	cli();

	// going to shutdown, turn off display
	ledOut.clear();
	
	// wait for button release
	while (BITCLEAR(PINC, PC1));
	CLEARBIT(PORTC, PC0);

	// infinite loop
	for (;;);
}

int main() {
	// INIT
	// soft power button
	SETBIT(DDRC, PC0);
	SETBIT(PORTC, PC0);
	SETBIT(PORTC, PC1);
	while (BITCLEAR(PINC, PC1));
	_delay_ms(5);
	
	// pwm for leds
	//SETBIT(DDRD, PD5);
	//SETBIT(DDRD, PD6);
	TCCR0A = 0b01010010;
	TCCR0B = 0b00000001;
	OCR0A = 105;
	OCR0B = 105;
	TIMSK0 = 0b00000010; // interrupt for ir and ms count

	// adc measurement
	ADMUX = 0b11000111;
	ADCSRA = 0b10001111;

	ledOut.set();

	// enable interrupts
	sei();

	// init spi (ss pin must be output for some reason in master mode)
	SETBIT(DDRB, PB2);
	SETBIT(DDRB, PB3);
	SETBIT(DDRB, PB5);
	SPCR = 0b01010000;
	SPSR = BIT(SPI2X);

	// cc1101 config
	ccinit();
	cc.command(CC1101::SRX);

	// ir stuff
	uint16_t ir_delay = 0;
	uint8_t ir_broken = 100;
	uint8_t ir_state = 0;
	ir_count = 0;
		
	uint16_t adc_timeout = ADC_STARTUP;
	uint8_t led_state = 0;

	// main loop with ir sensing
	for (;;) {
		// transceiver state machine
		switch (cc_state) {
		case 0:
			cc.write(CC1101::FIFO, cc_packet);
			cc.command(CC1101::STX);
			cc_state = 1;
			break;
		case 1:
			if (gdo0.isSet()) {
				cc_state = 2;
			}
			break;
		case 2:
			if (!gdo0.isSet()) {
				cc_state = 3;
				cc_count = 0;
			}
			break;
		case 3:
			if (gdo1.isSet()) {
				uint8_t v = cc.read(CC1101::FIFO);
				ir_broken_setting = (v & 0b011111) + 1;
				ir_delay_setting = ((v>>5) + 1) * 20;
				cc_state = CC_IDLE_STATE;
				cc_failures = 0;
			}
			if (cc_count >= 119) {
				cc_tries--;
				if (cc_tries > 0) {
					cc.command(CC1101::STX);
					cc_state = 0;
				} else {
					cc_state = CC_IDLE_STATE;
					cc_failures++;
					if (cc_failures > 10) {
						shutdown();
					}
				}
			}
			break;
		}

		// ir state machine
		switch (ir_state) {
			case 0: // check for signal 0 and timeout
				if (BITCLEAR(PIND, PD7)) {
					ir_state = 1;
				}
				if (ir_count >= 85) {
					ir_state = IR_ERROR;
				}
				break;
			case 1: // wait till end of signal 0
				if (BITSET(PIND, PD7)) {
					ir_state = 2;
					ir_count = 0;
				}
				break;
			case 2: // wait for required time and transmit 0
				if (ir_count >= 24) {
					ir_state = 3;
					SETBIT(DDRD, PD5);
					ir_count = 0;
				}
				break;
			case 3: // stop led 0
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
			case 5: // check for signal 1 and timeout
				if (BITCLEAR(PINB, PB6)) {
					ir_state = 6;
				}
				if (ir_count >= 130) {
					ir_state = IR_ERROR;
				}
				break;
			case 6: // wait till end of signal 1
				if (BITSET(PINB, PB6)) {
					ir_state = 7;
					ir_count = 0;
				}
				break;
			case 7: // wait required time, transmit 1
				if (ir_count >= 24) {
					ir_state = 8;
					SETBIT(DDRD, PD6);
					ir_count = 0;
				}
				break;
			case 8:
				if (ir_count >= 20) { // stop led 1
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
			case 10: // check for signal 2 and timeout
				if (BITCLEAR(PINB, PB7)) {
					ir_state = 11;
					ir_broken = 0;
				}
				if (ir_count >= 130) {
					ir_state = IR_ERROR;
				}
				break;
			case 11: // wait till end of signal 2
				if (BITSET(PINB, PB7)) {
					ir_state = 12;
					ir_count = 0;
				}
				break;
			case 12: // wait till after possible beginning (just)
				if (ir_count >= 18) {
					ir_state = 0;
				}
				break;
			case IR_ERROR:
				if (ir_broken < 100) {
					ir_broken++;
				}
				ir_count = 0;
				ir_state = 0;
				break;
		}
		
		// ir broken
		if (ir_broken == ir_broken_setting && ir_delay == 0) {
			ir_broken++;

			sendPacket(false, true);

			ir_delay = ir_delay_setting;
		}

		// execute every 50ms
		if (ms50_count >= 3810) {
			ms50_count = 0;
			
			// delay another action upon detection
			if (ir_delay > 0) {
				ir_delay--;
			}

			// blinking led
			if (voltage_warning) {
				led_state++;
				if (led_state == 16) {
					ledOut.set();
				} else if (led_state == 18) {
					ledOut.clear();
					led_state = 0;
				}
			}
			
			// adc
			if (adc_timeout > 0) {
				adc_timeout--;
			} else {
				adc_timeout = ADC_TIMEOUT;
				SETBIT(ADCSRA, ADSC);
			}

			// cc timeout (10s)
			if (cc_timeout > 0) {
				cc_timeout--;
			} else {
				sendPacket(false, false);
			}
			
			// button
			if (BITSET(PINC, PC1)) {
				button_state = 0;
			} else {
				// button pressed
				if (button_state == 1) {
					button_state ++;
					
					sendPacket(true, false);
				}

				// shutdown if pressed for 1s
				if (button_state >= 21) {
					shutdown();
				}
						
				// button pressed
				button_state++;
			}
		}
	}
}

ISR(TIMER0_COMPA_vect) {
	ir_count++;
	ms50_count++;
	cc_count++;
}

ISR(ADC_vect) {
	uint16_t tmp = ADC;
	if (tmp < (uint16_t)(900u*ADC_CALIBRATION)) {
		voltage_warning = true;
		if (tmp < (uint16_t)(850u*ADC_CALIBRATION)) {
			shutdown();
		}
	}
}
