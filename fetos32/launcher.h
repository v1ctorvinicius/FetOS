// launcher.h
#pragma once
#include "system.h"
#include "event.h"
#include "app.h"
#include "nav_action.h"

typedef enum {
  LAUNCHER_STATE_BROWSING,
  LAUNCHER_STATE_CONFIRM_LOCK
} LauncherState;

extern App app_debug;

static LauncherState state = LAUNCHER_STATE_BROWSING;

void launcher_on_enter();
void launcher_on_exit();
void launcher_on_event(Event* e);
void launcher_render();
void app_launcher_setup();
void launcher_discover_apps();