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

/** Алгоритм работы.
 *
 * 	1. ЗАПУСК, обратный отсчет начиная с 60:00.
 * 	2. При "обрыве" первого провода время ускоряется в 10 раз.
 * 	3. При "обрыве" второго провода время останавливается -> ОБЕЗВРЕЖЕНО.
 * 	4. При "обрыве" любого неверного провода -> ВЗРЫВ.
 *
 * 	ЗАПУСК:
 *	- срабатывание при воздействии на вход ПУСК;
 *
 *	ВЗРЫВ:
 *	- обнуление времени;
 *	- срабатывание РЕЛЕ1, для управления внешними устройствами;
 *
 *	ОБЕЗВРЕЖЕНО:
 * 	- время останавливается;
 * 	- срабатывание РЕЛЕ2.
 */


// текущее состояние работы бомбы
typedef enum {
	STATE_WAIT,			// ожидание начала
	STATE_COUNT_NORM,	// бомба запущена в нормальном режиме
	STATE_COUNT_FAST,	// бомба запущена в ускоренном режиме
	STATE_EXPLODED,		// бомба взорвалась
	STATE_DEFUSED		// бомба разряжена
} STATE;

// провода
static const uint8_t u8Wire1 = (1 << PC4);	// первый нужный
static const uint8_t u8Wire2 = (1 << PC5);	// второй нужный
static const uint8_t u8Wire3 = (1 << PC6);	// остальные
static const uint8_t u8CntWire = 250;		// кол-во повторов опроса провода

// выходы
static const uint8_t u8OutBoom = (1 << PB7);	// выход сработавшей бомбы
static const uint8_t u8OutDefuse = (1 << PC7);	// выход разряженной бомбы

// входы
static const uint8_t u8InStart = (1 << PB6);	// вход запуска бомбы

// индикаторы
static const uint8_t u8Digit1 = (1 << PB0);	// первый индикатор
static const uint8_t u8Digit2 = (1 << PB3);	// второй индикатор
static const uint8_t u8Digit3 = (1 << PB4);	// третий индикатор
static const uint8_t u8Digit4 = (1 << PB5);	// четвертый индикатор
static const uint8_t u8Digits = (u8Digit1 | u8Digit2 | u8Digit3 | u8Digit4);

static const uint16_t u8BoomInit = 3600;// начальное значение таймера в секундах
static const uint8_t u8Tick = 8;		// кол-во тиков таймера 1 для 1 секунды
static const uint8_t u8SpeedNo	= 0;	// счет таймера остановлен
static const uint8_t u8SpeedNom = 1;	// счет таймера нормальный
static const uint8_t u8SpeedMax = 10;	// счет таймера ускоренный

// разряд для вывода точки (срабатывает для любого индикатора)
static const uint8_t u8Point = (1 << PD0);
// разряды сегментов нечетных индикаторов
static const uint8_t u8OddA = (1 << PD2);
static const uint8_t u8OddB = (1 << PD1);
static const uint8_t u8OddC = (1 << PD7);
static const uint8_t u8OddD = (1 << PD6);
static const uint8_t u8OddE = (1 << PD5);
static const uint8_t u8OddF = (1 << PD3);
static const uint8_t u8OddG = (1 << PD4);
// разряды сегментов четных индикаторов
static const uint8_t u8EvenA = (1 << PD3);
static const uint8_t u8EvenB = (1 << PD4);
static const uint8_t u8EvenC = (1 << PD5);
static const uint8_t u8EvenD = (1 << PD6);
static const uint8_t u8EvenE = (1 << PD7);
static const uint8_t u8EvenF = (1 << PD2);
static const uint8_t u8EvenG = (1 << PD1);

// массив выбора текущего символа индикатора, начинается с младшего символа
static const uint8_t u8Digit[] PROGMEM = {
		u8Digit1,	// десятки минут
		u8Digit2,	// минуты
		u8Digit3,	// десяток секунд
		u8Digit4	// секунды
};

// массив отображаемых значений для нечетных индикаторов
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

// массив отображаемых значений для четных индикаторов
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

