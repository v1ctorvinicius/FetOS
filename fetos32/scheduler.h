#pragma once
#include <Arduino.h>
#include "app.h"

#define MAX_TASKS 10
#define TASK_OVERRUN_THRESHOLD_MS 5

typedef void (*TaskFn)(void* ctx);

typedef enum {
  TASK_PRIORITY_LOW = 0,
  TASK_PRIORITY_NORMAL = 1,
  TASK_PRIORITY_HIGH = 2
} TaskPriority;

typedef struct {
  TaskFn fn;
  void* context;
  uint32_t interval;
  uint32_t last_run;
  uint32_t last_duration_ms;
  uint32_t overruns;
  TaskPriority priority;
  bool active;
  const char* name;
} Task;

void scheduler_init();
int scheduler_add(TaskFn fn, void* ctx, uint32_t interval, TaskPriority priority, const char* name);
void scheduler_remove(int id);
void scheduler_run();

int scheduler_task_count();
const Task* scheduler_get_task(int id);

// void system_set_app(App* app);