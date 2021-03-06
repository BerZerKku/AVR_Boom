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
 *	����1 - ���������� ������� �����������, � ������ ������ ���������������.
 *	���� ���� ������� (�.�. ���� ����� ��� ����� ���), ������� �������. 
 *
 *	����2 - ������������ ��� ������ ��������� �������, ����� �������.
 *	
 *	1. ��� ��������� ���������� ����1 �������, ����2 �������.
 * 	2. ������, �������� ������ ������� � 60:00.
 * 	3. ��� "������" ������� ������� (��������� �� ����� 2 ���) > �����������.
 * 	4. ��� "������" ������ ��������� ������� ��� ��������� ������� -> �����.
 *
 * 	������:
 * 	- ����2 �������������� �����������, ����1 ������� (��� �������� �����);
 *	- ������������ ��� ����������� �� ���� ����;
 *	- �� ������������� ����������� ���� 1.wav.
 *
 *	�����:
 *	- ��������� �������;
 *	- ����������� ����1, ��� �������� �����;
 *	- �������� ������� ������ �� ������ ������������� (3.wav)
 *
 *	�����������:
 * 	- ����� ���������������;
 * 	- ����������� ����1, ��� �������� �����;
 * 	- �������� ��������� ������ �� ������ ������������� (2.wav).
 */


// ������� ��������� ������ �����
typedef enum {
	STATE_WAIT,			// �������� ������
	STATE_RESET_WAV,	// �������� ������ ������ WAV
	STATE_COUNT_NORM,	// ����� �������� � ���������� ������
	STATE_COUNT_FAST,	// ����� �������� � ���������� ������
	STATE_EXPLODED,		// ����� ����������
	STATE_DEFUSE_START,	// ����� ������ �������������� �����
	STATE_DEFUSE_CHECK,	// �������� �������������� �����
	STATE_DEFUSED		// ����� ���������
} STATE;

// �������
static const uint8_t u8Wire1 = (1 << PC4);	// ������ ������
static const uint8_t u8Wire2 = (1 << PC5);	// ������ ������
static const uint8_t u8Wire3 = (1 << PC6);	// ���������
static const uint8_t u8CntWire = 20;		// ���-�� �������� ������ �������

// ������
static const uint8_t u8PowerWav = (1 << PB7);	// ����� ����������� �����
static const uint8_t u8OutDefuse = (1 << PC7);	// ����� ����������� �����
static const uint8_t u8OutButton = (1 << PB1);	// ����� 0 ��� 3-� ���������

// �����
static const uint8_t u8InStart = (1 << PB6);	// ���� ������� �����

// ����������
static const uint8_t u8Digit1 = (1 << PB0);	// ������ ���������
static const uint8_t u8Digit2 = (1 << PB3);	// ������ ���������
static const uint8_t u8Digit3 = (1 << PB4);	// ������ ���������
static const uint8_t u8Digit4 = (1 << PB5);	// ��������� ���������
static const uint8_t u8Digits = (u8Digit1 | u8Digit2 | u8Digit3 | u8Digit4);

static const uint16_t u16BoomInit = 3600;	// ���������� ������ �� ������ �����
static const uint16_t u16CheckInit = 2;		// ���������� ������ ��� �������� �����������
static const uint8_t u8Tick = 40;			// ���-�� ����� ������� 1 ��� 1 �������
static const uint8_t u8SpeedNo	= 0;		// ���� ������� ����������
static const uint8_t u8SpeedNom = 1;		// ���� ������� ����������
static const uint8_t u8SpeedMax = 30;		// ���� ������� ����������

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
static void setTime(uint16_t val);
static void pressButtonWav(uint8_t num);

// ����� �������� �������� ��� ������ �� �����, ���������� � �������� �������
static uint8_t u8Buf[] = {10, 10, 10, 10};
// ����������(true) / �� ���������� (false) �����
static bool point = false;

// ������������� �������� �������� �� �����
static void setTime(uint16_t val) {
	uint8_t tmp = val % 60;
	u8Buf[3] = tmp % 10;
	u8Buf[2] = tmp / 10;

	tmp = val / 60;
	u8Buf[1] = tmp % 10;
	u8Buf[0] = tmp / 10;
}

