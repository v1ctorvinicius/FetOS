#include "app_keyboard.h"
#include "system.h"
#include "oled.h"
#include "display_hal.h"
#include "keyboard_device.h"
#include "button_gesture.h"
#include "Arduino.h"

App app_keyboard;

// ── render ────────────────────────────────────────────────────

static void app_keyboard_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  KeyboardState* kbd = keyboard_get_state();

  // Header
  hal_text_center(DISPLAY_DEFAULT_ID, 4, "Keyboard", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 15, 128, 1, true);

  if (!kbd->connected) {
    // ── Desconectado ──────────────────────────────────────
    hal_text_center(DISPLAY_DEFAULT_ID, 26, "Aguardando BLE...", 1);

    // Animação de loading: três pontos piscantes
    uint32_t tick = (millis() / 400) % 4;
    char dots[5] = "    ";
    for (int i = 0; i < (int)tick; i++) dots[i] = '.';
    hal_text_center(DISPLAY_DEFAULT_ID, 38, dots, 1);

  } else {
    // ── Conectado ─────────────────────────────────────────
    hal_text_center(DISPLAY_DEFAULT_ID, 20, "Conectado", 1);

    // Buffer size
    char buf[24];
    KeyboardState* k = keyboard_get_state();
    uint8_t sz = (k->tail >= k->head)
                  ? (k->tail - k->head)
                  : (KBD_BUFFER_SIZE - k->head + k->tail);
    snprintf(buf, sizeof(buf), "Buffer: %d", sz);
    hal_text_center(DISPLAY_DEFAULT_ID, 32, buf, 1);

    // Caps lock
    if (kbd->caps_lock) {
      hal_text_center(DISPLAY_DEFAULT_ID, 44, "CAPS LOCK", 1);
    }
  }

  // Footer
  hal_rect(DISPLAY_DEFAULT_ID, 0, 54, 128, 1, true);
  hal_text_center(DISPLAY_DEFAULT_ID, 56, "2x: Sair", 1);

  oled_flush(oled);
}

// ── on_event ──────────────────────────────────────────────────

static void app_keyboard_on_event(Event* e) {
  if (!e || !e->has_payload) return;
  if (e->payload.value == GESTURE_DOUBLE_TAP) {
    system_set_app(&launcher_app);
  }
}

// ── on_enter / on_exit ────────────────────────────────────────

static void app_keyboard_on_enter() {
  // nada — o device BLE já está rodando via kbd_poll_task
}

static void app_keyboard_on_exit() {
  // nada — mantém BLE ativo em background
}

// ── setup ─────────────────────────────────────────────────────

void app_keyboard_setup() {
  app_keyboard.name               = "Keyboard";
  app_keyboard.render             = app_keyboard_render;
  app_keyboard.on_event           = app_keyboard_on_event;
  app_keyboard.on_enter           = app_keyboard_on_enter;
  app_keyboard.on_exit            = app_keyboard_on_exit;
  app_keyboard.update             = nullptr;   // sem update — usa kbd_poll_task no scheduler
  app_keyboard.update_interval_ms = 0;
  app_keyboard.update_ctx         = nullptr;
  app_keyboard._task_id           = -1;
}