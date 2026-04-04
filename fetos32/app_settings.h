#pragma once
#include "app.h"
#include "event.h"
#include "buzzer.h"
#include "persistence.h"

extern App app_settings;

void app_settings_setup();
void app_settings_on_enter();
void app_settings_on_exit();
