// app.h
#pragma once
#include "event.h"

typedef struct App {
  const char* name;
  void (*on_event)(Event*);
  void (*render)();
  void (*on_enter)();
  void (*on_exit)();
} App;