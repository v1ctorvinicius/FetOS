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
#include "fetos_io.h"
#include "fetos_core.h"
#include "fetos_ui.h"
#include "fetos_hw.h"

#define NO_APP_SELECTED -1

void fetos_handle_serial(void) {
	if (!usart_available()) return;
	char c = usart_read_char();
	usart_send_char(c); // eco

	if (active_app == NO_APP_SELECTED) {
		if (c == 'd') { menu_index = (menu_index + 1) % APP_COUNT; render_menu(); }
		else if (c == 'a') { menu_index = (menu_index - 1 + APP_COUNT) % APP_COUNT; render_menu(); }
		else if (c == 'w') { app_show(menu_index); active_app = menu_index; }
		} else {
		switch (c) {
			case 'e': app_play(active_app); render_header_for_app(active_app, 1); break;
			case 'q': app_suspend(active_app); render_header_for_app(active_app, 1); break;
			case 'x': app_close(active_app); break;
			case 's': app_minimize(active_app); break;
			case 'w': app_show(active_app); render_header_for_app(active_app, 1); break;
		}
	}
}
