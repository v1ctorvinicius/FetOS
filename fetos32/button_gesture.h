//button_gesture.h
#pragma once
#include "device.h"

#define DEBOUNCE_MS 50
#define LONG_PRESS_MS 500
#define DOUBLE_TAP_WINDOW_MS 300
#define TRIPLE_TAP_WINDOW_MS 500

typedef enum
{
  GESTURE_NONE,
  GESTURE_TAP,
  GESTURE_DOUBLE_TAP,
  GESTURE_TRIPLE_TAP,
  GESTURE_LONG_PRESS,
  GESTURE_LONG_PRESS_PREVIEW
} GestureType;

typedef struct
{
  int last_state;
  int stable_state;
  unsigned long last_debounce;
  bool block_until_release;
  uint32_t down_ms;
  uint32_t last_up_ms;
  uint8_t tap_count;
  bool long_sent;
} ButtonGestureState;