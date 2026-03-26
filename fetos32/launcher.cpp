#include "system.h"
#include "oled.h"
#include "button.h"
#include "button_gesture.h"
#include <Arduino.h>
#include "scheduler.h"

extern App app_debug;

static void launcher_task();

int current_index = 0;
static int launcher_task_id = -1;

static void draw_launcher(OledState* oled) {
  oled_clear(oled);
  ui_center_text(oled, 0, "Apps", 1);

  int total = system_get_app_count();
  if (total > 0) {
    const char* app_names[MAX_APPS];
    for (int i = 0; i < total; i++) {
      app_names[i] = system_get_app_by_index(i)->name;
    }
    ui_list_scroll(oled, app_names, total, current_index, UI_BODY_Y);
  }

  oled_flush(oled);
}

static void draw_lock(OledState* oled) {
  ui_center_text(oled, 20, "Lock?", 2);
  ui_center_text(oled, 45, "Hold = yes", 1);
}

void launcher_on_enter() {
  state = LAUNCHER_STATE_BROWSING;
}

void launcher_on_exit() {
  if (launcher_task_id >= 0) {
    scheduler_remove(launcher_task_id);
    launcher_task_id = -1;
  }
}

void launcher_on_event(Event* e) {
  if (!e || !e->has_payload) return;

  NavAction nav = map_gesture_to_nav(e);
  if (nav == NAV_NONE) return;

  int total = system_get_app_count();
  if (total == 0) return;

  switch (state) {
    case LAUNCHER_STATE_BROWSING:
      if (nav == NAV_NEXT) {
        current_index = (current_index + 1) % total;
      } else if (nav == NAV_PREV) {
        current_index = (current_index - 1 + total) % total;
      } else if (nav == NAV_SELECT) {
        App* selected = system_get_app_by_index(current_index);
        if (selected) {
          system_set_app(selected);
        }
      } else if (nav == NAV_BACK) {
        state = LAUNCHER_STATE_CONFIRM_LOCK;
      }
      break;

    case LAUNCHER_STATE_CONFIRM_LOCK:
      if (nav == NAV_SELECT) {
        system_state = SYS_LOCK;
        current_app = nullptr;
        event_clear();
        button_reset();
        state = LAUNCHER_STATE_BROWSING;
      } else {
        state = LAUNCHER_STATE_BROWSING;
      }
      break;
  }
}

void launcher_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;

  if (state == LAUNCHER_STATE_BROWSING) {
    ui_center_text(oled, 0, "Apps", 2);

    int total = system_get_app_count();
    if (total > 0) {
      const char* app_names[MAX_APPS];
      for (int i = 0; i < total; i++) {
        app_names[i] = system_get_app_by_index(i)->name;
      }
      ui_list_scroll(oled, app_names, total, current_index, UI_BODY_Y);
    }
  } else if (state == LAUNCHER_STATE_CONFIRM_LOCK) {
    ui_center_text(oled, 20, "Lock?", 2);
    ui_center_text(oled, 45, "Hold = yes", 1);
  }
}

void launcher_task() {
  if (current_app && current_app->render) {
    current_app->render();
  }
}

void app_launcher_setup(){
  launcher_app.name = "Launcher";
  launcher_app.on_event = launcher_on_event;
  launcher_app.render = launcher_render;
  launcher_app.on_enter = launcher_on_enter;
  launcher_app.on_exit = launcher_on_exit;
}