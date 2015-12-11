/*
 * main.cpp
 *
 *  Created on: 22.07.2015
 *      Author: Shcheblykin
 */
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

/** �������� ������.
 *
 * 	1. ������, �������� ������ ������� � 60:00.
 * 	2. ��� "������" ������� ������� ����� ���������� � 10 ���.
 * 	3. ��� "������" ������� ������� ����� ��������������� -> �����������.
 * 	4. ��� "������" ������ ��������� ������� -> �����.
 *
 * 	������:
 *	- ������������ ��� ����������� �� ���� ����;
 *
 *	�����:
 *	- ��������� �������;
 *	- ������������ ����1, ��� ���������� �������� ������������;
 *
 *	�����������:
 * 	- ����� ���������������;
 * 	- ������������ ����2.
 */


// ������� ��������� ������ �����
typedef enum {
	STATE_WAIT,			// �������� ������
	STATE_COUNT_NORM,	// ����� �������� � ���������� ������
	STATE_COUNT_FAST,	// ����� �������� � ���������� ������
	STATE_EXPLODED,		// ����� ����������
	STATE_DEFUSED		// ����� ���������
} STATE;

// �������
static const uint8_t u8Wire1 = (1 << PC4);	// ������ ������
static const uint8_t u8Wire2 = (1 << PC5);	// ������ ������
static const uint8_t u8Wire3 = (1 << PC6);	// ���������
static const uint8_t u8CntWire = 250;		// ���-�� �������� ������ �������

// ������
static const uint8_t u8OutBoom = (1 << PB7);	// ����� ����������� �����
static const uint8_t u8OutDefuse = (1 << PC7);	// ����� ����������� �����

// �����
static const uint8_t u8InStart = (1 << PB6);	// ���� ������� �����

// ����������
static const uint8_t u8Digit1 = (1 << PB0);	// ������ ���������
static const uint8_t u8Digit2 = (1 << PB3);	// ������ ���������
static const uint8_t u8Digit3 = (1 << PB4);	// ������ ���������
static const uint8_t u8Digit4 = (1 << PB5);	// ��������� ���������
static const uint8_t u8Digits = (u8Digit1 | u8Digit2 | u8Digit3 | u8Digit4);

static const uint16_t u8BoomInit = 3600;// ��������� �������� ������� � ��������
static const uint8_t u8Tick = 8;		// ���-�� ����� ������� 1 ��� 1 �������
static const uint8_t u8SpeedNo	= 0;	// ���� ������� ����������
static const uint8_t u8SpeedNom = 1;	// ���� ������� ����������
static const uint8_t u8SpeedMax = 10;	// ���� ������� ����������

// ������ ��� ������ ����� (����������� ��� ������ ����������)
static const uint8_t u8Point = (1 << PD0);
// ������� ��������� �������� �����������
static const uint8_t u8OddA = (1 << PD2);
static const uint8_t u8OddB = (1 << PD1);
static const uint8_t u8OddC = (1 << PD7);
static const uint8_t u8OddD = (1 << PD6);
static const uint8_t u8OddE = (1 << PD5);
static const uint8_t u8OddF = (1 << PD3);
static const uint8_t u8OddG = (1 << PD4);
// ������� ��������� ������ �����������
static const uint8_t u8EvenA = (1 << PD3);
static const uint8_t u8EvenB = (1 << PD4);
static const uint8_t u8EvenC = (1 << PD5);
static const uint8_t u8EvenD = (1 << PD6);
static const uint8_t u8EvenE = (1 << PD7);
static const uint8_t u8EvenF = (1 << PD2);
static const uint8_t u8EvenG = (1 << PD1);

// ������ ������ �������� ������� ����������, ���������� � �������� �������
static const uint8_t u8Digit[] PROGMEM = {
		u8Digit1,	// ������� �����
		u8Digit2,	// ������
		u8Digit3,	// ������� ������
		u8Digit4	// �������
};

