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
#include "fetos_lcd.h"
#include "fetos_hw.h"
#include <util/delay.h>

// ====== LOW-LEVEL ======

static void lcd_send_nibble(uint8_t nibble)
{
	// envia nibble para D4–D7
	if (nibble & 0x01)
		PORTD |= (1 << LCD_D4);
	else
		PORTD &= ~(1 << LCD_D4);
	if (nibble & 0x02)
		PORTD |= (1 << LCD_D5);
	else
		PORTD &= ~(1 << LCD_D5);
	if (nibble & 0x04)
		PORTD |= (1 << LCD_D6);
	else
		PORTD &= ~(1 << LCD_D6);
	if (nibble & 0x08)
		PORTD |= (1 << LCD_D7);
	else
		PORTD &= ~(1 << LCD_D7);

	// pulso no Enable (agora em PD3)
	PORTD |= (1 << LCD_EN);
	_delay_us(1);
	PORTD &= ~(1 << LCD_EN);
	_delay_us(50);
}

void lcd_send(uint8_t value, uint8_t mode)
{
	if (mode)
		PORTB |= (1 << LCD_RS);
	else
		PORTB &= ~(1 << LCD_RS);

	lcd_send_nibble(value >> 4);
	lcd_send_nibble(value & 0x0F);
}

// ====== INITIALIZATION ======

void lcd_init(void)
{
	DDRB |= (1 << LCD_RS) | (1 << LCD_EN);
	DDRD |= (1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7);

	_delay_ms(40); // wait >30ms after power on

	lcd_send_nibble(0x03);
	_delay_ms(5);
	lcd_send_nibble(0x03);
	_delay_us(150);
	lcd_send_nibble(0x03);
	_delay_us(150);
	lcd_send_nibble(0x02); // set 4-bit mode

	lcd_send(0x28, 0); // 4-bit, 2-line, 5x8 font
	lcd_send(0x08, 0); // display off
	lcd_send(0x01, 0); // clear display
	_delay_ms(2);
	lcd_send(0x06, 0); // entry mode
	lcd_send(0x0C, 0); // display on, cursor off
}

// ====== BASIC OPS ======

void lcd_clear(void)
{
	lcd_send(0x01, 0);
	_delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
	static const uint8_t row_offsets[] = {0x00, 0x40};
	if (row > 1)
		row = 1;
	lcd_send(0x80 | (col + row_offsets[row]), 0);
}

void lcd_create_char(uint8_t location, const uint8_t *charmap)
{
	location &= 0x7; // only 0�7 are valid
	lcd_send(0x40 | (location << 3), 0);
	for (uint8_t i = 0; i < 8; i++)
	{
		lcd_send(charmap[i], 1);
	}
}
void lcd_print(const char *s)
{
	while (*s)
	{
		lcd_send_byte(*s++, 1); // RS = 1 → dado (caractere)
	}
}

void lcd_sim_print(const char *s)
{
	// Envia para o simulador (Python)
	usart_send_string("LCD:PRINT,");
	usart_send_string(s);
	usart_send_string("\r\n");

	// Mostra também no LCD físico
	lcd_print(s);
}

void lcd_send_byte(uint8_t value, uint8_t rs)
{
	if (rs)
		PORTB |= (1 << LCD_RS);
	else
		PORTB &= ~(1 << LCD_RS);

	// parte alta
	lcd_send_nibble(value >> 4);
	// parte baixa
	lcd_send_nibble(value & 0x0F);
}
