#include "app_debug.h"
#include "system.h"
#include "display_hal.h"
#include <Arduino.h>
#include "button_gesture.h"
#include "event.h"
#include "scheduler.h"
#include "capability.h"
#include "driver/temp_sensor.h"

App app_debug;

enum DebugPage
{
  PAGE_HEALTH,
  PAGE_TASKS,
  PAGE_EVENTS,
  PAGE_INPUT,
  PAGE_FVM,
  PAGE_COUNT
};

static DebugPage current_page = PAGE_HEALTH;
static uint32_t total_events = 0;
static const char *last_gesture = "NONE";

#define EVENT_LOG_SIZE 5
static const char *event_log[EVENT_LOG_SIZE];
static int event_log_idx = 0;

#define RAM_HISTORY_SIZE 20
static uint16_t ram_history[RAM_HISTORY_SIZE];
static int ram_idx = 0;

static uint32_t last_frame = 0;
static uint32_t frame_time = 0;
static int task_scroll = 0;
static bool task_detail_mode = false;
static bool input_locked = false;

static void draw_sparkline(DisplayDriver *drv, int x, int y, int w, int h, uint16_t *data, int size, int max_val)
{
  if (max_val == 0)
    max_val = 1;
  for (int i = 0; i < size; i++)
  {
    int bar_h = (data[i] * h) / max_val;
    if (bar_h > h)
      bar_h = h;
    hal_rect_on(drv, x + (i * 2), y + (h - bar_h), 1, bar_h, true);
  }
}

static void render_hal_list(DisplayDriver *drv, const char **items, int count, int scroll, int start_y, int max_vis)
{
  int y = start_y;
  for (int i = 0; i < max_vis; i++)
  {
    int idx = scroll + i;
    if (idx >= count)
      break;
    if (i == 0 && !task_detail_mode)
    {
      hal_text_invert_on(drv, 0, y, items[idx], 1);
    }
    else
    {
      hal_text_on(drv, 0, y, items[idx], 1);
    }
    y += 10;
  }
}

void app_debug_setup()
{
  app_debug.name = "SysMonitor+";
  app_debug.on_event = app_debug_on_event;
  app_debug.render = app_debug_render;
  app_debug.on_enter = app_debug_on_enter;
  app_debug.on_exit = app_debug_on_exit;
  app_debug.update = nullptr;
  app_debug.update_interval_ms = 0;
  app_debug._task_id = -1;
}

void app_debug_on_enter()
{
  current_page = PAGE_HEALTH;
  task_scroll = 0;
  task_detail_mode = false;
  input_locked = false;
  for (int i = 0; i < EVENT_LOG_SIZE; i++)
    event_log[i] = "-";
  for (int i = 0; i < RAM_HISTORY_SIZE; i++)
    ram_history[i] = 0;
}

void app_debug_on_exit() {}

void app_debug_on_event(Event *e)
{
  if (!e || !e->has_payload)
    return;
  int gesture = e->payload.value;
  total_events++;

  switch (gesture)
  {
  case GESTURE_TAP:
    last_gesture = "TAP";
    break;
  case GESTURE_DOUBLE_TAP:
    last_gesture = "DBL";
    break;
  case GESTURE_TRIPLE_TAP:
    last_gesture = "TPL";
    break;
  case GESTURE_LONG_PRESS:
    last_gesture = "HOLD";
    break;
  }

  event_log[event_log_idx] = last_gesture;
  event_log_idx = (event_log_idx + 1) % EVENT_LOG_SIZE;

  bool nav_blocked = task_detail_mode || input_locked;

  if (!nav_blocked)
  {
    if (gesture == GESTURE_DOUBLE_TAP)
    {
      system_set_app(&launcher_app);
      return;
    }
    if (gesture == GESTURE_TRIPLE_TAP)
    {
      current_page = (DebugPage)((current_page + 1) % PAGE_COUNT);
      task_scroll = 0;
      return;
    }
  }

  if (current_page == PAGE_TASKS)
  {
    if (gesture == GESTURE_LONG_PRESS)
      task_detail_mode = !task_detail_mode;
    else if (gesture == GESTURE_TAP && !task_detail_mode)
    {
      int active = 0;
      for (int i = 0; i < MAX_TASKS; i++)
      {
        const Task *t = scheduler_get_task(i);
        if (t && t->active)
          active++;
      }
      if (active > 0)
        task_scroll = (task_scroll + 1) % active;
    }
  }
  else if (current_page == PAGE_INPUT && gesture == GESTURE_LONG_PRESS)
  {
    input_locked = !input_locked;
  }
}

