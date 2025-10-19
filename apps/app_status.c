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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct {
	uint8_t selected;
} status_state_t;

static status_state_t status = {0};

void status_app_bg(void) { }
void status_app_run(void) { }

void status_app_fg(void) {
	char line[17];
	snprintf(line, 17, "[%c]%s",
	state_icon(apps[status.selected].state),
	apps[status.selected].name);
	lcd_draw_lines(NULL, line);
}

void status_app_cleanup(void) { }
