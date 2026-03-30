#pragma once
#include "app.h"
#include "event.h"

extern App app_buzzer;

void app_buzzer_setup();
void app_buzzer_on_enter();
void app_buzzer_on_exit();
void app_buzzer_on_event(Event* e);
void app_buzzer_render();