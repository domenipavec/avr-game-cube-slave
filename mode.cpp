/* File: mode.cpp
 * Everything for mode management
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
 
#include "mode.h"

Mode m;

extern Display display;

void nothing() {}

bool timerActive = false;
bool dispMin = false;
uint8_t secondTimeout;
void timerBroken() {
	if (timerActive) {
		timerActive = false;
		dispMin = false;
	} else {
		timerActive = true;
		display.zero();
	}
}
void timerms10() {
	if (timerActive) {
		if (dispMin) {
			if (secondTimeout == 0) {
				secondTimeout = 100;
				display.increase(10,6);
			}
			secondTimeout--;
		} else {
			if (display.increase(10,10,10,6)) {
				display.set(0,0,1,0);
				dispMin = true;
				secondTimeout = 100;
			}
		}
	}
}

void counterBroken() {
	display.increaseLS();
}

Mode * getMode(uint8_t i) {
	m.ds = DisplaySettings();
	m.irDelay = 50;
	m.broken = nothing;
	m.ms10 = nothing;
	m.button = nothing;
	switch (i) {
	case 0: // timer mode
		m.ds = DisplaySettings(true, true, true, false, false, false, true);
		m.irDelay = 200;
		m.broken = timerBroken;
		break;
	case 1: // counter mode
		m.ds = DisplaySettings(true);
		m.irDelay = 500;
		m.broken = counterBroken;
		break;
	case 2: // alarm mode
		m.ds = DisplaySettings(false, false, false, false, true);
		break;
	}
	return &m;
}
