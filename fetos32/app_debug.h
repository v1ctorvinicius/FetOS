#pragma once
#include "app.h"
#include "event.h"

extern App app_debug;

void app_debug_setup();
void app_debug_on_enter();
void app_debug_on_exit();
void app_debug_on_event(Event* e);
void app_debug_render();