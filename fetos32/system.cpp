#include "system.h"
#include "hardware_profile.h"
#include "event.h"
#include "launcher.h"
#include "scheduler.h"
#include "Arduino.h"
#include "persistence.h"
#include "capability.h"
#include <LittleFS.h>
#include "fetlink.h"

#include "rgb_led.h"
#include "oled.h"
#include "buzzer.h"
#include "button.h"
#include "buffer_device.h"
#include "fetlink_dual.h"
#include "device_net.h"
#include "text_buffer_device.h"
#include "ui_device.h"
#include "keyboard_device.h"


#include "display_hal.h"
#include "driver_ssd1306.h"

#include "app_debug.h"
#include "app_pomodoro.h"
#include "app_clock.h"
#include "app_settings.h"
#include "app_buzzer.h"
#include "app_manager.h"
#include "app_keyboard.h"
#include "app_airdrop.h"

#define MAX_DEVICES 16

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

bool is_system_mutating = false;
static char cmd_buf[128];
static uint8_t cmd_len = 0;

static uint8_t s_app_event_mailbox = 0;

void task_uart_keyboard(void *ctx);

static RequestResult handle_millis(Device* dev, const RequestPayload* p, CallerContext* c) {
  // Retorna o tempo direto pro FetScript no slot 0
  p->params[0].int_value = millis(); 
  return REQ_ACCEPTED;
}

static RequestResult handle_get_event(Device *dev, const RequestPayload *payload, CallerContext *caller) {
  // NÃO use event_pop aqui! Leia da Mailbox que a task_events preencheu.
  payload->params[0].int_value = s_app_event_mailbox;

  // Limpa a caixa após a leitura para o script não ler o mesmo TAP repetidamente
  s_app_event_mailbox = 0;

  return REQ_ACCEPTED;
}

static RequestResult handle_sys_exit(Device *dev, const RequestPayload *payload, CallerContext *caller) {
  oled_set_owner(OLED_OWNER_KERNEL);
  system_set_app(&launcher_app);
  return REQ_ACCEPTED;
}

static uint32_t last_rx_time = 0;
static void task_serial_uploader(void *ctx) {
  yield();  // Mantém o watchdog do ESP32 feliz

  if (!is_uploading) {
    // ── MODO COMANDO: Aguardando instruções em texto ──
    while (Serial.available() > 0) {
      char c = (char)Serial.read();

      // Fim da linha (comando recebido)
      if (c == '\n' || c == '\r') {
        if (cmd_len == 0) continue;
        cmd_buf[cmd_len] = '\0';
        cmd_len = 0;

        String line = String(cmd_buf);
        line.trim();

        // 1. COMANDO DE UPLOAD
        if (line.startsWith("FVM_UPLOAD ")) {
          int firstSpace = line.indexOf(' ');
          int secondSpace = line.indexOf(' ', firstSpace + 1);

          if (firstSpace > 0 && secondSpace > 0) {
            String filename = line.substring(firstSpace + 1, secondSpace);
            String sizeStr = line.substring(secondSpace + 1);
            upload_bytes_remaining = sizeStr.toInt();

            if (!filename.startsWith("/")) filename = "/" + filename;

            upload_file = LittleFS.open(filename, "w");
            if (upload_file) {
              is_uploading = true;
              last_rx_time = millis();
              Serial.println("OK");  // Libera o Python para mandar os bytes
            } else {
              Serial.println("ERR_FS");
            }
          }
        }
        // 2. COMANDO DE DELETAR
        else if (line.startsWith("FVM_DELETE ")) {
          String filename = line.substring(11);
          filename.trim();

          if (!filename.startsWith("/")) filename = "/" + filename;

          // 1º Passo: Procura se o app está carregado no Kernel
          App *app_to_delete = nullptr;
          for (int i = 0; i < system_get_app_count(); i++) {
            App *a = system_get_app_by_index(i);
            if (a && String(a->name) == filename) {
              app_to_delete = a;
              break;
            }
          }

          if (app_to_delete) {
            // Se achou, o unregister faz o serviço completo:
            // mata a task, tira do array, apaga o arquivo do FS e libera a memória (FVM)
            if (system_unregister_app(app_to_delete)) {
              Serial.println("DONE_DELETE");
            } else {
              Serial.println("ERR_DELETE");
            }
          } else {
            // Se o app não estava na RAM (estranho, mas possível), apaga só do FS
            if (LittleFS.exists(filename)) {
              if (LittleFS.remove(filename)) {
                Serial.println("DONE_DELETE");
              } else {
                Serial.println("ERR_DELETE");
              }
            } else {
              Serial.println("ERR_NOT_FOUND");
            }
          }
        }

        break;  // Sai do while para processar o comando
      }
      // Acumula os caracteres do comando
      else {
        if (cmd_len < sizeof(cmd_buf) - 1) {
          cmd_buf[cmd_len++] = c;
        }
      }
    }
  } else {
    // ── MODO DADOS: Recebendo bytecode binário ──
    if (Serial.available() > 0) {
      last_rx_time = millis();

      while (Serial.available() > 0 && upload_bytes_remaining > 0) {
        uint8_t buf[64];
        int avail = Serial.available();
        int to_read = avail > (int)sizeof(buf) ? sizeof(buf) : avail;

        if (to_read > (int)upload_bytes_remaining) {
          to_read = upload_bytes_remaining;
        }

        int bytes_read = Serial.readBytes(buf, to_read);
        if (bytes_read > 0) {
          upload_file.write(buf, bytes_read);
          upload_bytes_remaining -= bytes_read;
          yield();  // Evita travamento durante gravações pesadas na flash
        }
      }

      // Fim do Upload
      if (upload_bytes_remaining == 0) {
        upload_file.flush();
        upload_file.close();
        is_uploading = false;

        // Limpa a sujeira de input antes de voltar ao SO
        event_clear();
        button_reset();

        Serial.println("DONE");

        // Descobre o novo app e injeta no Launcher
        system_discover_fs_apps();
      }
    } else {
      // ── TIMEOUT: Python travou ou cabo soltou ──
      if (millis() - last_rx_time > 2000) {
        upload_file.close();
        is_uploading = false;
        Serial.println("ERR_TIMEOUT");
      }
    }
  }
}

