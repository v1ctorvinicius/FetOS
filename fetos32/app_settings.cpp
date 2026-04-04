#include "app_settings.h"
#include "oled.h"
#include "event.h"
#include "button_gesture.h"
#include "launcher.h"
#include "persistence.h"
#include "display_hal.h"

App app_settings;

static int brightness_level = 3;

static void app_settings_render();
static void app_settings_on_event(Event* e);

static void app_settings_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;
  OledState* oled = (OledState*)oled_dev->state;

  ui_center_text(oled, 5, "SETTINGS", 1);

  char buf[32];
  snprintf(buf, sizeof(buf), "Brightness: %d", brightness_level);
  ui_center_text(oled, 30, buf, 1);

  ui_center_text(oled, 50, "Tap: Change | Hold: Exit", 1);
}

static void app_settings_on_event(Event* e) {
  if (e->type == EVENT_BUTTON_PRESS && e->has_payload) {
    int gesture = e->payload.value;

    if (gesture == GESTURE_TAP) {
      brightness_level++;
      if (brightness_level > 5) brightness_level = 1;

      uint8_t brightness_val = 255;
      switch (brightness_level) {
        case 1: brightness_val = 10; break;
        case 2: brightness_val = 60; break;
        case 3: brightness_val = 120; break;
        case 4: brightness_val = 190; break;
        case 5: brightness_val = 255; break;
      }

      persistence_write_int("scr_brt", brightness_val);

      
      
      hal_set_contrast(DISPLAY_DEFAULT_ID, brightness_val);

    } else if (gesture == GESTURE_LONG_PRESS) {
      system_set_app(&launcher_app);
    }
  }
}

void app_settings_setup() {
  app_settings.name = "Settings";
  app_settings.render = app_settings_render;
  app_settings.on_event = app_settings_on_event;
  app_settings.update = nullptr;
  app_settings.update_interval_ms = 0;
  app_settings._task_id = -1;
}