#include "system.h"
#include "event.h"
#include "launcher.h"
#include "scheduler.h"
#include "Arduino.h"
#include "persistence.h"
#include "capability.h"
#include "device_ble.h"
#include <LittleFS.h>

#include "rgb_led.h"
#include "oled.h"
#include "buzzer.h"
#include "button.h"
#include "buffer_device.h"

#include "app_debug.h"
#include "app_pomodoro.h"
#include "app_clock.h"
#include "app_settings.h"
#include "app_buzzer.h"

#define MAX_DEVICES 6

Device devices[MAX_DEVICES];
int device_count = 0;
App *current_app = nullptr;

App launcher_app;

static uint32_t input_block_until = 0;

SystemState system_state = SYS_LOCK;

App *registered_apps[MAX_APPS];
int registered_app_count = 0;

static bool is_uploading = false;
static File upload_file;
static uint32_t upload_bytes_remaining = 0;

static void task_serial_uploader(void *ctx)
{
  while (Serial.available() > 0)
  {
    if (!is_uploading)
    {
      String line = Serial.readStringUntil('\n');
      line.trim();

      if (line.length() > 0)
      {
        Serial.println("[Uploader] Ouvi: '" + line + "'");

        if (line.startsWith("FVM_UPLOAD "))
        {
          int firstSpace = line.indexOf(' ');
          int secondSpace = line.indexOf(' ', firstSpace + 1);

          if (firstSpace > 0 && secondSpace > 0)
          {
            String filename = line.substring(firstSpace + 1, secondSpace);
            String sizeStr = line.substring(secondSpace + 1);
            upload_bytes_remaining = sizeStr.toInt();

            if (!filename.startsWith("/"))
              filename = "/" + filename;

            upload_file = LittleFS.open(filename, "w");
            if (upload_file)
            {
              is_uploading = true;
              Serial.println("OK");
            }
            else
            {
              Serial.println("ERR_FS");
            }
          }
          else
          {
            Serial.println("[Uploader] Erro: Faltam parametros no comando.");
          }
        }
      }
    }
    else
    {
      uint8_t buf[64];
      int to_read = Serial.available();
      if (to_read > (int)sizeof(buf))
        to_read = sizeof(buf);
      if (to_read > (int)upload_bytes_remaining)
        to_read = upload_bytes_remaining;

      int bytes_read = Serial.readBytes((char *)buf, to_read);
      if (bytes_read > 0)
      {
        upload_file.write(buf, bytes_read);
        upload_bytes_remaining -= bytes_read;
      }

      if (upload_bytes_remaining == 0)
      {
        upload_file.close();
        is_uploading = false;
        Serial.println("DONE");
        system_discover_fs_apps();
      }
    }
  }
}

bool system_register_capability(const char *cap, CapabilityHandler handler, Device *dev)
{
  return capability_register(cap, handler, dev);
}

RequestResult system_request(const char *cap, const RequestPayload *payload)
{
  return capability_request_native(cap, payload);
}

RequestResult system_request_fvm(const char *cap, const RequestPayload *payload, CallerContext *caller)
{
  return capability_request(cap, payload, caller);
}

static void task_events(void *ctx)
{
  Event e;
  while (event_pop(&e))
  {
    if (millis() < input_block_until)
      continue;

    if (system_state == SYS_LOCK)
    {
      if (e.has_payload && e.payload.value == GESTURE_LONG_PRESS)
      {
        system_state = SYS_RUNNING;
        system_set_app(&launcher_app);
      }
      continue;
    }

    if (system_state == SYS_RUNNING)
    {
      if (e.type == EVENT_BUTTON_PRESS)
      {
        if (current_app && current_app->on_event)
          current_app->on_event(&e);
      }
      else
      {
        for (int i = 0; i < registered_app_count; i++)
        {
          App *a = registered_apps[i];
          if (a->_task_id >= 0 && a->on_event)
            a->on_event(&e);
        }
      }
    }

    for (int i = 0; i < device_count; i++)
    {
      if (devices[i].on_event)
        devices[i].on_event(&devices[i], &e);
    }
  }
}

static void task_render(void *ctx)
{
  for (int i = 0; i < device_count; i++)
  {
    if (devices[i].render)
      devices[i].render(&devices[i]);
  }
}

static void task_debug_serial(void *ctx)
{
  Serial.println("=== SYSTEM ===");
  Serial.print("Focus: ");
  Serial.println(current_app ? current_app->name : "none");
  for (int i = 0; i < registered_app_count; i++)
  {
    App *a = registered_apps[i];
    Serial.print("  app:");
    Serial.print(a->name);
    Serial.print(" task:");
    Serial.println(a->_task_id);
  }
  Serial.println("==============");
}