bool system_register_capability(const char *cap, CapabilityHandler handler, Device *dev) {
  return capability_register(cap, handler, dev);
}

RequestResult system_request(const char *cap, const RequestPayload *payload) {
  return capability_request_native(cap, payload);
}

RequestResult system_request_fvm(const char *cap, const RequestPayload *payload, CallerContext *caller) {
  return capability_request(cap, payload, caller);
}

static void task_events(void *ctx) {
  Event e;
  // O Kernel consome da fila global aqui...
  while (event_pop(&e)) {
    if (millis() < input_block_until)
      continue;

    if (system_state == SYS_LOCK) {
      if (e.has_payload && e.payload.value == GESTURE_LONG_PRESS) {
        system_state = SYS_RUNNING;
        system_set_app(&launcher_app);
      }
      continue;
    }

    if (system_state == SYS_RUNNING) {
      if (e.type == EVENT_BUTTON_PRESS) {

        // ...e guarda o valor na Mailbox para o App FetScript!
        s_app_event_mailbox = e.payload.value;

        if (current_app && current_app->on_event)
          current_app->on_event(&e);
      } else {
        // Eventos de sistema (não-botão) continuam o fluxo normal
        for (int i = 0; i < registered_app_count; i++) {
          App *a = registered_apps[i];
          if (a->_task_id >= 0 && a->on_event)
            a->on_event(&e);
        }
      }
    }

    for (int i = 0; i < device_count; i++) {
      if (devices[i].on_event)
        devices[i].on_event(&devices[i], &e);
    }
  }
}

static void task_render(void *ctx) {
  if (is_system_mutating)
    return;
  for (int i = 0; i < device_count; i++) {
    if (devices[i].render)
      devices[i].render(&devices[i]);
  }
}

static void task_debug_serial(void *ctx) {
  Serial.println("=== SYSTEM ===");
  Serial.print("Focus: ");
  Serial.println(current_app ? current_app->name : "none");
  for (int i = 0; i < registered_app_count; i++) {
    App *a = registered_apps[i];
    Serial.print("  app:");
    Serial.print(a->name);
    Serial.print(" task:");
    Serial.println(a->_task_id);
  }
  Serial.println("==============");
}

