#include "scheduler.h"

static Task tasks[MAX_TASKS];

void scheduler_init() {
  for (int i = 0; i < MAX_TASKS; i++) {
    tasks[i].active = false;
    tasks[i].fn = nullptr;
    tasks[i].context = nullptr;
    tasks[i].interval = 0;
    tasks[i].last_run = 0;
    tasks[i].last_duration_ms = 0;
    tasks[i].overruns = 0;
    tasks[i].priority = TASK_PRIORITY_NORMAL;
    tasks[i].name = "unnamed";
  }
}

int scheduler_add(TaskFn fn, void* ctx, uint32_t interval, TaskPriority priority, const char* name) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (!tasks[i].active) {
      tasks[i].fn = fn;
      tasks[i].context = ctx;
      tasks[i].interval = interval;
      tasks[i].last_run = 0;
      tasks[i].last_duration_ms = 0;
      tasks[i].overruns = 0;
      tasks[i].priority = priority;
      tasks[i].active = true;
      tasks[i].name = name;
      return i;
    }
  }
  return -1;
}

void scheduler_remove(int id) {
  if (id >= 0 && id < MAX_TASKS) {
    tasks[id].active = false;
  }
}

void scheduler_run() {
  uint32_t now = millis();

  for (int p = TASK_PRIORITY_HIGH; p >= TASK_PRIORITY_LOW; p--) {
    for (int i = 0; i < MAX_TASKS; i++) {
      Task* t = &tasks[i];

      if (!t->active || t->priority != (TaskPriority)p) continue;
      if (now - t->last_run < t->interval) continue;

      uint32_t start = millis();
      t->fn(t->context);
      uint32_t duration = millis() - start;

      t->last_duration_ms = duration;
      t->last_run = now;

      if (duration > t->interval + TASK_OVERRUN_THRESHOLD_MS) {
        t->overruns++;
      }
    }
  }
}

int scheduler_task_count() {
  return MAX_TASKS;
}

const Task* scheduler_get_task(int id) {
  if (id < 0 || id >= MAX_TASKS) return nullptr;
  return &tasks[id];
}