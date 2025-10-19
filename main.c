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
#include <avr/io.h>
#include <avr/interrupt.h>
#include "src/fetos_hw.h"
#include <util/delay.h>
#include "src/fetos_core.h"
#include "src/fetos_ui.h"
#include "src/fetos_io.h"
#include "src/fetos_lcd.h"



int main(void) {
	fetos_hw_init();
	lcd_init();
	lcd_clear();
	lcd_set_cursor(0, 0);
	lcd_sim_print("FetOS Booting...");
	_delay_ms(800);
	
	fetos_ui_init();
	fetos_core_init();

	sei();

	while (1) {
		scheduler_tick();
		fetos_handle_serial();
		fetos_delay_ms(80);
	}
}