// буфер текущего значения для вывода на экран, начинается с младшего символа
static uint8_t u8Buf[] = {10, 10, 10, 10};
// показывать(true) / не показывать (false) точки
static bool point = false;

// перекодировка текущего значения во время
void setTime(uint16_t val) {
	uint8_t tmp = val % 60;
	u8Buf[3] = tmp % 10;
	u8Buf[2] = tmp / 10;

	tmp = val / 60;
	u8Buf[1] = tmp % 10;
	u8Buf[0] = tmp / 10;
}

// вывод значения на индикаторы
void printValue() {
	static uint8_t digit = 0;

	uint8_t p = point ? u8Point : 0;

	// при переходе от разряда к разряду добавим задержку для оптореле
	PORTB &= ~u8Digits;
	_delay_us(500);
	PORTB |= pgm_read_byte(&u8Digit[digit]);

	// для четных и нечетных символов разряды отличаются
	if (digit % 2 == 0) {
		PORTD = pgm_read_byte(&u8SymOdd[u8Buf[digit]]) | p;
	} else {
		PORTD = pgm_read_byte(&u8SymEven[u8Buf[digit]]) | p;
	}

	// переход к следующему разряду
	digit = (digit >= (sizeof(u8Buf) - 1)) ? 0 : digit + 1;
}


int main() {
	STATE state = STATE_WAIT;
	uint8_t tick1s = 0;	// счетчик 1с
	uint8_t speed = u8SpeedNo;	// скорость счета за тик таймера
	uint8_t cntWire = 0;
	uint8_t prevWire = 0;
	uint16_t time = u8BoomInit;	// время до взрыва бомбы

	sei();

	while(1) {

		// быстрый цикл для динамической индикации
		if (TIFR0 & (1 << OCF0A)) {
			printValue();
			TIFR0 = (1 << OCF0A);
		}

		// медленный цикл для подсчета секунд
		if (TIFR1 & (1 << OCF1A)) {
			tick1s += speed;
			if (tick1s >= u8Tick) {	// прошла секунда
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
					state = STATE_EXPLODED;		// вытащили неверный провод
				} else if ((wire & u8Wire2) == u8Wire2) {
					state = STATE_EXPLODED;		// вытащили неверный провод
				} else if ((wire & u8Wire1) == u8Wire1) {
					state = STATE_COUNT_FAST;	// вытащили верный провод
				}
			} break;

			case STATE_COUNT_FAST: {
				speed = u8SpeedMax;
				if ((wire & u8Wire3) == u8Wire3) {
					state = STATE_EXPLODED;		// вытащили неверный провод
				} else if ((wire & u8Wire1) == 0) {
					state = STATE_EXPLODED;		// провод вставили обратно
				} else if ((wire & u8Wire2) == u8Wire2) {
					state = STATE_DEFUSED;		// вытащили верный провод
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
				// запуск работы бомбы
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


/**	Начальная инициализация периферии.
 *
 * 	По-умолчанию МК тактируется от внешнего кварца, в данном случае 16МГц. При
 * 	этом включен фьюз деления частоты на 8. Т.е. в итоге получаем рабочую
 * 	частоту в 2МГц. Сбросить данный фьюз можно только при полной очистке чипа,
 * 	при этом затрется и бутлоадер. Поэтому работаем на этих 2МГц.
 *
 * 	Таймер 0 срабатывает каждые 3.2 мс.
 * 	Таймер 1 срабатывает каждые 125 мс.
 */
void low_level_init() {

	// PORTB
	// Выбор индикатора лог.1
	// Выход срабатывания бомбы лог.0
	// Вход запуска бомбы лог.0
	DDRB = u8Digit1 | u8Digit2 | u8Digit3 | u8Digit4 | u8OutBoom;
	PORTB = u8Digit1 | u8OutBoom | u8InStart;

	// PORTC
	// Провода для бомбы, лог.1 при обрыве провода
	// Выход разряженной бомбы лог.0
	DDRC = u8OutDefuse;
	PORTC = u8Wire1 | u8Wire2 | u8Wire3 | u8OutDefuse;

	// PORTD
	// Сегменты индикатора, светодиод горит при лог. 1
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


