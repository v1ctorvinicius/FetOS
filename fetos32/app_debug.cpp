#include "app_debug.h"
#include "system.h"
#include "oled.h"
#include <Arduino.h>
#include "button_gesture.h"

App app_debug;

enum DebugPage {
  PAGE_SYSTEM,
  PAGE_EVENTS
};

static DebugPage current_page = PAGE_SYSTEM;
static uint32_t total_events = 0;
static const char* last_gesture = "NONE";

void app_debug_setup() {
  app_debug.name = "Sys Monitor";
  app_debug.on_event = app_debug_on_event;
  app_debug.render = app_debug_render;
  app_debug.on_enter = app_debug_on_enter;
  app_debug.on_exit = app_debug_on_exit;
}

void app_debug_on_enter() {
  current_page = PAGE_SYSTEM;
}

void app_debug_on_exit() {
}

void app_debug_on_event(Event* e) {
  if (!e) return;

  total_events++;

  if (e->has_payload) {
    switch (e->payload.value) {
      case GESTURE_TAP: last_gesture = "TAP"; break;
      case GESTURE_DOUBLE_TAP: last_gesture = "DBL_TAP"; break;
      case GESTURE_TRIPLE_TAP: last_gesture = "TPL_TAP"; break;
      case GESTURE_LONG_PRESS: last_gesture = "HOLD"; break;
      default: last_gesture = "UNKNOWN"; break;
    }

    if (e->payload.value == GESTURE_DOUBLE_TAP) {
      system_set_app(&launcher_app);
      return;
    }

    if (e->payload.value == GESTURE_TAP) {
      if (current_page == PAGE_SYSTEM) current_page = PAGE_EVENTS;
      else current_page = PAGE_SYSTEM;
    }
  }
}

void app_debug_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  char buf1[32];
  char buf2[32];

  if (current_page == PAGE_SYSTEM) {
    ui_center_text(oled, 0, "- SYSTEM -", 1);

    uint32_t free_heap = ESP.getFreeHeap() / 1024;
    snprintf(buf1, sizeof(buf1), "RAM: %lu KB", free_heap);

    uint32_t uptime_sec = millis() / 1000;
    snprintf(buf2, sizeof(buf2), "UP: %lu s", uptime_sec);

    ui_text(oled, 0, 25, buf1, 1);
    ui_text(oled, 0, 40, buf2, 1);
  } else if (current_page == PAGE_EVENTS) {
    ui_center_text(oled, 0, "- EVENTS -", 1);

    snprintf(buf1, sizeof(buf1), "Total: %lu", total_events);
    snprintf(buf2, sizeof(buf2), "Last: %s", last_gesture);

    ui_text(oled, 0, 25, buf1, 1);
    ui_text(oled, 0, 40, buf2, 1);
  }

  oled_flush(oled);
}