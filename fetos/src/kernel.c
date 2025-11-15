//kernel.c
#include "fetos.h"
#include "scheduler.h"
#include "fetlink.h"
#include "fetvm.h"

void fetos_tick(void) {
  scheduler_run_bg();
  scheduler_run_core();
  scheduler_run_fg();
  fetlink_tick();
  fetvm_tick();
}

