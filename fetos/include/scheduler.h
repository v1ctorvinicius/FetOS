#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "fetos.h"

void scheduler_init(void);
void scheduler_run_bg(void);
void scheduler_run_core(void);
void scheduler_run_fg(void);
void scheduler_add(FetApp* app);

#endif
