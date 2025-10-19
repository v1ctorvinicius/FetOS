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

#include "../src/fetos_core.h"
#include "../src/fetos_ui.h"
#include "../src/fetos_hw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct {
	uint8_t led_state;
	uint32_t last_toggle;
} heart_state_t;

static heart_state_t heart = {0};

void fetos_heart_app_bg(void) {
	if ((fetos_millis() - heart.last_toggle) >= 500) {
		heart.last_toggle = fetos_millis();
		heart.led_state = !heart.led_state;
		if (heart.led_state) PORTB |= (1 << PB5);
		else PORTB &= ~(1 << PB5);
	}
}

void fetos_heart_app_run(void) { }

void fetos_heart_app_fg(void) {
	const char *state = heart.led_state ? "LED: ON " : "LED: OFF";
	lcd_draw_lines(NULL, state);
}

void fetos_heart_app_cleanup(void) {
	heart.led_state = 0;
	PORTB &= ~(1 << PB5);
}
