#pragma once
#include "device.h"

typedef struct {
  int last_state;
  unsigned long last_debounce;
} ButtonState;

void button_init(Device* dev_base);
void button_poll(Device* dev_base);