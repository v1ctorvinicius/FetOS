#ifndef FETOS_H
#define FETOS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_APPS      8
#define MAX_DEVICES   8
#define FET_TICK_MS   50

typedef struct FetApp {
  void (*init)(void);
  void (*bg)(void);
  void (*core)(void);
  void (*fg)(void);
  bool active;
  uint8_t priority_bg;
  uint8_t priority_core;
  uint8_t priority_fg;
} FetApp;

void fetos_init(void);
void fetos_tick(void);

#endif
