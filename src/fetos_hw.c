/*
 * FetOS - Embedded Cooperative Operating System
 * Copyright 2025 Victor Santos
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "fetos_hw.h"
#include "fetos_lcd.h"
#include <avr/interrupt.h>
#include <util/delay.h>

#define F_CPU 16000000UL

static volatile uint32_t system_ms = 0;

// =============================
// ======  TIME BASE  =========
// =============================

ISR(TIMER0_COMPA_vect)
{
	system_ms++;
}

void fetos_hw_init(void)
{
	// --- GPIO ---
	// RS, BACKLIGHT e CONTRASTE → PORTB
	DDRB |= (1 << LCD_RS) | (1 << LCD_BACKLIGHT) | (1 << LCD_CONTRASTE);

	// ENABLE (E) e D4–D7 → PORTD
	DDRD |= (1 << LCD_EN) | (1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7);

	// --- UART ---
	usart_init(9600);
	usart_send_string("FetOS UART ready\r\n");

	// --- TIMER0 → 1ms tick ---
	TCCR0A = (1 << WGM01);
	OCR0A = 249; // 1ms @ 16MHz / 64 prescaler
	TCCR0B = (1 << CS01) | (1 << CS00);
	TIMSK0 = (1 << OCIE0A);

	sei(); // habilita interrupções globais
}

uint32_t fetos_millis(void)
{
	uint32_t ms;
	cli();
	ms = system_ms;
	sei();
	return ms;
}

void fetos_delay_ms(uint16_t ms)
{
	while (ms--)
		_delay_ms(1);
}

void fetos_pwm_init(void)
{
	// Timer1 ? Fast PWM 8-bit
	TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
	TCCR1B = (1 << WGM12) | (1 << CS11); // prescaler 8

	fetos_pwm_set(35, 80);
}

void fetos_pwm_set(uint8_t contrast, uint8_t brightness)
{
	OCR1A = contrast;		// D11 (PB3)
	OCR1B = brightness; // D10 (PB2)
}

void usart_init(uint16_t baud)
{
	uint16_t ubrr = (F_CPU / 16 / baud) - 1;
	UBRR0H = (ubrr >> 8);
	UBRR0L = ubrr;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void usart_send_char(char c)
{
	while (!(UCSR0A & (1 << UDRE0)))
		;
	UDR0 = c;
}

void usart_send_string(const char *s)
{
	while (*s)
		usart_send_char(*s++);
}

char usart_read_char(void)
{
	while (!(UCSR0A & (1 << RXC0)))
		;
	return UDR0;
}

uint8_t usart_available(void)
{
	return (UCSR0A & (1 << RXC0));
}
