#include "app_clock.h"
#include "system.h"
#include "oled.h"
#include "button_gesture.h"
#include "Arduino.h"

App app_clock;

enum ClockMode {
  MODE_TIME,
  MODE_STOPWATCH,
  MODE_TIMER
};

static ClockMode current_mode = MODE_TIME;

static uint32_t stopwatch_elapsed_ms = 0;
static uint32_t stopwatch_last_tick = 0;
static bool stopwatch_running = false;

static uint32_t timer_remaining_ms = 0;
static uint32_t timer_last_tick = 0;
static bool timer_running = false;
static bool timer_is_overtime = false;

static const uint32_t timer_options[] = { 60000, 180000, 300000, 600000 };
#define NUM_TIMER_OPTIONS (sizeof(timer_options) / sizeof(timer_options[0]))
static uint8_t current_timer_idx = 0;

#define ALARM_LED_PIN 2

void app_clock_setup() {
  app_clock.name = "Clock";
  app_clock.on_event = app_clock_on_event;
  app_clock.render = app_clock_render;
  app_clock.on_enter = app_clock_on_enter;
  app_clock.on_exit = app_clock_on_exit;

  pinMode(ALARM_LED_PIN, OUTPUT);
  timer_remaining_ms = timer_options[current_timer_idx];
}

void app_clock_on_enter() {
  current_mode = MODE_TIME;
  digitalWrite(ALARM_LED_PIN, LOW);
}

void app_clock_on_exit() {
  stopwatch_running = false;
  timer_running = false;
  timer_is_overtime = false;
  digitalWrite(ALARM_LED_PIN, LOW);
}

void app_clock_on_event(Event* e) {
  if (!e || !e->has_payload) return;
  int gesture = e->payload.value;

  if (gesture == GESTURE_DOUBLE_TAP) {
    system_set_app(&launcher_app);
    return;
  }

  if (gesture == GESTURE_TAP) {
    if (current_mode == MODE_TIMER && timer_is_overtime) {
      timer_running = false;
      timer_is_overtime = false;
      timer_remaining_ms = timer_options[current_timer_idx];
      digitalWrite(ALARM_LED_PIN, LOW);
      return;
    }

    if (current_mode == MODE_TIME) current_mode = MODE_STOPWATCH;
    else if (current_mode == MODE_STOPWATCH) current_mode = MODE_TIMER;
    else if (current_mode == MODE_TIMER) current_mode = MODE_TIME;
    return;
  }

  if (current_mode == MODE_STOPWATCH) {
    if (gesture == GESTURE_LONG_PRESS) {
      if (!stopwatch_running) stopwatch_last_tick = millis();
      stopwatch_running = !stopwatch_running;
    }
    if (gesture == GESTURE_TRIPLE_TAP) {
      stopwatch_running = false;
      stopwatch_elapsed_ms = 0;
    }
  } else if (current_mode == MODE_TIMER) {
    if (gesture == GESTURE_LONG_PRESS) {
      if (timer_is_overtime) {
        timer_running = false;
        timer_is_overtime = false;
        timer_remaining_ms = timer_options[current_timer_idx];
        digitalWrite(ALARM_LED_PIN, LOW);
      } else {
        if (!timer_running) {
          if (timer_remaining_ms == 0 && !timer_is_overtime) timer_remaining_ms = timer_options[current_timer_idx];
          timer_last_tick = millis();
        }
        timer_running = !timer_running;
      }
    }
    if (gesture == GESTURE_TRIPLE_TAP && !timer_running && !timer_is_overtime) {
      current_timer_idx = (current_timer_idx + 1) % NUM_TIMER_OPTIONS;
      timer_remaining_ms = timer_options[current_timer_idx];
    }
  }
}

void format_time(uint32_t ms, char* buffer, bool is_negative) {
  uint32_t total_sec = ms / 1000;
  uint32_t h = total_sec / 3600;
  uint32_t m = (total_sec % 3600) / 60;
  uint32_t s = total_sec % 60;

  if (is_negative) {
    snprintf(buffer, 16, "-%02d:%02d:%02d", h, m, s);
  } else {
    snprintf(buffer, 16, "%02d:%02d:%02d", h, m, s);
  }
}

void app_clock_render() {
  Device* oled_dev = system_get_device_by_id(2);
  OledState* oled = (OledState*)oled_dev->state;
  oled_clear(oled);

  char time_str[16];

  if (current_mode == MODE_TIME) {
    digitalWrite(ALARM_LED_PIN, LOW);
    ui_center_text(oled, 0, "UPTIME", 1);
    format_time(millis(), time_str, false);
    ui_center_text(oled, 20, time_str, 2);
    ui_center_text(oled, 50, "1 Tap: Next", 1);
  } else if (current_mode == MODE_STOPWATCH) {
    digitalWrite(ALARM_LED_PIN, LOW);
    ui_center_text(oled, 0, "STOPWATCH", 1);

    if (stopwatch_running) {
      uint32_t now = millis();
      stopwatch_elapsed_ms += (now - stopwatch_last_tick);
      stopwatch_last_tick = now;
    }

    format_time(stopwatch_elapsed_ms, time_str, false);
    ui_center_text(oled, 20, time_str, 2);
    ui_center_text(oled, 50, stopwatch_running ? "Hold: Pause" : "Hold: Start | 3x: Reset", 1);
  } else if (current_mode == MODE_TIMER) {
    uint32_t mins = timer_options[current_timer_idx] / 60000;
    char title_str[16];

    if (timer_is_overtime) {
      ui_center_text(oled, 0, "TIME'S UP!", 1);

      bool blink_state = (millis() / 250) % 2;
      digitalWrite(ALARM_LED_PIN, blink_state);

      if (timer_running) {
        uint32_t now = millis();
        timer_remaining_ms += (now - timer_last_tick);
        timer_last_tick = now;
      }

    } else {
      snprintf(title_str, 16, "TIMER (%dm)", mins);
      ui_center_text(oled, 0, title_str, 1);
      digitalWrite(ALARM_LED_PIN, LOW);

      if (timer_running) {
        uint32_t now = millis();
        uint32_t delta = now - timer_last_tick;
        timer_last_tick = now;

        if (timer_remaining_ms > delta) {
          timer_remaining_ms -= delta;
        } else {
          uint32_t excess = delta - timer_remaining_ms;
          timer_remaining_ms = excess;
          timer_is_overtime = true;
        }
      }
    }

    format_time(timer_remaining_ms, time_str, timer_is_overtime);
    ui_center_text(oled, 20, time_str, 2);

    if (timer_is_overtime) {
      ui_center_text(oled, 50, "Tap/Hold: Stop & Reset", 1);
    } else if (timer_running) {
      ui_center_text(oled, 50, "Hold: Pause", 1);
    } else {
      ui_center_text(oled, 50, "Hold: Start | 3x: Time", 1);
    }
  }

  oled_flush(oled);
}