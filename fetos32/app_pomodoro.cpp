#include "app_pomodoro.h"
#include "system.h"
#include "oled.h"
#include "button_gesture.h"
#include "Arduino.h"
#include "persistence.h"
#include "scheduler.h"

static void save_slot(uint8_t i);
static void reset_session_runtime();
static void flush_accumulated_time();
static void load_slots();
static void task_pomo_flush(void* ctx);

App app_pomodoro;

#define FOCUS_TIME (25 * 60 * 1000)
#define SHORT_BREAK_TIME (5 * 60 * 1000)
#define LONG_BREAK_TIME (15 * 60 * 1000)

enum PomoState {
  POMO_IDLE,
  POMO_FOCUS,
  POMO_PAUSED,
  POMO_BREAK_SHORT,
  POMO_BREAK_LONG,
  POMO_ALARM
};

PomoSlot slots[MAX_SLOTS];
uint8_t current_slot = 0;

static PomoState current_state = POMO_IDLE;
static PomoState next_state_after_alarm = POMO_IDLE;
static PomoState paused_state = POMO_IDLE;

static bool timer_running = false;
static uint32_t session_accumulated_ms = 0;
static uint32_t time_left_ms = 0;
static uint32_t last_tick = 0;
static uint8_t pomo_cycle = 0;
static bool reset_armed = false;

static int flush_task_id = -1;

// ── persistência ─────────────────────────────────────────────

static void reset_session_runtime() {
  time_left_ms = FOCUS_TIME;
  session_accumulated_ms = 0;
  last_tick = 0;
  timer_running = false;
}

static void flush_accumulated_time() {
  if (session_accumulated_ms == 0) return;

  PomoSlot* s = &slots[current_slot];
  s->total_seconds += session_accumulated_ms / 1000;
  session_accumulated_ms = 0;

  save_slot(current_slot);
}

static void task_pomo_flush(void* ctx) {
  if (current_state == POMO_FOCUS && timer_running) {
    flush_accumulated_time();
  }
}

static void load_slots() {
  char key[16];
  for (int i = 0; i < MAX_SLOTS; i++) {
    snprintf(key, sizeof(key), "pomo_s_%d", i);
    slots[i].total_sessions = persistence_read_int(key, 0);

    snprintf(key, sizeof(key), "pomo_sec_%d", i);
    slots[i].total_seconds = persistence_read_int(key, 0);
  }
}

static void save_slot(uint8_t i) {
  char key[16];
  snprintf(key, sizeof(key), "pomo_s_%d", i);
  persistence_write_int(key, slots[i].total_sessions);

  snprintf(key, sizeof(key), "pomo_sec_%d", i);
  persistence_write_int(key, slots[i].total_seconds);
}

// ── alarme ───────────────────────────────────────────────────

static void trigger_alarm(PomoState next) {
  current_state = POMO_ALARM;
  next_state_after_alarm = next;
  timer_running = false;

  // não chama buzzer diretamente — pede o recurso ao sistema
  system_request("audio:beep_beep", nullptr);
}

// ── lifecycle ─────────────────────────────────────────────────

void app_pomodoro_setup() {
  app_pomodoro.name = "Pomodoro";
  app_pomodoro.on_event = app_pomodoro_on_event;
  app_pomodoro.render = app_pomodoro_render;
  app_pomodoro.on_enter = app_pomodoro_on_enter;
  app_pomodoro.on_exit = app_pomodoro_on_exit;
  app_pomodoro.update = nullptr;
  app_pomodoro.update_interval_ms = 0;
  app_pomodoro.update_ctx = nullptr;
  app_pomodoro._task_id = -1;
  pinMode(2, OUTPUT);
}

void app_pomodoro_on_enter() {
  current_state = POMO_IDLE;
  pomo_cycle = 0;
  reset_armed = false;

  load_slots();
  reset_session_runtime();

  flush_task_id = scheduler_add(task_pomo_flush, nullptr, 60000, TASK_PRIORITY_LOW, "pomo_flush");
}

void app_pomodoro_on_exit() {
  flush_accumulated_time();

  if (flush_task_id >= 0) {
    scheduler_remove(flush_task_id);
    flush_task_id = -1;
  }

  timer_running = false;
  current_state = POMO_IDLE;
  digitalWrite(2, LOW);
}

// ── eventos ───────────────────────────────────────────────────

