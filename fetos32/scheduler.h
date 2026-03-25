#pragma once
#include <Arduino.h>
#include "app.h"

#define MAX_TASKS 10

typedef void (*TaskFn)();

typedef struct {
  TaskFn fn;
  uint32_t interval;
  uint32_t last_run;
  bool active;
} Task;

void scheduler_init();
int scheduler_add(TaskFn fn, uint32_t interval);
void scheduler_run();
void scheduler_remove(int id);
void system_set_app(App* app);