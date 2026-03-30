#pragma once
#include "device.h"
#include "capability.h"

typedef struct {
  int last_state;
  unsigned long last_debounce;
} ButtonState;

void button_init(Device* dev_base);
void button_poll(Device* dev_base);
void button_reset();

static RequestResult handle_get_button(Device* dev, const RequestPayload* payload, CallerContext* caller);