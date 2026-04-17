#pragma once
#include "Wire.h"
#include "launcher.h"
#include "device.h"
#include "app.h"
#include "capability.h"
#include "fvm_process.h"
#include "fvm_app.h"

#define MAX_APPS 10

enum SystemState
{
  SYS_BOOT,
  SYS_LOCK,
  SYS_RUNNING
};

void system_init();
void system_loop();
void register_device(Device dev);

extern Device devices[];
extern int device_count;
extern App *current_app;
Device *system_get_device_by_id(uint8_t id);

extern App launcher_app;
extern App app_debug;

extern SystemState system_state;
void system_render();

extern bool is_system_mutating;

bool system_register_app(App *app);
bool system_unregister_app(App* app);
int system_get_app_count();
App *system_get_app_by_index(int index);

void system_launch_app(App *app);
void system_focus_app(App *app);
void system_kill_app(App *app);
void system_set_app(App *app);

bool system_register_capability(const char *cap, CapabilityHandler handler, Device *dev);

RequestResult system_request(const char *cap, const RequestPayload *payload);

RequestResult system_request_fvm(const char *cap, const RequestPayload *payload, CallerContext *caller);
void system_discover_fs_apps();