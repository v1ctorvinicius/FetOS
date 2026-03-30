#pragma once
#include "app.h"
#include "event.h"

#define MAX_SLOTS 4

typedef struct {
  uint32_t total_sessions;
  uint32_t total_seconds;
} PomoSlot;

extern App app_pomodoro;

extern PomoSlot slots[MAX_SLOTS];
extern uint8_t current_slot;

void app_pomodoro_setup();
void app_pomodoro_on_enter();
void app_pomodoro_on_exit();
void app_pomodoro_on_event(Event* e);
void app_pomodoro_render();