#pragma once
#include "device.h"
#include "persistence.h"

typedef struct {
  uint8_t pin;

  const uint16_t* pattern;
  uint8_t length;
  uint8_t index;

  uint32_t last_tick;
  bool active;
  bool state_on;
} BuzzerState;

void buzzer_init(Device* dev);

void buzzer_play(const uint16_t* pattern, uint8_t length);
void buzzer_stop();
void buzzer_beep(uint16_t duration);
void buzzer_render(Device* dev);
void buzzer_beep_beep();
bool buzzer_is_enabled();