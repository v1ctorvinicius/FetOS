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


#ifndef FETOS_UI_H
#define FETOS_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "fetos_core.h"

void fetos_ui_init(void);
void lcd_draw_lines(const char *line1, const char *line2);
void render_menu(void);
void render_header_for_app(uint8_t idx, bool forceRedraw);
void render_header(const char *title);
char state_icon(app_state_t s);

#endif
/* FETOS_UI_H_ */