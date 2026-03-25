#include "app_debug.h"
#include "system.h"
#include "oled.h"
#include <Arduino.h>
#include "app.h"
#include "button_gesture.h"

App app_debug;

static int event_count = 0;
static int last_event = -1;
static bool ignore_input = false;
static uint32_t enter_time = 0;

void app_debug_on_enter() {
  event_count = 0;
  last_event = -1;

  ignore_input = true;
  enter_time = millis();
}

void app_debug_on_exit() {
}

void app_debug_on_event(Event* e) {
  if (!e) return;

  // if (ignore_input) {
  //   if (millis() - enter_time > 200) {
  //     ignore_input = false;
  //   } else {
  //     return;
  //   }
  // }

  event_count++;
  last_event = e->payload.value;

  if (e->has_payload && e->payload.value == GESTURE_DOUBLE_TAP) {
    system_set_app(&launcher_app);
    return;
  }

  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;

  char buffer[64];
  snprintf(buffer, sizeof(buffer),
           "Events: %d\nLast: %d",
           event_count,
           last_event);

  strcpy(oled->text, buffer);
}

void app_debug_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;

  oled_clear(oled);

  char buffer[64];
  snprintf(buffer, sizeof(buffer),
           "Events: %d\nLast: %d",
           event_count,
           last_event);

  ui_text(oled, 0, 10, buffer, 1);

  oled_flush(oled);
}

void app_debug_setup() {
  app_debug.name = "Debug";
  app_debug.on_event = app_debug_on_event;
  app_debug.render = app_debug_render;
  app_debug.on_enter = app_debug_on_enter;
  app_debug.on_exit = app_debug_on_exit;
}