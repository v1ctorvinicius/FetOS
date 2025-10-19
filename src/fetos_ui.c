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

#define F_CPU 16000000UL  // 16 MHz

#include "fetos_ui.h"
#include "fetos_lcd.h"
#include "fetos_hw.h"
#include <stdio.h>
#include <util/delay.h>

void fetos_ui_init(void) {
	fetos_pwm_init();
	lcd_init();

	static const uint8_t ICON_RUN[8] = {0x00,0x08,0x0C,0x0E,0x0F,0x0E,0x0C,0x08};
	static const uint8_t ICON_MIN[8] = {0x00,0x0F,0x09,0x1D,0x1F,0x1C,0x00,0x00};
	static const uint8_t ICON_SUS[8] = {0x00,0x1B,0x1B,0x1B,0x1B,0x1B,0x00,0x00};
	static const uint8_t ICON_STOP[8] = {0x00,0x0E,0x19,0x15,0x13,0x0E,0x00,0x00};
	static const uint8_t ICON_LIGHTNING[8] = {0x07,0x0E,0x0E,0x1C,0x1F,0x03,0x06,0x04};

	lcd_create_char(0, ICON_RUN);
	lcd_create_char(1, ICON_MIN);
	lcd_create_char(2, ICON_SUS);
	lcd_create_char(3, ICON_STOP);
	lcd_create_char(4, ICON_LIGHTNING);

	fetos_core_init();
}

void lcd_draw_lines(const char *line1, const char *line2) {
	char buf[17];
	if (line1) {
		snprintf(buf, 17, "%-16s", line1);
		lcd_set_cursor(0, 0);
		lcd_sim_print(buf);
	}
	if (line2) {
		snprintf(buf, 17, "%-16s", line2);
		lcd_set_cursor(0, 1);
		lcd_sim_print(buf);
	}
}

char state_icon(app_state_t s) {
	switch (s) {
		case APP_RUNNING:   return 0;
		case APP_SUSPENDED: return 2;
		case APP_CLOSED:    return 3;
		default:            return '?';
	}
}

void render_menu(void) {
	char line1[17];
	snprintf(line1, 17, "FetOS Menu %d/%d", menu_index + 1, APP_COUNT);
	lcd_draw_lines(line1, "");

	lcd_set_cursor(0, 1);
	lcd_send('[', 1);
	lcd_send(state_icon(apps[menu_index].state), 1);
	lcd_send(']', 1);
	lcd_sim_print(apps[menu_index].name);
}

void render_header_for_app(uint8_t idx, bool forceRedraw) {
	(void)forceRedraw;
	app_t *a = &apps[idx];
	char line1[17];
	char icon = state_icon(a->state);

	snprintf(line1, 17, "%-15s", a->name);
	lcd_set_cursor(0, 0);
	lcd_sim_print(line1);
	lcd_set_cursor(15, 0);
	lcd_send(icon, 1);
}

void render_header(const char *title) {
	char line1[17];
	snprintf(line1, 17, "%-15s", title);
	lcd_set_cursor(0, 0);
	lcd_sim_print(line1);
	lcd_set_cursor(15, 0);
	lcd_send(3, 1);
}
