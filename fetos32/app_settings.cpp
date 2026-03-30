#include "app_settings.h"
#include "system.h"
#include "oled.h"
#include "persistence.h"
#include "nav_action.h"

App app_settings;

static const char* options[] = { "Brightness", "Buzzer", "Factory Reset" };
static int current_opt = 0;
static int brightness_val = 255;
static bool buzzer_enabled = true;

void app_settings_setup() {
  app_settings.name = "Settings";
  app_settings.on_event = app_settings_on_event;
  app_settings.render = app_settings_render;
  app_settings.on_enter = app_settings_on_enter;
  app_settings.on_exit = app_settings_on_exit;
  app_settings.update = nullptr;
  app_settings.update_interval_ms = 0;
  app_settings.update_ctx = nullptr;
  app_settings._task_id = -1;
}

void app_settings_on_enter() {
  current_opt = 0;
  brightness_val = persistence_read_int("scr_brt", 255);
  buzzer_enabled = (bool)persistence_read_int("sys_snd", 1);
}

void app_settings_on_exit() {}

void app_settings_on_event(Event* e) {
  NavAction nav = map_gesture_to_nav(e);

  if (nav == NAV_BACK) {
    system_set_app(&launcher_app);
    return;
  }

  if (nav == NAV_NEXT) {
    current_opt = (current_opt + 1) % 3;
  }

  if (nav == NAV_SELECT) {
    if (current_opt == 0) {
      brightness_val -= 85;
      if (brightness_val < 85) brightness_val = 255;

      Device* oled_dev = system_get_device_by_id(2);
      if (oled_dev && oled_dev->state) {
        OledState* st = (OledState*)oled_dev->state;
        st->display->ssd1306_command(SSD1306_SETCONTRAST);
        st->display->ssd1306_command(brightness_val);
      }

      persistence_write_int("scr_brt", brightness_val);

    } else if (current_opt == 1) {
      buzzer_enabled = !buzzer_enabled;
      persistence_write_int("sys_snd", buzzer_enabled ? 1 : 0);
      if (buzzer_enabled) {
        system_request("audio:beep", nullptr);
      }
    }
  }
}

void app_settings_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  ui_center_text(oled, 0, "SETTINGS", 1);

  for (int i = 0; i < 3; i++) {
    int y = 20 + (i * 12);
    char buf[32];

    if (i == 0) snprintf(buf, sizeof(buf), "Bright: %d", brightness_val);
    else if (i == 1) snprintf(buf, sizeof(buf), "Sound: %s", buzzer_enabled ? "ON" : "OFF");
    else snprintf(buf, sizeof(buf), "Reset NVS");

    if (i == current_opt) ui_text_invert(oled, 5, y, buf, 1);
    else ui_text(oled, 5, y, buf, 1);
  }

  oled_flush(oled);
}