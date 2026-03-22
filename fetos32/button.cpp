//button.cpp
#include "button.h"
#include "event.h"
#include <stdlib.h>

void button_init(Device* dev) {
  pinMode(dev->pin, INPUT_PULLUP);

  ButtonState* st = (ButtonState*)malloc(sizeof(ButtonState));
  st->last_state = HIGH;
  st->last_debounce = 0;

  dev->state = st;
}

void button_poll(Device* dev) {
  ButtonState* st = (ButtonState*)dev->state;

  int reading = digitalRead(dev->pin);

  if (reading != st->last_state) {
    st->last_debounce = millis();
  }

  if ((millis() - st->last_debounce) > 50) {

    static int stable_state = HIGH;

    if (reading != stable_state) {
      stable_state = reading;

      if (stable_state == LOW) {
        Serial.println("BUTTON PRESS DETECTED");

        Event e;
        e.type = EVENT_BUTTON_PRESS;
        e.device_id = dev->id;
        e.has_payload = false;

        if (!event_push(e)) {
          Serial.println("QUEUE CHEIA 💀");
        } else {
          Serial.println("EVENT PUSH OK");
        }
      }
    }
  }

  st->last_state = reading;
}