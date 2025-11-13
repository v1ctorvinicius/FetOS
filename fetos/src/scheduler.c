#include "scheduler.h"

static FetApp* bg_q[MAX_APPS];
static FetApp* core_q[MAX_APPS];
static FetApp* fg_q[MAX_APPS];
static uint8_t n_apps = 0;

void scheduler_init(void) {}

void scheduler_add(FetApp* app) {
  bg_q[n_apps] = app;
  core_q[n_apps] = app;
  fg_q[n_apps] = app;
  n_apps++;
}

void scheduler_run_bg(void) {
  for (uint8_t i = 0; i < n_apps; i++)
    if (bg_q[i]->bg) bg_q[i]->bg();
}

void scheduler_run_core(void) {
  for (uint8_t i = 0; i < n_apps; i++)
    if (core_q[i]->active && core_q[i]->core)
      core_q[i]->core();
}

void scheduler_run_fg(void) {
  for (uint8_t i = 0; i < n_apps; i++)
    if (fg_q[i]->active && fg_q[i]->fg)
      fg_q[i]->fg();
}
