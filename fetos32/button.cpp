//button.cpp
#include "button.h"
#include "event.h"
#include <Arduino.h>
#include "button_gesture.h"
#include <stdlib.h>
#include "scheduler.h"
#include "system.h"


#define BOOT_IGNORE_MS 300

static Device* button_dev = nullptr;

static void button_task(void* ctx) {
  Device* dev = (Device*)ctx;
  if (dev && dev->poll) {
    dev->poll(dev);
  }
}

void button_init(Device* dev) {
  pinMode(dev->pin, INPUT_PULLUP);

  ButtonGestureState* st = (ButtonGestureState*)malloc(sizeof(ButtonGestureState));
  st->last_state = HIGH;
  st->last_debounce = 0;
  st->down_ms = 0;
  st->last_up_ms = 0;
  st->tap_count = 0;
  st->long_sent = false;
  st->stable_state = HIGH;
  st->block_until_release = false;

  dev->state = st;

  button_dev = dev;

  scheduler_add(button_task, dev, 10, TASK_PRIORITY_HIGH, "button");
  system_register_capability("input:button", handle_get_button, dev);
}

void button_poll(Device* dev) {
  ButtonGestureState* st = (ButtonGestureState*)dev->state;
  int reading = digitalRead(dev->pin);

  if (st->block_until_release) {
    if (reading == HIGH) {
      st->block_until_release = false;
    } else {
      st->last_state = reading;
      return;
    }
  }

  if (millis() < BOOT_IGNORE_MS) {
    st->tap_count = 0;
    st->down_ms = 0;
    st->last_up_ms = millis();
    st->long_sent = false;
    st->last_state = reading;
    return;
  }

  // debounce
  if (reading != st->last_state) {
    st->last_debounce = millis();
  }

  if ((millis() - st->last_debounce) > DEBOUNCE_MS) {
    if (reading != st->stable_state) {
      st->stable_state = reading;
      unsigned long now = millis();

      if (st->stable_state == LOW) {
        st->down_ms = now;
        st->long_sent = false;
      } else {
        if (st->long_sent) {
          st->tap_count = 0;
        } else {
          st->tap_count++;
        }

        st->last_up_ms = now;
        st->down_ms = 0;
      }
    }
  }

  // long press
  if (st->down_ms > 0 && !st->long_sent && (millis() - st->down_ms) > LONG_PRESS_MS) {
    Event e;
    e.type = EVENT_BUTTON_PRESS;
    e.device_id = dev->id;
    e.has_payload = true;
    e.payload.value = GESTURE_LONG_PRESS;

    event_push(e);

    st->long_sent = true;
    st->tap_count = 0;
    st->down_ms = 0;
    st->block_until_release = true;

    return;
  }

  // taps
  if (st->tap_count > 0 && (millis() - st->last_up_ms) > DOUBLE_TAP_WINDOW_MS) {
    Event e;
    e.type = EVENT_BUTTON_PRESS;
    e.device_id = dev->id;
    e.has_payload = true;

    if (st->tap_count == 1) e.payload.value = GESTURE_TAP;
    else if (st->tap_count == 2) e.payload.value = GESTURE_DOUBLE_TAP;
    else e.payload.value = GESTURE_TRIPLE_TAP;

    event_push(e);
    st->tap_count = 0;
  }

  st->last_state = reading;
}



void button_reset() {
  if (!button_dev || !button_dev->state) return;

  ButtonGestureState* st = (ButtonGestureState*)button_dev->state;

  st->tap_count = 0;
  st->down_ms = 0;
  st->last_up_ms = millis();
  st->long_sent = false;
  st->block_until_release = true;
  st->stable_state = HIGH;
  st->last_state = HIGH;
}



static RequestResult handle_get_button(Device* dev, const RequestPayload* payload, CallerContext* caller) {
  // O slot "result" (params[0]) é onde a FVM espera a resposta
  // digitalRead retorna 0 ou 1. Como o seu botão provavelmente está com PULLUP:
  // !digitalRead(dev->pin) retorna 1 quando APERTADO.
  payload->params[0].int_value = !digitalRead(dev->pin);

  return REQ_ACCEPTED;
}