void system_init() {
  if (!LittleFS.begin(true))
    Serial.println("[System] error mounting LittleFS");

  hardware_profile_load();

  event_init();
  scheduler_init();
  capability_init();
  system_register_capability("system:exit", handle_sys_exit, nullptr);
  system_register_capability("system:get_event", handle_get_event, nullptr);
  system_register_capability("system:millis", handle_millis, nullptr);

  Device button = { .id = 1, .pin = 4, .init = button_init, .poll = button_poll, .render = NULL, .on_event = NULL };
  Device oled = { .id = 2, .pin = 0, .init = oled_init, .poll = NULL, .render = oled_render, .on_event = oled_on_event };
  Device buzzer = { .id = 3, .pin = 15, .init = buzzer_init, .poll = NULL, .render = buzzer_render, .on_event = NULL };
  Device rgb = { .id = 4, .pin = 16, .init = rgb_init, .poll = NULL, .render = NULL, .on_event = NULL };
  Device button_buffer = { .id = 5, .pin = 0, .init = buffer_device_init, .poll = NULL, .render = NULL, .on_event = NULL };
  Device net = { .id = 6, .pin = 0, .init = net_device_init, .poll = NULL, .render = NULL, .on_event = NULL };
  Device text_buffer = { .id = 7, .pin = 0, .type = 4, .init = textbuf_device_init, .poll = NULL, .render = NULL, .on_event = NULL };
  Device keyboard = { .id = 8, .init = keyboard_device_init, .poll = NULL, .render = NULL, .on_event = NULL };
  Device ui_device = { .id = 10, .init = ui_device_init };

  register_device(button);
  register_device(oled);
  register_device(buzzer);
  register_device(rgb);
  register_device(button_buffer);
  register_device(net);
  register_device(text_buffer);
  register_device(keyboard);
  register_device(ui_device);

  for (int i = 0; i < device_count; i++) {
    if (devices[i].init)
      devices[i].init(&devices[i]);
  }

  persistence_init();
  int saved_brightness = persistence_read_int("scr_brt", 255);

  DisplayDriver *drv = display_hal_get_primary(DISPLAY_DEFAULT_ID);
  if (drv && drv->set_contrast) {
    drv->set_contrast(drv, (uint8_t)saved_brightness);
  }

  scheduler_add(task_uart_keyboard, nullptr, 20, TASK_PRIORITY_NORMAL, "uart_kbd");
  scheduler_add([](void *) {
    fetlink_tick();
  },
  nullptr, 100, TASK_PRIORITY_LOW, "fetlink");
  scheduler_add(task_events, nullptr, 10, TASK_PRIORITY_HIGH, "events");
  scheduler_add(task_render, nullptr, 33, TASK_PRIORITY_NORMAL, "render");
  scheduler_add(task_serial_uploader, nullptr, 50, TASK_PRIORITY_NORMAL, "uploader");

  // scheduler_add(task_debug_serial, nullptr, 2000, TASK_PRIORITY_LOW, "dbg_serial");

  app_launcher_setup();
  app_debug_setup();
  app_pomodoro_setup();
  app_clock_setup();
  app_settings_setup();
  app_buzzer_setup();
  app_manager_setup();
  app_airdrop_setup();

  system_register_app(&app_debug);
  system_register_app(&app_pomodoro);
  system_register_app(&app_clock);
  system_register_app(&app_settings);
  system_register_app(&app_buzzer);
  system_register_app(&app_manager);
  system_register_app(&app_airdrop);

  system_discover_fs_apps();
  system_launch_app(&launcher_app);
  system_focus_app(&launcher_app);
}

void system_loop() {
  scheduler_run();
}

void register_device(Device dev) {
  devices[device_count++] = dev;
}

Device *system_get_device_by_id(uint8_t id) {
  for (int i = 0; i < device_count; i++) {
    if (devices[i].id == id)
      return &devices[i];
  }
  return nullptr;
}

static bool app_is_fvm(App *app) {
  if (!app)
    return false;
  return (app->update != nullptr && app->update_ctx != nullptr);
}

void system_launch_app(App *app) {
  if (!app || app->_task_id >= 0)
    return;

  if (app->update && app->update_interval_ms > 0) {
    app->_task_id = scheduler_add(app->update, app->update_ctx,
      app->update_interval_ms,
      TASK_PRIORITY_NORMAL, app->name);
  } else {
    app->_task_id = 0;
  }

  if (app->on_enter)
    app->on_enter();
}

