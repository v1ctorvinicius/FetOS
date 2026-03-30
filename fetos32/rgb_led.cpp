#include "rgb_led.h"
#include <Arduino.h>
#include "system.h"
#include "capability.h"
#include <stdlib.h>

static Device *rgb_dev = nullptr;

static RequestResult handle_led_set(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  (void)caller;
  if (!dev || !dev->state)
    return REQ_NO_DEVICE;

  RgbState *st = (RgbState *)dev->state;

  const RequestParam *r = payload_get(payload, "r");
  const RequestParam *g = payload_get(payload, "g");
  const RequestParam *b = payload_get(payload, "b");

  if (r)
    st->r = (uint8_t)r->int_value;
  if (g)
    st->g = (uint8_t)g->int_value;
  if (b)
    st->b = (uint8_t)b->int_value;

  return REQ_ACCEPTED;
}

static RequestResult handle_led_off(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  (void)caller;
  if (!dev || !dev->state)
    return REQ_NO_DEVICE;

  RgbState *st = (RgbState *)dev->state;
  st->r = st->g = st->b = 0;

  return REQ_ACCEPTED;
}

void rgb_init(Device *dev)
{
  RgbState *st = (RgbState *)malloc(sizeof(RgbState));

  st->pin_r = 14;
  st->pin_g = 12;
  st->pin_b = 13;
  st->r = st->g = st->b = 0;

  pinMode(st->pin_r, OUTPUT);
  pinMode(st->pin_g, OUTPUT);
  pinMode(st->pin_b, OUTPUT);

  dev->state = st;
  rgb_dev = dev;

  system_register_capability("led:set", handle_led_set, dev);
  system_register_capability("led:off", handle_led_off, dev);
}

void rgb_render(Device *dev)
{
  if (!dev || !dev->state)
    return;

  RgbState *st = (RgbState *)dev->state;
  analogWrite(st->pin_r, st->r);
  analogWrite(st->pin_g, st->g);
  analogWrite(st->pin_b, st->b);
}

void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
  if (!rgb_dev || !rgb_dev->state)
    return;
  RgbState *st = (RgbState *)rgb_dev->state;
  st->r = r;
  st->g = g;
  st->b = b;
}