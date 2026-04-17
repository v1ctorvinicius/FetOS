#pragma once
#include <Arduino.h>
#include "event.h"

typedef struct Device Device;

typedef void (*DeviceInit)(Device*);
typedef void (*DevicePoll)(Device*);
typedef void (*DeviceRender)(Device*);
typedef void (*DeviceOnEvent)(Device*, Event*);

struct Device {
  uint8_t id;
  uint8_t pin;
  uint8_t type;

  DeviceInit init;
  DevicePoll poll;
  DeviceRender render;
  DeviceOnEvent on_event;

  void* state;
};