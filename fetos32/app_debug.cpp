#include "app_debug.h"
#include "system.h"
#include "oled.h"
#include <Arduino.h>
#include "button_gesture.h"
#include "event.h"
#include "scheduler.h"

App app_debug;

enum DebugPage {
  PAGE_SYSTEM,
  PAGE_TASKS,
  PAGE_EVENTS,
  PAGE_INPUT,
  PAGE_COUNT
};

static DebugPage current_page = PAGE_SYSTEM;

static uint32_t total_events = 0;
static const char* last_gesture = "NONE";

#define EVENT_LOG_SIZE 5
static const char* event_log[EVENT_LOG_SIZE];
static int event_log_idx = 0;

static uint32_t last_frame = 0;
static uint32_t frame_time = 0;

static int task_scroll = 0;
static int system_scroll = 0;

static bool task_detail_mode = false;
static bool input_locked = false;

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
  app_debug.update = nullptr;
  app_debug.update_interval_ms = 0;
  app_debug.update_ctx = nullptr;
  app_debug._task_id = -1;
}

void app_debug_on_enter() {
  current_page = PAGE_SYSTEM;
  task_scroll = 0;
  system_scroll = 0;

  for (int i = 0; i < EVENT_LOG_SIZE; i++) {
    event_log[i] = "-";
  }
  input_locked = false;
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

  if (current_page == PAGE_SYSTEM) {
    if (e->payload.value == GESTURE_TRIPLE_TAP) {
      const int METRIC_COUNT = 5;
      const int VISIBLE_ITEMS = 4;

      int max_scroll = METRIC_COUNT - VISIBLE_ITEMS;
      if (max_scroll < 0) max_scroll = 0;

      system_scroll = (system_scroll + 1) % (max_scroll + 1);
      return;
    }
  }

  if (current_page == PAGE_TASKS) {
    if (e->payload.value == GESTURE_LONG_PRESS) {
      task_detail_mode = !task_detail_mode;
      return;
    }

    if (e->payload.value == GESTURE_TRIPLE_TAP && !task_detail_mode) {
      int active_count = 0;
      for (int i = 0; i < MAX_TASKS; i++) {
        if (scheduler_get_task(i) && scheduler_get_task(i)->active) active_count++;
      }
      if (active_count > 0) task_scroll = (task_scroll + 1) % active_count;
      return;
    }
    if (task_detail_mode) return;
  }

  if (current_page == PAGE_INPUT) {
    if (e->payload.value == GESTURE_LONG_PRESS) {
      input_locked = !input_locked;
      return;
    }
    if (input_locked) return;
  }

  if (e->payload.value == GESTURE_DOUBLE_TAP) {
    system_set_app(&launcher_app);
    return;
  }

  if (e->payload.value == GESTURE_TAP) {
    current_page = (DebugPage)((current_page + 1) % PAGE_COUNT);
    task_scroll = 0;
    task_detail_mode = false;
    input_locked = false;
    return;
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

  // ── PAGE_SYSTEM (HEALTH) ─────────────────────────────────
  if (current_page == PAGE_SYSTEM) {
    ui_center_text(oled, 0, "- HEALTH -", 1);

    const int METRIC_COUNT = 5;
    char metrics_buf[METRIC_COUNT][24];
    const char* list_items[METRIC_COUNT];

    snprintf(metrics_buf[0], 24, "RAM: %lu KB", ESP.getFreeHeap() / 1024);
    snprintf(metrics_buf[1], 24, "MaxBlk: %lu KB", ESP.getMaxAllocHeap() / 1024);
    snprintf(metrics_buf[2], 24, "Uptime: %lu s", millis() / 1000);
    snprintf(metrics_buf[3], 24, "CPU: %lu MHz", ESP.getCpuFreqMHz());
    snprintf(metrics_buf[4], 24, "Frame: %lums", frame_time);

    for (int i = 0; i < METRIC_COUNT; i++) {
      list_items[i] = metrics_buf[i];
    }

    ui_text_scroll(oled, list_items, METRIC_COUNT, system_scroll, UI_BODY_Y, 4);
  }

  // ── PAGE_TASKS ───────────────────────────────────────────
  else if (current_page == PAGE_TASKS) {
    int active_indices[MAX_TASKS];

    char task_names[MAX_TASKS][16];
    const char* list_items[MAX_TASKS];
    int active_count = 0;

    for (int i = 0; i < MAX_TASKS; i++) {
      const Task* t = scheduler_get_task(i);
      if (t && t->active) {
        active_indices[active_count] = i;
        snprintf(task_names[active_count], 16, "%s", t->name);
        list_items[active_count] = task_names[active_count];
        active_count++;
      }
    }

    if (active_count == 0) {
      ui_center_text(oled, 0, "- TASKS -", 1);
      ui_center_text(oled, 30, "no active tasks", 1);
    } else {
      if (task_scroll >= active_count) task_scroll = 0;
      const Task* selected_task = scheduler_get_task(active_indices[task_scroll]);

      if (task_detail_mode) {
        ui_center_text(oled, 0, "- TASK INFO -", 1);

        snprintf(buf1, sizeof(buf1), "Name: %s", selected_task->name);
        ui_text(oled, 0, UI_BODY_Y, buf1, 1);

        const char* prio = selected_task->priority == TASK_PRIORITY_HIGH ? "HIGH" : selected_task->priority == TASK_PRIORITY_NORMAL ? "NORM"
                                                                                                                                    : "LOW";
        snprintf(buf1, sizeof(buf1), "Prio: %s", prio);
        ui_text(oled, 0, UI_BODY_Y + 10, buf1, 1);

        snprintf(buf1, sizeof(buf1), "Time: %lums", selected_task->last_duration_ms);
        ui_text(oled, 0, UI_BODY_Y + 20, buf1, 1);

        snprintf(buf1, sizeof(buf1), "Overruns: %lu", selected_task->overruns);
        ui_text(oled, 0, UI_BODY_Y + 30, buf1, 1);

        ui_text(oled, 0, 64 - 7, "HOLD: back", 1);

      } else {
        ui_center_text(oled, 0, "- TASKS -", 1);

        ui_list_scroll(oled, list_items, active_count, task_scroll, UI_BODY_Y, 4);

        ui_text(oled, 0, 64 - 7, "3x:move  HOLD:info", 1);
      }
    }
  }

  // ── PAGE_EVENTS ──────────────────────────────────────────
  else if (current_page == PAGE_EVENTS) {
    ui_center_text(oled, 0, "- EVENTS -", 1);

    // coluna esquerda
    snprintf(buf1, sizeof(buf1), "Total: %lu", total_events);
    snprintf(buf2, sizeof(buf2), "Last: %s", last_gesture);
    ui_text(oled, 0, UI_BODY_Y, buf1, 1);
    ui_text(oled, 0, UI_BODY_Y + 10, buf2, 1);

    snprintf(buf1, sizeof(buf1), "Queue: %d", event_count());
    ui_text(oled, 0, UI_BODY_Y + 20, buf1, 1);

    // coluna direita
    for (int i = 0; i < EVENT_LOG_SIZE; i++) {
      int idx = (event_log_idx + i) % EVENT_LOG_SIZE;


      ui_text(oled, 64, UI_BODY_Y + (i * 10), event_log[idx], 1);
    }
  }

  // ── PAGE_INPUT ───────────────────────────────────────────
  else if (current_page == PAGE_INPUT) {

    if (input_locked) {
      ui_center_text(oled, 0, "- INPUT (LOCKED) -", 1);
    } else {
      ui_center_text(oled, 0, "- INPUT -", 1);
    }

    Device* btn = system_get_device_by_id(1);
    if (btn && btn->state) {
      ButtonGestureState* st = (ButtonGestureState*)btn->state;


      snprintf(buf1, sizeof(buf1), "tap:%d long:%d", st->tap_count, st->long_sent);
      snprintf(buf2, sizeof(buf2), "block:%d", st->block_until_release);
      ui_text(oled, 0, 16, buf1, 1);
      ui_text(oled, 0, 26, buf2, 1);

      snprintf(buf1, sizeof(buf1), "down:%lu", st->down_ms);
      snprintf(buf2, sizeof(buf2), "up:%lu", st->last_up_ms);
      ui_text(oled, 0, 38, buf1, 1);
      ui_text(oled, 0, 48, buf2, 1);
    }


    ui_text(oled, 0, 64 - 7, "HOLD: lock/unlock", 1);
  }

  oled_flush(oled);
}