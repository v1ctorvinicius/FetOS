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

#ifndef FETOS_HW_H
#define FETOS_HW_H

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "fetos_lcd.h"

void fetos_hw_init(void);
void fetos_pwm_init(void);
void fetos_pwm_set(uint8_t contrast, uint8_t brightness);

uint32_t fetos_millis(void);
void fetos_delay_ms(uint16_t ms);

void usart_init(uint16_t baud);
void usart_send_char(char c);
void usart_send_string(const char *s);
char usart_read_char(void);
uint8_t usart_available(void);

#endif
/* FETOS_HW_H_ */