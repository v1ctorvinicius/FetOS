#include "scheduler.h"

static Task tasks[MAX_TASKS];

void scheduler_init() {
  for (int i = 0; i < MAX_TASKS; i++) {
    tasks[i].active = false;
  }
}

int scheduler_add(TaskFn fn, uint32_t interval) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (!tasks[i].active) {
      tasks[i].fn = fn;
      tasks[i].interval = interval;
      tasks[i].last_run = 0;
      tasks[i].active = true;
      return i;
    }
  }
  return -1;  // TODO: scheduler full
}

void scheduler_remove(int id) {
  if (id >= 0 && id < MAX_TASKS) {
    tasks[id].active = false;
  }
}

void scheduler_run() {
  uint32_t now = millis();

  for (int i = 0; i < MAX_TASKS; i++) {
    if (!tasks[i].active) continue;

    if (now - tasks[i].last_run >= tasks[i].interval) {
      tasks[i].last_run = now;
      tasks[i].fn();
    }
  }
}