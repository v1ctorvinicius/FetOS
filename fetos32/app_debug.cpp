#include "app_debug.h"
#include "system.h"
#include "oled.h"
#include <Arduino.h>
#include "button_gesture.h"
#include "event.h"

App app_debug;

enum DebugPage {
  PAGE_SYSTEM,
  PAGE_EVENTS,
  PAGE_INPUT
};

static DebugPage current_page = PAGE_SYSTEM;

static uint32_t total_events = 0;
static const char* last_gesture = "NONE";

#define EVENT_LOG_SIZE 5
static const char* event_log[EVENT_LOG_SIZE];
static int event_log_idx = 0;

static uint32_t last_frame = 0;
static uint32_t frame_time = 0;

int event_count() {
  extern int head;
  extern int tail;

  if (head >= tail) return head - tail;
  return EVENT_QUEUE_SIZE - (tail - head);
}

void app_debug_setup() {
  app_debug.name = "Sys Monitor";
  app_debug.on_event = app_debug_on_event;
  app_debug.render = app_debug_render;
  app_debug.on_enter = app_debug_on_enter;
  app_debug.on_exit = app_debug_on_exit;
}

void app_debug_on_enter() {
  current_page = PAGE_SYSTEM;

  for (int i = 0; i < EVENT_LOG_SIZE; i++) {
    event_log[i] = "-";
  }
}

void app_debug_on_exit() {}

void app_debug_on_event(Event* e) {
  if (!e || !e->has_payload) return;

  total_events++;

  const char* g = "UNK";

  switch (e->payload.value) {
    case GESTURE_TAP: g = "TAP"; break;
    case GESTURE_DOUBLE_TAP: g = "DBL"; break;
    case GESTURE_TRIPLE_TAP: g = "TPL"; break;
    case GESTURE_LONG_PRESS: g = "HOLD"; break;
  }

  last_gesture = g;

  event_log[event_log_idx] = g;
  event_log_idx = (event_log_idx + 1) % EVENT_LOG_SIZE;

  if (e->payload.value == GESTURE_DOUBLE_TAP) {
    system_set_app(&launcher_app);
    return;
  }

  if (e->payload.value == GESTURE_TAP) {
    current_page = (DebugPage)((current_page + 1) % 3);
  }
}

void app_debug_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  char buf1[32];
  char buf2[32];

  uint32_t now = millis();
  frame_time = now - last_frame;
  last_frame = now;

  if (current_page == PAGE_SYSTEM) {
    ui_center_text(oled, 0, "- SYSTEM -", 1);

    uint32_t free_heap = ESP.getFreeHeap() / 1024;
    snprintf(buf1, sizeof(buf1), "RAM: %lu KB", free_heap);

    uint32_t uptime_sec = millis() / 1000;
    snprintf(buf2, sizeof(buf2), "UP: %lu s", uptime_sec);

    ui_text(oled, 0, 20, buf1, 1);
    ui_text(oled, 0, 30, buf2, 1);

    snprintf(buf1, sizeof(buf1), "App: %s", current_app ? current_app->name : "none");
    snprintf(buf2, sizeof(buf2), "Frame: %lums", frame_time);

    ui_text(oled, 0, 45, buf1, 1);
    ui_text(oled, 0, 55, buf2, 1);
  }

  else if (current_page == PAGE_EVENTS) {
    ui_center_text(oled, 0, "- EVENTS -", 1);

    snprintf(buf1, sizeof(buf1), "Total: %lu", total_events);
    snprintf(buf2, sizeof(buf2), "Last: %s", last_gesture);

    ui_text(oled, 0, UI_BODY_Y, buf1, 1);
    ui_text(oled, 0, UI_BODY_Y + 10, buf2, 1);

    snprintf(buf1, sizeof(buf1), "Queue: %d", event_count());
    ui_text(oled, 0, UI_BODY_Y + 20, buf1, 1);

    for (int i = 0; i < EVENT_LOG_SIZE; i++) {
      int idx = (event_log_idx + i) % EVENT_LOG_SIZE;
      ui_text(oled, 60, UI_BODY_Y + 30     + i * 9, event_log[idx], 1);
    }
  }

  else if (current_page == PAGE_INPUT) {
    ui_center_text(oled, 0, "- INPUT -", 1);

    Device* btn = system_get_device_by_id(1);
    if (btn && btn->state) {
      ButtonGestureState* st = (ButtonGestureState*)btn->state;

      snprintf(buf1, sizeof(buf1), "tap:%d long:%d", st->tap_count, st->long_sent);
      snprintf(buf2, sizeof(buf2), "block:%d", st->block_until_release);

      ui_text(oled, 0, 20, buf1, 1);
      ui_text(oled, 0, 35, buf2, 1);

      snprintf(buf1, sizeof(buf1), "down:%lu", st->down_ms);
      snprintf(buf2, sizeof(buf2), "up:%lu", st->last_up_ms);

      ui_text(oled, 0, 50, buf1, 1);
      ui_text(oled, 0, 60, buf2, 1);
    }
  }

  oled_flush(oled);
}