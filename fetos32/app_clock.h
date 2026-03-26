#pragma once
#include "app.h"
#include "event.h"

extern App app_clock;

void app_clock_setup();
void app_clock_on_enter();
void app_clock_on_exit();
void app_clock_on_event(Event* e);
void app_clock_render();