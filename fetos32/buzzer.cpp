#include "buzzer.h"
#include <Arduino.h>
#include "system.h"
#include "capability.h"
#include <stdlib.h>

static Device *buzzer_dev = nullptr;

static RequestResult handle_beep(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  (void)caller;
  if (!buzzer_is_enabled())
    return REQ_IGNORED;

  int32_t dur = 200;
  const RequestParam *p = payload_get(payload, "dur");
  if (p)
    dur = p->int_value;

  uint16_t pattern[2] = {(uint16_t)dur, 0};
  buzzer_play(pattern, 2);

  return REQ_ACCEPTED;
}

static RequestResult handle_beep_beep(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  (void)caller;
  if (!buzzer_is_enabled())
    return REQ_IGNORED;
  buzzer_beep_beep();
  return REQ_ACCEPTED;
}

static RequestResult handle_stop(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  (void)caller;
  buzzer_stop();
  return REQ_ACCEPTED;
}

void buzzer_init(Device *dev)
{
  BuzzerState *st = (BuzzerState *)malloc(sizeof(BuzzerState));

  st->pin = dev->pin;
  st->pattern = nullptr;
  st->length = 0;
  st->index = 0;
  st->last_tick = 0;
  st->active = false;
  st->state_on = false;

  pinMode(st->pin, OUTPUT);
  digitalWrite(st->pin, LOW);

  dev->state = st;
  buzzer_dev = dev;

  system_register_capability("audio:beep", handle_beep, dev);
  system_register_capability("audio:beep_beep", handle_beep_beep, dev);
  system_register_capability("audio:stop", handle_stop, dev);
}

void buzzer_render(Device *dev)
{
  if (!dev || !dev->state)
    return;

  BuzzerState *st = (BuzzerState *)dev->state;
  if (!st->active || !st->pattern)
    return;

  uint32_t now = millis();
  if (now - st->last_tick >= st->pattern[st->index])
  {
    st->last_tick = now;
    st->state_on = !st->state_on;
    digitalWrite(st->pin, st->state_on);
    st->index++;
    if (st->index >= st->length)
      buzzer_stop();
  }
}

void buzzer_play(const uint16_t *pattern, uint8_t length)
{
  if (!buzzer_is_enabled())
    return;
  if (!buzzer_dev || !buzzer_dev->state)
    return;

  BuzzerState *st = (BuzzerState *)buzzer_dev->state;
  st->pattern = pattern;
  st->length = length;
  st->index = 0;
  st->last_tick = millis();
  st->active = true;
  st->state_on = true;

  digitalWrite(st->pin, HIGH);
}

void buzzer_stop()
{
  if (!buzzer_dev || !buzzer_dev->state)
    return;
  BuzzerState *st = (BuzzerState *)buzzer_dev->state;
  st->active = false;
  st->pattern = nullptr;
  digitalWrite(st->pin, LOW);
}

void buzzer_beep(uint16_t duration)
{
  uint16_t pattern[2] = {duration, 0};
  buzzer_play(pattern, 2);
}

void buzzer_beep_beep()
{
  static uint16_t pattern[] = {100, 100, 100, 0};
  buzzer_play(pattern, 4);
}

bool buzzer_is_enabled()
{
  return persistence_read_int("sys_snd", 1) == 1;
}