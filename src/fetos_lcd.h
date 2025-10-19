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

#ifndef FETOS_LCD_H
#define FETOS_LCD_H

#include <avr/io.h>
#include <stdint.h>
#include <stdbool.h>

// === Pin mapping ===
#define LCD_RS PB0
#define LCD_EN PD3
#define LCD_D4 PD4
#define LCD_D5 PD5
#define LCD_D6 PD6
#define LCD_D7 PD7
#define LCD_BACKLIGHT PB2
#define LCD_CONTRASTE PB3

// === Public API ===
void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_send(uint8_t value, uint8_t mode);
void lcd_create_char(uint8_t location, const uint8_t *charmap);
void lcd_sim_print(const char *s);
void lcd_print(const char *s);
void lcd_send_byte(uint8_t value, uint8_t rs);

#endif
/* FETOS_LCD_H_ */