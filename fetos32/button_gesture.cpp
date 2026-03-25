// button_gesture.cpp
#include "button_gesture.h"
#include <Arduino.h>

#define LONG_PRESS_MS 500
#define DOUBLE_TAP_WINDOW_MS 300
#define TRIPLE_TAP_WINDOW_MS 500

GestureType process_button_gesture(Device* dev, ButtonGestureState* state, bool pressed) {
  uint32_t now = millis();
  GestureType result = GESTURE_NONE;

  if (pressed) {
    if (state->down_ms == 0) state->down_ms = now;
    if (!state->long_sent && (now - state->down_ms) > LONG_PRESS_MS) {
      result = GESTURE_LONG_PRESS;
      state->long_sent = true;
      state->tap_count = 0;  
    }
  } else {
    if (state->down_ms > 0) {
      if (!state->long_sent) {
        state->tap_count++;
      }
      state->last_up_ms = now;
      state->down_ms = 0;
      state->long_sent = false;
    }
    
    if (state->tap_count > 0) {
      if ((now - state->last_up_ms) > DOUBLE_TAP_WINDOW_MS) {
        if (state->tap_count == 1) result = GESTURE_TAP;
        else if (state->tap_count == 2) result = GESTURE_DOUBLE_TAP;
        else if (state->tap_count >= 3) result = GESTURE_TRIPLE_TAP;

        state->tap_count = 0;
      }
    }
  }

  return result;
}