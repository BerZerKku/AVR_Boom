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

static const uint8_t u8Timer0 = 6;
static const uint8_t u8wireDefuse = 0b0011;
static const uint8_t u8Tick = 8;	// кол-во тиков таймера 1 для 1 секунды
static const uint8_t u8BoomInit = 120;
static const uint8_t u8SpeedMin = 1;
static const uint8_t u8SpeedMax = 2;


void low_level_init() __attribute__((__naked__)) __attribute__((section(".init3")));

static void printValue();
static void setValue(uint16_t val, bool point);

// массив отображаемых значений
static const uint8_t sym[] PROGMEM= {
		0b11000000,	// '0'
		0b11111001,	// '1'
		0b10100100,	// '2'
		0b10110000,	// '3'
		0b10011001,	// '4'
		0b10010010,	// '5'
		0b10000010,	// '6'
		0b11111000,	// '7'
		0b10000000,	// '8'
		0b10010000,	// '9'
		0b11111111	// ' '

};

// массив выбора текущего символа индикатора, начинается с младшего символа
static const uint8_t u8Digit[] PROGMEM = {0x07, 0x0B, 0x0D, 0x0E};

// буфер текущего значения для вывода на экран, начинается с младшего символа
static uint8_t u8Buf[] = {0, 0, 0, 0};

// время до взрыва бомбы
uint8_t time = u8BoomInit;
// положение проводников, 1 - был обрезан, 0 - не трогали.
uint8_t wire = 0b0000;

// показывать(true) / не показывать (false) точки
bool point = true;
// текущее значение скорости счета, чем больше, тем быстрее
uint8_t speed = u8SpeedMin;



// установка значения выводимого на индикатор
void setValue(uint16_t val) {
	for(uint8_t i = 0; i < sizeof(u8Buf); i++) {
		u8Buf[i] = val % 10;
		val /= 10;
	}
}

// перекодировка текущего значения во время
void setTime(uint16_t val) {
	uint8_t tmp = val % 60;
	u8Buf[0] = tmp % 10;
	u8Buf[1] = tmp / 10;

	tmp = val / 60;
	u8Buf[2] = tmp % 10;
	u8Buf[3] = tmp / 10;
}

void stopTimer() {
	TCCR1B = 0;
}


void checkWire() {

	PORTB = 0x0F;	// сегменты индикатора отключены
	DDRD = 0xF0;	//
	PORTD = 0x0F;	// входы с подтяжкой к +

	_delay_us(5);
	wire |= (PIND & 0x0F);

	DDRD = 0xFF;
}

void printValue() {
	static uint8_t digit = 0;

	uint8_t p = (point ? 0 : 1)  << 4;
	PORTB = (PORTB & 0xE0) + (0x0F & pgm_read_byte(&u8Digit[digit])) + p;
	PORTD = pgm_read_byte(&sym[u8Buf[digit]]);

	digit = (digit >= (sizeof(u8Buf) - 1)) ? 0 : digit + 1;
}


int main() {
	uint8_t tick = 0;	// счетчик 1с

	sei();

	while(1) {

		if (TIFR & (1 << OCF1A)) {
			tick += speed;
			if (tick >= u8Tick) {	// a second passed
				if  (time > 0)
					time--;
				tick = 0;
			}
			setTime(time);
			TIFR = (1 << OCF1A);
		}

		if (wire) {
			uint8_t wireBoom = (u8wireDefuse ^ 0x0F);
			if ((wire & (u8wireDefuse ^ 0x0F)) == (u8wireDefuse ^ 0x0F)) {
				time = 0;			// BOOM
			} else if ((wire & u8wireDefuse) == u8wireDefuse) {
				stopTimer();		// DEFUSE
			} else if ((wire & wireBoom) != 0) {
				speed = u8SpeedMax;	// TICK-TICK
			}
		}
	}
}

ISR(TIMER0_OVF0_vect) {
	checkWire();
	printValue();
	TCNT0 = u8Timer0;
}

void low_level_init() {
	DDRB = 0xFF;
	PORTB = 0x0E;

	DDRD = 0xFF;
	PORTD = 0x00;

	TIMSK =  (1 << TOIE0);

	// (8Mhz / 64) / 250 = 2ms
	TCNT0 = u8Timer0;
	TCCR0 = (0 << CS02) | (1 << CS01) | (1 << CS00);	// 64

	// (8Mhz / 64) / 15625 = 0.125s
	OCR1A = 15625-1;
	TCCR1B = (1 << CTC1) | (0 << CS12) | (1 << CS11) | (1 << CS10); 	// CTC, 64
};


