#pragma once
#include "Wire.h"
#include "launcher.h"
#include "device.h"
#include "app.h"

#define MAX_APPS 8

enum SystemState {
  SYS_BOOT,
  SYS_LOCK,
  SYS_RUNNING
};

void system_init();
void system_loop();
void register_device(Device dev);

extern Device devices[];
extern int device_count;
extern App* current_app;
Device* system_get_device_by_id(uint8_t id);

extern App launcher_app;
extern App app_debug;
void system_set_app(App* app);

extern SystemState system_state;
void system_render();

bool system_register_app(App* app);
int system_get_app_count();
App* system_get_app_by_index(int index);