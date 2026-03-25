#include "system.h"
#include "event.h"
#include "button.h"
#include "oled.h"
#include "launcher.h"
#include "Arduino.h"
#include "app_debug.h"
#include "scheduler.h"

#define MAX_DEVICES 4

Device devices[MAX_DEVICES];
int device_count = 0;
App* current_app = nullptr;

App launcher_app;

static uint32_t input_block_until = 0;

SystemState system_state = SYS_LOCK;

static void event_task() {
  Event e;
  while (event_pop(&e)) {
    for (int i = 0; i < device_count; i++) {
      if (devices[i].on_event) {
        devices[i].on_event(&devices[i], &e);
      }
    }
  }
}

void system_init() {
  event_init();
  scheduler_init();

  Device button = {
    .id = 1,
    .pin = 4,
    .init = button_init,
    .poll = button_poll,
    .render = NULL,
    .on_event = NULL
  };

  Device oled = {
    .id = 2,
    .pin = 0,
    .init = oled_init,
    .poll = NULL,
    .render = oled_render,
    .on_event = oled_on_event
  };

  register_device(button);
  register_device(oled);

  for (int i = 0; i < device_count; i++) {
    if (devices[i].init)
      devices[i].init(&devices[i]);
  }

  launcher_app.name = "Launcher";
  launcher_app.on_event = launcher_on_event;
  launcher_app.render = launcher_render;
  launcher_app.on_enter = launcher_on_enter;
  launcher_app.on_exit = launcher_on_exit;

  app_debug_setup();

  current_app = &launcher_app;
  current_app->on_enter();

  // !debug
  Serial.println("REGISTERED DEVICES:");
  for (int i = 0; i < device_count; i++) {
    Serial.println(devices[i].id);
  }
}

void system_loop() {
  scheduler_run();

  Event e;
  while (event_pop(&e)) {
    if (millis() < input_block_until) continue;

    if (system_state == SYS_LOCK) {
      if (e.has_payload && e.payload.value == GESTURE_LONG_PRESS) {
        system_state = SYS_RUNNING;
        system_set_app(&launcher_app);
      }
      continue;
    }

    if (system_state == SYS_RUNNING && current_app) {
      current_app->on_event(&e);
    }

    for (int i = 0; i < device_count; i++) {
      if (devices[i].on_event) {
        devices[i].on_event(&devices[i], &e);
      }
    }
  }

  for (int i = 0; i < device_count; i++) {
    if (devices[i].render) {
      devices[i].render(&devices[i]);
    }
  }

  // !debug
  static unsigned long last_dbg = 0;
  if (millis() - last_dbg > 2000) {
    last_dbg = millis();

    Serial.println("=== TASKS ===");
    Serial.print("Current app: ");
    Serial.println(current_app ? current_app->name : "none");

    for (int i = 0; i < device_count; i++) {
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(" id: ");
      Serial.print(devices[i].id);
      Serial.print(" state ptr: ");
      Serial.println((uint32_t)devices[i].state);
    }

    Serial.println("==============");
  }
}

void register_device(Device dev) {
  devices[device_count++] = dev;
}

Device* system_get_device_by_id(uint8_t id) {
  for (int i = 0; i < device_count; i++) {
    if (devices[i].id == id) return &devices[i];
  }
  return nullptr;
}

void system_set_app(App* app) {
  input_block_until = millis() + 250;

  if (current_app && current_app->on_exit) {
    current_app->on_exit();
  }

  event_clear();
  button_reset();

  delay(50);

  Device* oled_dev = system_get_device_by_id(2);
  if (oled_dev && oled_dev->state) {
    OledState* st = (OledState*)oled_dev->state;
    st->dirty = true;
    strcpy(st->text, "");
  }

  current_app = app;

  if (current_app && current_app->on_enter) {
    current_app->on_enter();
  }
}


void system_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;

  oled_clear(oled);

  switch (system_state) {
    case SYS_LOCK:
      ui_center_text(oled, 20, "Locked", 2);
      ui_center_text(oled, 45, "Hold to unlock", 1);
      break;

    case SYS_RUNNING:
      if (current_app && current_app->render)
        current_app->render();
      break;
  }

  oled_flush(oled);
}