void system_focus_app(App *app) {
  if (!app)
    return;

  input_block_until = millis() + 250;
  event_clear();
  button_reset();

  if (app_is_fvm(app)) {
    oled_set_owner(OLED_OWNER_FVM);
  } else {
    oled_set_owner(OLED_OWNER_KERNEL);
  }

  current_app = app;
}

void system_kill_app(App *app) {
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

void system_set_app(App *app) {
  if (!app)
    return;

  for (int i = 0; i < system_get_app_count(); i++) {
    if (system_get_app_by_index(i) == app) {
      persistence_write_int("last_app", i);
      break;
    }
  }

  if (app->_task_id < 0)
    system_launch_app(app);
  system_focus_app(app);
}

void system_render() {
  Device *oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;
  OledState *oled = (OledState *)oled_dev->state;

  oled_clear(oled);

  if (oled->owner == OLED_OWNER_KERNEL) {
    oled_clear(oled);

    switch (system_state) {
    case SYS_LOCK:
      ui_center_text(oled, 0, "v0.3", 1);
      hal_text(DISPLAY_DEFAULT_ID, 15, 20, "FetOS", 3);
      ui_center_text(oled, 55, "Hold to unlock", 1);
      break;
    case SYS_RUNNING:
      if (current_app && current_app->render)
        current_app->render();
      break;
    }

    oled_flush(oled);
  }
}

bool system_register_app(App *app) {
  if (registered_app_count >= MAX_APPS)
    return false;
  app->_task_id = -1;
  registered_apps[registered_app_count++] = app;
  return true;
}

int system_get_app_count() {
  return registered_app_count;
}
App *system_get_app_by_index(int index) {
  if (index < 0 || index >= registered_app_count)
    return nullptr;
  return registered_apps[index];
}

void system_discover_fs_apps() {
  is_system_mutating = true;
  Serial.println("[System] Buscando apps no FS...");
  File root = LittleFS.open("/");
  if (!root) {
    is_system_mutating = false;
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".fvm")) {
      String fullPath = fileName.startsWith("/") ? fileName : "/" + fileName;

      bool already_registered = false;
      for (int i = 0; i < registered_app_count; i++) {
        if (String(registered_apps[i]->name) == fullPath) {
          already_registered = true;
          break;
        }
      }

      if (!already_registered) {
        Serial.printf("[System] App detectado: %s\n", fullPath.c_str());
        App *new_app = fvm_app_load_fs(fullPath.c_str());
        if (new_app)
          system_register_app(new_app);
      }
    }
    file = root.openNextFile();
  }
  is_system_mutating = false;
}

bool system_unregister_app(App *app) {
  if (!app || app == &launcher_app)
    return false;

  if (current_app == app) {
    system_focus_app(&launcher_app);
  }

  is_system_mutating = true;

  system_kill_app(app);

  bool is_fvm = (app->update != nullptr && app->update_ctx != nullptr);
  if (is_fvm && app->name && app->name[0] == '/') {
    LittleFS.remove(app->name);
  }

  for (int i = 0; i < registered_app_count; i++) {
    if (registered_apps[i] == app) {
      for (int j = i; j < registered_app_count - 1; j++) {
        registered_apps[j] = registered_apps[j + 1];
      }
      registered_apps[--registered_app_count] = nullptr;
      break;
    }
  }

  if (is_fvm)
    fvm_app_destroy(app);

  is_system_mutating = false;
  return true;
}

void task_uart_keyboard(void *ctx) {
  static uint32_t last_key_time = 0;
  if (!is_uploading && system_state == SYS_RUNNING && current_app && app_is_fvm(current_app) && Serial.available() > 0) {
    char c = Serial.read();

    // Debounce simples: ignora se for muito rápido (opcional)
    // if (millis() - last_key_time < 50) return;
    // last_key_time = millis();

    if (c == '\r') {
      keyboard_inject_key(KEY_ENTER);
    } else if (c == '\n') {
      // Ignora o \n para não duplicar o Enter do Windows
      return;
    } else if (c == 8 || c == 127) {
      keyboard_inject_key(KEY_BACKSPACE);
    } else if (c >= 32 && c <= 126) {
      keyboard_inject(c);
    }
  }
}