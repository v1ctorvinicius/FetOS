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
#include "fetos_apps.h"
#include "fetos_core.h"
#include "fetos_ui.h"
#include "fetos_hw.h"
#include "fetos_lcd.h"


unsigned long system_start_ms = 0;
int8_t menu_index = 0;
int8_t active_app = -1;
int8_t visible_app = -1;
bool cleanup_pending[10];

app_t apps[] = {
	{ "Clock", APP_CLOSED, false, clock_app_run, clock_app_bg, clock_app_fg, clock_app_cleanup, "" },
	{ "Heart", APP_CLOSED, false, fetos_heart_app_run, fetos_heart_app_bg, fetos_heart_app_fg, fetos_heart_app_cleanup, "" },
	{ "Status", APP_CLOSED, false, status_app_run, status_app_bg, status_app_fg, status_app_cleanup, "" }
};

const uint8_t APP_COUNT = sizeof(apps) / sizeof(app_t);


void fetos_core_init(void) {
	system_start_ms = fetos_millis();
	render_menu();
}

void app_play(uint8_t idx) {
	app_t *a = &apps[idx];
	a->state = APP_RUNNING;
}

void app_suspend(uint8_t idx) {
	app_t *a = &apps[idx];
	if (a->state == APP_RUNNING)
	a->state = APP_SUSPENDED;
}

void app_close(uint8_t idx) {
	app_t *a = &apps[idx];
	if (a->state != APP_CLOSED) {
		a->state = APP_CLOSED;
		cleanup_pending[idx] = true;
	}

	a->is_visible = false;

	if (visible_app == idx)
	visible_app = -1;
	if (active_app == idx)
	active_app = -1;

	lcd_clear();
	render_menu();
}

void app_minimize(uint8_t idx) {
	apps[idx].is_visible = false;
	visible_app = -1;
	active_app = -1;
	lcd_clear();
	render_menu();
}

void app_show(uint8_t idx) {
	for (uint8_t i = 0; i < APP_COUNT; ++i)
	apps[i].is_visible = false;

	apps[idx].is_visible = true;
	visible_app = idx;
	active_app = idx;

	lcd_clear();
	render_header_for_app(idx, 1);
	if (apps[idx].fg)
	apps[idx].fg();
}

void scheduler_tick(void) {
	static int8_t last_visible = -2;

	// --- 1. Executa lógica dos apps ativos ---
	for (uint8_t i = 0; i < APP_COUNT; ++i) {
		app_t *a = &apps[i];
		if (a->state == APP_RUNNING) {
			if (a->bg)
			a->bg();
			if (a->run)
			a->run();
		}
	}

	// --- 2. Detecta troca de app visível ---
	if (visible_app != last_visible) {
		lcd_clear();

		if (visible_app >= 0 && apps[visible_app].fg) {
			render_header(apps[visible_app].name);
			apps[visible_app].fg();
			} else {
			render_menu();
		}

		last_visible = visible_app;
	}

	// --- 3. Renderiza app ativo ---
	if (visible_app >= 0 && apps[visible_app].is_visible) {
		render_header(apps[visible_app].name);
		if (apps[visible_app].fg)
		apps[visible_app].fg();
	}

	// --- 4. Executa cleanup pendente ---
	for (uint8_t i = 0; i < APP_COUNT; ++i) {
		if (cleanup_pending[i]) {
			if (apps[i].cleanup)
			apps[i].cleanup();
			cleanup_pending[i] = false;
		}
	}
}