// ������� ������ �� WAV-������������� num-���
// ���� 0 - �� ������� ����
// ���� �� 0, �� ���-�� ����������� �������
// �������� ��� � 25��
static void pressButtonWav(uint8_t num) {
	static uint8_t cnt = 0;
	static uint8_t delayOn = 0;
	static uint8_t delayOff = 0;

	if (num != 0) {
		cnt = num;
	} else if (delayOn > 0) {
		DDRB |= u8OutButton;
		delayOn--;
	} else if (delayOff > 0) {
		DDRB &= ~u8OutButton;
		delayOff--;
	} else if (cnt > 0) {
		delayOn = 3;
		delayOff = 2;
		cnt--;
	}
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
	STATE tstate = state;
	uint8_t tick1s = 0;			// ������� 1�
	uint8_t speed = u8SpeedNo;	// �������� ����� �� ��� �������
	uint8_t cntWire = 0;
	uint8_t cntInStart = 0;
	uint8_t prevWire = 0;
	int16_t time = u16BoomInit;	// ����� �� ������ �����
	int16_t ticksCheck = 0;		// ���������� ����� �� �������� �����������
	uint8_t delay = 0;			// ����� �� ����� ��������, ��� = 25��


	sei();

	while(1) {
		// ������� ����
		if (TIFR0 & (1 << OCF0A)) {
			printValue();

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

			if (delay == 0) {
				switch(state) {
					case STATE_COUNT_NORM: {
						point = true;
						speed = u8SpeedNom;
						if ((wire & u8Wire3) == u8Wire3) {
							state = STATE_EXPLODED;		// �������� �������� ������
						} else if ((wire & u8Wire2) == u8Wire2) {
							state = STATE_EXPLODED;		// �������� �������� ������
						} else if ((wire & u8Wire1) == u8Wire1) {
							tstate = state;
							state = STATE_DEFUSE_START;	// �������� ������ ������
						}
					} break;

					case STATE_COUNT_FAST: {
						speed = u8SpeedMax;
						if ((wire & u8Wire3) == u8Wire3) {
							state = STATE_EXPLODED;		// �������� �������� ������
						} else if ((wire & u8Wire2) == u8Wire2) {
							tstate = state;
							state = STATE_DEFUSE_START;		// �������� ������ ������
						}
					} break;

					case STATE_DEFUSE_START: {
						ticksCheck = u8Tick * u16CheckInit;
						state = STATE_DEFUSE_CHECK;
					} break;

					case STATE_DEFUSE_CHECK: {
						if ((wire & u8Wire3) == u8Wire3) {
							state = STATE_EXPLODED;		// �������� �������� ������
						} else if ((wire & u8Wire2) == u8Wire2) {
							state = STATE_EXPLODED;		// �������� �������� ������
						} else if ((wire & u8Wire1) != u8Wire1) {
							state = tstate;				// �������� ������ ������
						} else if (ticksCheck <= 0) {
							state = STATE_DEFUSED;		// ����������� ����� ��������
						}
					} break;

					case STATE_DEFUSED: {
						point = false;
						speed = u8SpeedNo;
						PORTC |= u8OutDefuse;

						pressButtonWav(1);

						state = STATE_WAIT;
					} break;

					case STATE_EXPLODED: {
						point = false;
						time = 0;
						speed = u8SpeedNo;
						PORTC |= u8OutDefuse;

						pressButtonWav(2);

						state = STATE_WAIT;
					} break;

					case STATE_RESET_WAV:
						PORTB &= ~u8PowerWav;
						delay = 80;
						speed = u8SpeedNom;
						state = STATE_COUNT_NORM;
						break;

					case STATE_WAIT: {
						// ������ ������ �����
						if ((PINB & u8InStart) == 0) {
							if (cntInStart <= u8CntWire) {
								cntInStart++;
							} else {
								// ������ ���������� ������ ���� ��� ������� �� �����
								if ((wire & (u8Wire1 | u8Wire2 | u8Wire3)) == 0) {
									// ���������� ������������� �� 200��
									// � ����� ����� ����� ��������� 2� (25�� � 80) �� ����� ��������
									// �.�. ������������� � ��� ����� �� ��������� �� �������
									PORTB |= u8PowerWav;
									delay = 10;

									PORTC &= ~u8OutDefuse;

									cntInStart = 0;
									tick1s = 0;
									time = u16BoomInit;

									state = STATE_RESET_WAV;
								}
							}
						} else {
							cntInStart = 0;
						}
					} break;
				}
			}

			TIFR0 = (1 << OCF0A);
		}

		// ��������� ���� ��� �������� ������
		if (TIFR1 & (1 << OCF1A)) {
			tick1s += speed;
			if ((tick1s / u8Tick) > 0) {
				time -= (tick1s / u8Tick);
				if (time <= 0) {
					if (state != STATE_WAIT)
						state = STATE_EXPLODED;
					time = 0;
				}
				tick1s %=  u8Tick;
			}

			if (ticksCheck > 0) {
				ticksCheck--;
			}

			if (delay > 0) {
				delay--;
			}
			pressButtonWav(0);

			setTime(time);
			TIFR1 = (1 << OCF1A);
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
 * 	������ 1 ����������� ������ 25 ��.
 */
void low_level_init() {

	// PORTB
	// ����� ���������� ���.1
	// ����� ��������� ������� WAV_������ ���.0
	// ���� ������� ����� ���.0
	// ����� �� ������ ���.0 ��� � 3-�� ���������
	DDRB = u8Digit1 | u8Digit2 | u8Digit3 | u8Digit4 | u8PowerWav;
	PORTB = u8Digit1 | u8InStart | u8PowerWav;

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
	// CTC, (2Mhz / 8) / 6250 = 0.025s
	OCR1A = 6250 - 1;
	TCCR1A = (0 << WGM11) | (0 << WGM10);
	TCCR1B = (0 << WGM13) | (1 << WGM12);
	TCCR1B |= (0 << CS12) | (1 << CS11) | (0 << CS10);
};