void system_init()
{
  event_init();
  scheduler_init();
  capability_init();

  Device button = {.id = 1, .pin = 4, .init = button_init, .poll = button_poll, .render = NULL, .on_event = NULL};
  Device oled = {.id = 2, .pin = 0, .init = oled_init, .poll = NULL, .render = oled_render, .on_event = oled_on_event};
  Device buzzer = {.id = 3, .pin = 15, .init = buzzer_init, .poll = NULL, .render = buzzer_render, .on_event = NULL};
  Device rgb = {.id = 4, .pin = 16, .init = rgb_init, .poll = NULL, .render = NULL, .on_event = NULL};
  Device buffer = {.id = 5, .pin = 0, .init = buffer_device_init, .poll = NULL, .render = NULL, .on_event = NULL};

  register_device(button);
  register_device(oled);
  register_device(buzzer);
  register_device(rgb);
  register_device(buffer);

  for (int i = 0; i < device_count; i++)
  {
    if (devices[i].init)
      devices[i].init(&devices[i]);
  }

  persistence_init();
  int saved_brightness = persistence_read_int("scr_brt", 255);
  Device *oled_dev = system_get_device_by_id(2);
  if (oled_dev && oled_dev->state)
  {
    OledState *st = (OledState *)oled_dev->state;
    st->display->ssd1306_command(SSD1306_SETCONTRAST);
    st->display->ssd1306_command(saved_brightness);
  }

  scheduler_add(task_events, nullptr, 10, TASK_PRIORITY_HIGH, "events");
  scheduler_add(task_render, nullptr, 33, TASK_PRIORITY_NORMAL, "render");
  scheduler_add(task_debug_serial, nullptr, 2000, TASK_PRIORITY_LOW, "dbg_serial");
  scheduler_add(task_serial_uploader, nullptr, 50, TASK_PRIORITY_NORMAL, "uploader");

  app_launcher_setup();
  app_debug_setup();
  app_pomodoro_setup();
  app_clock_setup();
  app_settings_setup();
  app_buzzer_setup();

  system_register_app(&app_debug);
  system_register_app(&app_pomodoro);
  system_register_app(&app_clock);
  system_register_app(&app_settings);
  system_register_app(&app_buzzer);

  system_discover_fs_apps();

  system_launch_app(&launcher_app);
  system_focus_app(&launcher_app);
}

void system_loop() { scheduler_run(); }

void register_device(Device dev) { devices[device_count++] = dev; }

Device *system_get_device_by_id(uint8_t id)
{
  for (int i = 0; i < device_count; i++)
  {
    if (devices[i].id == id)
      return &devices[i];
  }
  return nullptr;
}

static bool app_is_fvm(App *app)
{
  if (!app)
    return false;
  return (app->update != nullptr && app->update_ctx != nullptr);
}

void system_launch_app(App *app)
{
  if (!app || app->_task_id >= 0)
    return;

  if (app->update && app->update_interval_ms > 0)
  {
    app->_task_id = scheduler_add(app->update, app->update_ctx,
                                  app->update_interval_ms,
                                  TASK_PRIORITY_NORMAL, app->name);
  }
  else
  {
    app->_task_id = 0;
  }

  if (app->on_enter)
    app->on_enter();
}

void system_focus_app(App *app)
{
  if (!app)
    return;

  input_block_until = millis() + 250;
  event_clear();
  button_reset();

  if (app_is_fvm(app))
  {
    oled_set_owner(OLED_OWNER_FVM);
  }
  else
  {
    oled_set_owner(OLED_OWNER_KERNEL);
  }

  current_app = app;
}

void system_kill_app(App *app)
{
  if (!app || app->_task_id < 0)
    return;

  if (app->_task_id > 0)
    scheduler_remove(app->_task_id);
  app->_task_id = -1;

  if (app->on_exit)
    app->on_exit();

  if (current_app == app)
    system_focus_app(&launcher_app);
}

void system_set_app(App *app)
{
  if (!app)
    return;

  for (int i = 0; i < system_get_app_count(); i++)
  {
    if (system_get_app_by_index(i) == app)
    {
      persistence_write_int("last_app", i);
      break;
    }
  }

  if (app->_task_id < 0)
    system_launch_app(app);
  system_focus_app(app);
}

void system_render()
{
  Device *oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state)
    return;

  OledState *oled = (OledState *)oled_dev->state;
  oled_clear(oled);

  switch (system_state)
  {
  case SYS_LOCK:
    ui_center_text(oled, 0, "v0.2", 1);
    oled->display->setCursor(20, 20);
    oled->display->setTextSize(3);
    oled->display->printf("FetOS");
    oled->display->setTextSize(1);
    oled->display->printf("32");
    ui_center_text(oled, 55, "Hold to unlock", 1);
    break;
  case SYS_RUNNING:
    if (current_app && current_app->render)
      current_app->render();
    break;
  }

  oled_flush(oled);
}

bool system_register_app(App *app)
{
  if (registered_app_count >= MAX_APPS)
    return false;
  app->_task_id = -1;
  registered_apps[registered_app_count++] = app;
  return true;
}

int system_get_app_count() { return registered_app_count; }
App *system_get_app_by_index(int index)
{
  if (index < 0 || index >= registered_app_count)
    return nullptr;
  return registered_apps[index];
}

void system_discover_fs_apps()
{
  Serial.println("[System] Buscando apps no FS...");
  File root = LittleFS.open("/");
  if (!root)
    return;

  File file = root.openNextFile();
  while (file)
  {
    String fileName = file.name();
    if (fileName.endsWith(".fvm"))
    {
      String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;

      bool already_registered = false;
      for (int i = 0; i < registered_app_count; i++)
      {
        if (String(registered_apps[i]->name) == fullPath)
        {
          already_registered = true;
          break;
        }
      }

      if (!already_registered)
      {
        Serial.printf("[System] App detectado: %s\n", fullPath.c_str());
        App *new_app = fvm_app_load_fs(fullPath.c_str());
        if (new_app)
          system_register_app(new_app);
      }
    }
    file = root.openNextFile();
  }
}