void app_debug_render()
{

  static uint32_t last_render_time = 0;
  static uint32_t last_data_update = 0;
  static float cached_temp = 0.0f;
  static uint32_t cached_free_kb = 0;

  uint32_t now = millis();

  if (now - last_render_time < 33)
    return;
  frame_time = now - last_render_time;
  last_render_time = now;

  if (now - last_data_update > 3000)
  {
    cached_temp = temperatureRead();
    cached_free_kb = ESP.getFreeHeap() / 1024;

    if (current_page == PAGE_HEALTH)
    {
      ram_history[ram_idx] = (uint16_t)cached_free_kb;
      ram_idx = (ram_idx + 1) % RAM_HISTORY_SIZE;
    }
    last_data_update = now;
  }

  DisplayDriver *drv = display_hal_get_primary(DISPLAY_DEFAULT_ID);
  if (!drv)
    return;
  if (drv->clear)
    drv->clear(drv);

  const char *pg_names[] = {"HEALTH", "TASKS", "EVENTS", "INPUT", "FVM"};
  hal_text_on(drv, 0, 0, pg_names[current_page], 1);

  for (int i = 0; i < PAGE_COUNT; i++)
  {
    int dx = 90 + (i * 6);
    if (i == (int)current_page)
      hal_rect_on(drv, dx, 2, 4, 4, true);
    else
      hal_rect_on(drv, dx, 2, 4, 4, false);
  }

  hal_rect_on(drv, 0, 12, 128, 1, true);

  int body_y = 16;
  char buf[32];

  if (current_page == PAGE_HEALTH)
  {
    hal_text_on(drv, 0, body_y, "RAM:", 1);
    draw_sparkline(drv, 0, body_y + 10, 40, 12, ram_history, RAM_HISTORY_SIZE, 300);

    snprintf(buf, 32, "Uptime:%lus", now / 1000);
    hal_text_on(drv, 65, body_y, buf, 1);

    snprintf(buf, 32, "FPS:%d", frame_time > 0 ? 1000 / frame_time : 0);
    hal_text_on(drv, 65, body_y + 20, buf, 1);

    snprintf(buf, 32, "Temp:%.1fC", cached_temp);
    hal_text_on(drv, 65, body_y + 30, buf, 1);

    snprintf(buf, 32, "Free:%luKb", cached_free_kb);
    hal_text_on(drv, 0, body_y + 28, buf, 1);
  }

  else if (current_page == PAGE_TASKS)
  {
    int active_indices[MAX_TASKS];
    const char *list[MAX_TASKS];
    int count = 0;

    for (int i = 0; i < MAX_TASKS; i++)
    {
      const Task *t = scheduler_get_task(i);
      if (t && t->active)
      {
        active_indices[count] = i;
        list[count++] = t->name;
      }
    }

    if (task_detail_mode)
    {
      const Task *t = scheduler_get_task(active_indices[task_scroll]);

      const char *p_name = (t->priority == TASK_PRIORITY_HIGH) ? "HIGH" : (t->priority == TASK_PRIORITY_NORMAL) ? "NORM"
                                                                                                                : "LOW";
      snprintf(buf, 32, "ID:%d %s", active_indices[task_scroll], p_name);
      hal_text_on(drv, 0, body_y, buf, 1);

      hal_text_on(drv, 0, body_y + 10, t->name, 1);
      snprintf(buf, 32, "Dur: %lums", t->last_duration_ms);
      hal_text_on(drv, 0, body_y + 20, buf, 1);
      snprintf(buf, 32, "Ovr: %lu", t->overruns);
      hal_text_on(drv, 70, body_y + 20, buf, 1);

      uint32_t base_int = (t->interval > 0) ? t->interval : 1;
      int load_w = (t->last_duration_ms * 90) / base_int;
      if (load_w > 90)
        load_w = 90;
      hal_text_on(drv, 0, body_y + 30, "Load:", 1);
      hal_rect_on(drv, 35, body_y + 31, 92, 5, false);
      hal_rect_on(drv, 35, body_y + 31, load_w, 5, true);
    }
    else
    {
      render_hal_list(drv, list, count, task_scroll, body_y, 3);
    }
  }

  else if (current_page == PAGE_EVENTS)
  {
    hal_text_on(drv, 0, body_y, "Event Log:", 1);
    for (int i = 0; i < 4; i++)
    {
      int idx = (event_log_idx + i) % EVENT_LOG_SIZE;
      hal_text_on(drv, 5, (body_y + 10) + (i * 8), event_log[idx], 1);
    }
    snprintf(buf, 16, "Total: %lu", total_events);
    hal_text_on(drv, 70, body_y + 10, buf, 1);
  }

  else if (current_page == PAGE_INPUT)
  {
    Device *btn = system_get_device_by_id(1);
    if (btn && btn->state)
    {
      ButtonGestureState *st = (ButtonGestureState *)btn->state;
      if (input_locked)
        hal_text_invert_on(drv, 0, body_y, " LOCKED ", 1);
      else
        hal_text_on(drv, 0, body_y, "Monitor:", 1);

      snprintf(buf, 32, "Hold: %lums", st->down_ms);
      hal_text_on(drv, 0, body_y + 12, buf, 1);

      int hp = (st->down_ms * 128) / 600;
      if (hp > 128)
        hp = 128;
      hal_rect_on(drv, 0, body_y + 25, 128, 4, false);
      hal_rect_on(drv, 0, body_y + 25, hp, 4, true);
    }
  }

  else if (current_page == PAGE_FVM)
  {
    RequestParam params[] = {{"pc", 0, 0}, {"sp", 0, 0}, {"csp", 0, 0}, {"err", 0, 0}, {"halted", 0, 0}};
    RequestPayload payload = {params, 5};

    if (system_request("fvm:stats", &payload) == REQ_ACCEPTED)
    {
      int pc = (int)payload_get(&payload, "pc")->int_value;
      int sp = (int)payload_get(&payload, "sp")->int_value;
      int csp = (int)payload_get(&payload, "csp")->int_value;
      int err = (int)payload_get(&payload, "err")->int_value;
      int halted = (int)payload_get(&payload, "halted")->int_value;

      snprintf(buf, 32, "PC: %04X", pc);
      hal_text_on(drv, 0, body_y, buf, 1);

      if (err > 0)
      {
        hal_text_invert_on(drv, 60, body_y, " ERROR ", 1);
        snprintf(buf, 32, "Code: %d", err);
        hal_text_on(drv, 60, body_y + 10, buf, 1);
      }
      else if (halted)
        hal_text_on(drv, 60, body_y, "[HALTED]", 1);
      else
        hal_text_on(drv, 60, body_y, "[RUNNING]", 1);

      hal_text_on(drv, 0, body_y + 12, "Stack:", 1);
      hal_rect_on(drv, 45, body_y + 13, 40, 6, false);
      hal_rect_on(drv, 45, body_y + 13, (sp * 40) / 16, 6, true);

      hal_text_on(drv, 0, body_y + 22, "C-Stack:", 1);
      hal_rect_on(drv, 45, body_y + 23, 40, 6, false);
      hal_rect_on(drv, 45, body_y + 23, (csp * 40) / 8, 6, true);
    }
    else
    {
      hal_text_center_on(drv, body_y + 10, "FVM NOT ACTIVE", 1);
    }
  }

  if (!input_locked)
  {
    hal_rect_on(drv, 0, 55, 128, 1, true);
    hal_text_on(drv, 0, 57, task_detail_mode ? "Hold: back" : "1:list 3:page H:action", 1);
  }

  if (drv->flush)
    drv->flush(drv);
}
