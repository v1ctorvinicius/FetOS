#pragma once
#include "device.h"
#include "system.h"

typedef struct {
  uint8_t pin_r;
  uint8_t pin_g;
  uint8_t pin_b;

  uint8_t r;
  uint8_t g;
  uint8_t b;
} RgbState;

void rgb_init(Device* dev);
void rgb_set(uint8_t r, uint8_t g, uint8_t b);
void rgb_set(uint8_t r, uint8_t g, uint8_t b);