void app_pomodoro_on_event(Event* e) {
  if (!e || !e->has_payload) return;

  int gesture = e->payload.value;

  if (gesture == GESTURE_DOUBLE_TAP) {
    if (current_state == POMO_IDLE) {
      system_set_app(&launcher_app);
    } else {
      current_state = POMO_IDLE;
      timer_running = false;
      reset_armed = false;
      reset_session_runtime();
    }
    return;
  }

  if (current_state == POMO_ALARM) {
    if (gesture == GESTURE_LONG_PRESS) {
      current_state = next_state_after_alarm;

      if (current_state == POMO_BREAK_SHORT) time_left_ms = SHORT_BREAK_TIME;
      else if (current_state == POMO_BREAK_LONG) time_left_ms = LONG_BREAK_TIME;
      else time_left_ms = FOCUS_TIME;

      last_tick = millis();
      timer_running = true;
    }
    return;
  }

  if (gesture == GESTURE_TAP) {
    if (current_state == POMO_IDLE && !reset_armed) {
      current_slot = (current_slot + 1) % MAX_SLOTS;
      load_slots();
    }

    if (current_state != POMO_IDLE) {
      reset_armed = true;
    }
    return;
  }

  if (gesture == GESTURE_LONG_PRESS) {
    if (reset_armed) {
      PomoSlot* s = &slots[current_slot];
      s->total_seconds = 0;
      s->total_sessions = 0;
      save_slot(current_slot);
      reset_armed = false;
      return;
    }

    if (current_state == POMO_IDLE) {
      current_state = POMO_FOCUS;
      last_tick = millis();
      timer_running = true;
      return;
    }

    if (current_state == POMO_PAUSED) {
      current_state = paused_state;
      last_tick = millis();
      timer_running = true;
      return;
    }

    paused_state = current_state;
    current_state = POMO_PAUSED;
    timer_running = false;
  }
}

// ── render ────────────────────────────────────────────────────

void app_pomodoro_render() {
  Device* oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state) return;

  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  if (timer_running) {
    uint32_t now = millis();
    uint32_t delta = now - last_tick;
    last_tick = now;

    if (current_state == POMO_FOCUS) {
      session_accumulated_ms += delta;
    }

    if (time_left_ms > delta) {
      time_left_ms -= delta;
    } else {
      time_left_ms = 0;

      if (current_state == POMO_FOCUS) {
        PomoSlot* s = &slots[current_slot];
        s->total_sessions++;
        flush_accumulated_time();
        pomo_cycle++;

        if (pomo_cycle == 4) {
          trigger_alarm(POMO_BREAK_LONG);
          pomo_cycle = 0;
        } else {
          trigger_alarm(POMO_BREAK_SHORT);
        }
      } else {
        trigger_alarm(POMO_FOCUS);
      }
    }
  }

  if (current_state == POMO_ALARM) {
    bool blink = (millis() / 250) % 2;
    digitalWrite(2, blink);

    if (blink) ui_center_text(oled, 20, "TIME'S UP!", 2);

    ui_center_text(oled, 50, "Hold to continue", 1);
    oled_flush(oled);
    return;
  } else {
    digitalWrite(2, LOW);
  }

  uint32_t total_sec = time_left_ms / 1000;
  uint32_t m = total_sec / 60;
  uint32_t s = total_sec % 60;

  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02lu:%02lu", (unsigned long)m, (unsigned long)s);

  if (current_state == POMO_IDLE) {
    char slot_str[16];
    snprintf(slot_str, sizeof(slot_str), "SLOT %d", current_slot + 1);

    char stats[32];
    uint32_t sec = slots[current_slot].total_seconds;
    uint32_t ss = sec % 60;
    uint32_t mm = (sec / 60) % 60;
    uint32_t hh = (sec / 3600) % 24;
    uint32_t dd = sec / 86400;

    if (dd > 0) snprintf(stats, sizeof(stats), "%lud %02lu:%02lu:%02lu", dd, hh, mm, ss);
    else if (hh > 0) snprintf(stats, sizeof(stats), "%lu:%02lu:%02lu", hh, mm, ss);
    else snprintf(stats, sizeof(stats), "%lu:%02lu", mm, ss);

    ui_center_text(oled, 0, slot_str, 1);
    ui_center_text(oled, 20, "READY", 1);
    ui_center_text(oled, 50, stats, 1);

    if (reset_armed) ui_center_text(oled, 35, "Hold to RESET", 1);

  } else {
    if (current_state == POMO_FOCUS) ui_center_text(oled, 0, "FOCUS", 1);
    else if (current_state == POMO_BREAK_SHORT) ui_center_text(oled, 0, "SHORT BREAK", 1);
    else if (current_state == POMO_BREAK_LONG) ui_center_text(oled, 0, "LONG BREAK", 1);
    else if (current_state == POMO_PAUSED) ui_center_text(oled, 0, "PAUSED", 1);

    ui_center_text(oled, 20, time_str, 3);

    char cycle_str[32];
    snprintf(cycle_str, sizeof(cycle_str), "%s%s%s%s",
             (pomo_cycle > 0) ? "(x) " : "( ) ",
             (pomo_cycle > 1) ? "(x) " : "( ) ",
             (pomo_cycle > 2) ? "(x) " : "( ) ",
             (pomo_cycle > 3) ? "(x) " : "( ) ");

    ui_center_text(oled, 50, cycle_str, 1);
  }

  oled_flush(oled);
}