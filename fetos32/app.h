#pragma once
#include "event.h"

typedef struct App
{
  const char *name;

  void (*on_enter)();
  void (*on_exit)();
  void (*on_event)(Event *);

  void (*render)();

  void (*update)(void *ctx);
  uint32_t update_interval_ms;
  void *update_ctx;

  int _task_id;
} App;