// ������ ������������ �������� ��� �������� �����������
static const uint8_t u8SymOdd[] PROGMEM = {
		u8OddA | u8OddB | u8OddC | u8OddD | u8OddE | u8OddF,			// '0'
		u8OddB | u8OddC,												// '1'
		u8OddA | u8OddB | u8OddD | u8OddE | u8OddG,						// '2'
		u8OddA | u8OddB | u8OddC | u8OddD | u8OddG,						// '3'
		u8OddB | u8OddC | u8OddF | u8OddG,								// '4'
		u8OddA | u8OddC | u8OddD | u8OddF | u8OddG,						// '5'
		u8OddA | u8OddC | u8OddD | u8OddE | u8OddF | u8OddG,			// '6'
		u8OddA | u8OddB | u8OddC,										// '7'
		u8OddA | u8OddB | u8OddC | u8OddD | u8OddE | u8OddF | u8OddG,	// '8'
		u8OddA | u8OddB | u8OddC | u8OddD | u8OddF | u8OddG,			// '9'
		0b00000000														// ' '
};

// ������ ������������ �������� ��� ������ �����������
static const uint8_t u8SymEven[] PROGMEM = {
		u8EvenA | u8EvenB | u8EvenC | u8EvenD | u8EvenE | u8EvenF,		// '0'
		u8EvenB | u8EvenC,												// '1'
		u8EvenA | u8EvenB | u8EvenD | u8EvenE | u8EvenG,				// '2'
		u8EvenA | u8EvenB | u8EvenC | u8EvenD | u8EvenG,				// '3'
		u8EvenB | u8EvenC | u8EvenF | u8EvenG,							// '4'
		u8EvenA | u8EvenC | u8EvenD | u8EvenF | u8EvenG,				// '5'
		u8EvenA | u8EvenC | u8EvenD | u8EvenE | u8EvenF | u8EvenG,		// '6'
		u8EvenA | u8EvenB | u8EvenC,									// '7'
		u8EvenA | u8EvenB | u8EvenC | u8EvenD | u8EvenE | u8EvenF | u8EvenG,// '8'
		u8EvenA | u8EvenB | u8EvenC | u8EvenD | u8EvenF | u8EvenG,		// '9'
		0b00000000														// ' '
};

//
void low_level_init() __attribute__((__naked__)) __attribute__((section(".init3")));
static void printValue();

// ����� �������� �������� ��� ������ �� �����, ���������� � �������� �������
static uint8_t u8Buf[] = {10, 10, 10, 10};
// ����������(true) / �� ���������� (false) �����
static bool point = false;

// ������������� �������� �������� �� �����
void setTime(uint16_t val) {
	uint8_t tmp = val % 60;
	u8Buf[3] = tmp % 10;
	u8Buf[2] = tmp / 10;

	tmp = val / 60;
	u8Buf[1] = tmp % 10;
	u8Buf[0] = tmp / 10;
}

// ����� �������� �� ����������
void printValue() {
	static uint8_t digit = 0;

	uint8_t p = point ? u8Point : 0;

	// ��� �������� �� ������� � ������� ������� �������� ��� ��������
	PORTB &= ~u8Digits;
	_delay_us(500);
	PORTB |= pgm_read_byte(&u8Digit[digit]);

	// ��� ������ � �������� �������� ������� ����������
	if (digit % 2 == 0) {
		PORTD = pgm_read_byte(&u8SymOdd[u8Buf[digit]]) | p;
	} else {
		PORTD = pgm_read_byte(&u8SymEven[u8Buf[digit]]) | p;
	}

	// ������� � ���������� �������
	digit = (digit >= (sizeof(u8Buf) - 1)) ? 0 : digit + 1;
}


