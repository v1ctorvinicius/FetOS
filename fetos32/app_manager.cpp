#include "app_manager.h"
#include "oled.h"
#include "event.h"
#include "button_gesture.h"
#include "launcher.h"
#include <Arduino.h>

typedef enum {
  APP_MGR_LIST,
  APP_MGR_CONFIRM
} MgrState;

static MgrState mgr_state = APP_MGR_LIST;
static int selected_idx = 0;

App app_manager;

static void app_manager_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;
  OledState* oled = (OledState*)oled_dev->state;

  int total_apps = system_get_app_count();

  if (mgr_state == APP_MGR_LIST) {
    ui_center_text(oled, 0, "GERENCIAR APPS", 1);
    oled->display->drawFastHLine(0, 10, 128, SSD1306_WHITE);

    if (total_apps <= 0) {
      ui_center_text(oled, 30, "Lista vazia", 1);
    } else {
      const char* names[MAX_APPS];
      for (int i = 0; i < total_apps; i++) {
        App* a = system_get_app_by_index(i);
        names[i] = a ? a->name : "???";
      }
      ui_list_scroll(oled, names, total_apps, selected_idx, 12);
    }
  } else if (mgr_state == APP_MGR_CONFIRM) {
    App* target = system_get_app_by_index(selected_idx);
    ui_center_text(oled, 5, "APAGAR ESTE APP?", 1);
    oled->display->drawRect(2, 22, 124, 20, SSD1306_WHITE);

    if (target) {
      ui_center_text(oled, 28, target->name, 1);
    }

    ui_center_text(oled, 50, "HOLD: Sim | TAP: Nao", 1);
  }
}

static void app_manager_on_event(Event* e) {
  if (e->type != EVENT_BUTTON_PRESS || !e->has_payload) return;

  int gesture = e->payload.value;
  int total_apps = system_get_app_count();

  if (mgr_state == APP_MGR_LIST) {
    if (gesture == GESTURE_TAP) {
      if (total_apps > 0) {
        selected_idx = (selected_idx + 1) % total_apps;
      }
    } else if (gesture == GESTURE_DOUBLE_TAP) {
      system_set_app(&launcher_app);
    } else if (gesture == GESTURE_LONG_PRESS) {
      App* target = system_get_app_by_index(selected_idx);
      if (target && target != &launcher_app && target != &app_manager) {
        mgr_state = APP_MGR_CONFIRM;
      }
    }
  } else if (mgr_state == APP_MGR_CONFIRM) {
    if (gesture == GESTURE_LONG_PRESS) {
      App* target = system_get_app_by_index(selected_idx);
      if (target) {
        system_unregister_app(target);
        if (selected_idx >= system_get_app_count()) selected_idx = 0;
      }
      mgr_state = APP_MGR_LIST;
    } else if (gesture == GESTURE_TAP) {
      mgr_state = APP_MGR_LIST;
    }
  }
}

void app_manager_setup() {
  app_manager.name = "App Manager";
  app_manager.render = app_manager_render;
  app_manager.on_event = app_manager_on_event;
  app_manager.update = nullptr;
  app_manager.update_interval_ms = 0;
  app_manager._task_id = -1;
}