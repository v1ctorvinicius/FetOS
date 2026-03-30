#include "app_buzzer.h"
#include "system.h"
#include "oled.h"
#include "nav_action.h"
#include "buzzer.h"

App app_buzzer;

static const char* options[] = { "Beep", "Beep Beep", "Stop" };
static int current_opt = 0;

void app_buzzer_setup() {
  app_buzzer.name = "Buzzer Test";
  app_buzzer.on_event = app_buzzer_on_event;
  app_buzzer.render = app_buzzer_render;
  app_buzzer.on_enter = app_buzzer_on_enter;
  app_buzzer.on_exit = app_buzzer_on_exit;
  app_buzzer.update = nullptr;
  app_buzzer.update_interval_ms = 0;
  app_buzzer.update_ctx = nullptr;
  app_buzzer._task_id = -1;
}

void app_buzzer_on_enter() {
  current_opt = 0;
}

void app_buzzer_on_exit() {
  system_request("audio:stop", nullptr);
}

void app_buzzer_on_event(Event* e) {
  NavAction nav = map_gesture_to_nav(e);

  if (nav == NAV_BACK) {
    system_set_app(&launcher_app);
    return;
  }

  if (nav == NAV_NEXT) current_opt = (current_opt + 1) % 3;
  if (nav == NAV_PREV) current_opt = (current_opt - 1 + 3) % 3;

  if (nav == NAV_SELECT) {
    switch (current_opt) {
      case 0: system_request("audio:beep", nullptr); break;
      case 1: system_request("audio:beep_beep", nullptr); break;
      case 2: system_request("audio:stop", nullptr); break;
    }
  }
}

void app_buzzer_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  ui_center_text(oled, 0, "BUZZER TEST", 1);

  for (int i = 0; i < 3; i++) {
    int y = 20 + (i * 12);
    if (i == current_opt) ui_text_invert(oled, 10, y, options[i], 1);
    else ui_text(oled, 10, y, options[i], 1);
  }

  oled_flush(oled);
}