int main() {
	STATE state = STATE_WAIT;
	uint8_t tick1s = 0;	// ������� 1�
	uint8_t speed = u8SpeedNo;	// �������� ����� �� ��� �������
	uint8_t cntWire = 0;
	uint8_t prevWire = 0;
	uint16_t time = u8BoomInit;	// ����� �� ������ �����

	sei();

	while(1) {

		// ������� ���� ��� ������������ ���������
		if (TIFR0 & (1 << OCF0A)) {
			printValue();
			TIFR0 = (1 << OCF0A);
		}

		// ��������� ���� ��� �������� ������
		if (TIFR1 & (1 << OCF1A)) {
			tick1s += speed;
			if (tick1s >= u8Tick) {	// ������ �������
				if  (time > 0) {
					time--;
				}
				tick1s = 0;
			}
			setTime(time);
			TIFR1 = (1 << OCF1A);
		}

		uint8_t wire = PINC;
		if (prevWire == wire) {
			if (cntWire <= u8CntWire) {
				cntWire++;
				wire = 0;
			}
		} else {
			cntWire = 0;
			prevWire = wire;
			wire = 0;
		}

		switch(state) {
			case STATE_COUNT_NORM: {
				point = true;
				speed = u8SpeedNom;
				if ((wire & u8Wire3) == u8Wire3) {
					state = STATE_EXPLODED;		// �������� �������� ������
				} else if ((wire & u8Wire2) == u8Wire2) {
					state = STATE_EXPLODED;		// �������� �������� ������
				} else if ((wire & u8Wire1) == u8Wire1) {
					state = STATE_COUNT_FAST;	// �������� ������ ������
				}
			} break;

			case STATE_COUNT_FAST: {
				speed = u8SpeedMax;
				if ((wire & u8Wire3) == u8Wire3) {
					state = STATE_EXPLODED;		// �������� �������� ������
				} else if ((wire & u8Wire1) == 0) {
					state = STATE_EXPLODED;		// ������ �������� �������
				} else if ((wire & u8Wire2) == u8Wire2) {
					state = STATE_DEFUSED;		// �������� ������ ������
				}
			} break;

			case STATE_DEFUSED: {
				point = false;
				speed = u8SpeedNo;
				PORTC &= ~u8OutDefuse;
				state = STATE_WAIT;
			} break;

			case STATE_EXPLODED: {
				point = false;
				time = 0;
				speed = u8SpeedNo;
				PORTB &= ~u8OutBoom;
				state = STATE_WAIT;
			} break;

			case STATE_WAIT: {
				// ������ ������ �����
				if ((PINB & u8InStart) == 0) {
					tick1s = 0;
					time = u8BoomInit;
					PORTB |= u8OutBoom;
					PORTC |= u8OutDefuse;
					state = STATE_COUNT_NORM;
				}
			} break;
		}
	}
}


/**	��������� ������������� ���������.
 *
 * 	��-��������� �� ����������� �� �������� ������, � ������ ������ 16���. ���
 * 	���� ������� ���� ������� ������� �� 8. �.�. � ����� �������� �������
 * 	������� � 2���. �������� ������ ���� ����� ������ ��� ������ ������� ����,
 * 	��� ���� �������� � ���������. ������� �������� �� ���� 2���.
 *
 * 	������ 0 ����������� ������ 3.2 ��.
 * 	������ 1 ����������� ������ 125 ��.
 */
void low_level_init() {

	// PORTB
	// ����� ���������� ���.1
	// ����� ������������ ����� ���.0
	// ���� ������� ����� ���.0
	DDRB = u8Digit1 | u8Digit2 | u8Digit3 | u8Digit4 | u8OutBoom;
	PORTB = u8Digit1 | u8OutBoom | u8InStart;

	// PORTC
	// ������� ��� �����, ���.1 ��� ������ �������
	// ����� ����������� ����� ���.0
	DDRC = u8OutDefuse;
	PORTC = u8Wire1 | u8Wire2 | u8Wire3 | u8OutDefuse;

	// PORTD
	// �������� ����������, ��������� ����� ��� ���. 1
	DDRD = 0xFF;
	PORTD = 0x00;

	// TIMER0
	// CTC, (2Mhz / 64) / 100 = 3.2ms
	OCR0A = 100 - 1;
	TCCR0A = (1 << WGM01) | (0 << WGM00);
	TCCR0B = (0 << WGM02) | (0 << CS02) | (1 << CS01) | (1 << CS00);

	// TIMER1
	// CTC, (2Mhz / 8) / 31250 = 0.125s
	OCR1A = 31250 - 1;
	TCCR1A = (0 << WGM11) | (0 << WGM10);
	TCCR1B = (0 << WGM13) | (1 << WGM12);
	TCCR1B |= (0 << CS12) | (1 << CS11) | (0 << CS10);
};


