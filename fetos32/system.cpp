#include "system.h"
#include "device.h"
#include "event.h"
#include "button.h"
#include "oled.h"

#define MAX_DEVICES 4

static Device devices[MAX_DEVICES];
static int device_count = 0;


void register_device(Device dev) {
  devices[device_count++] = dev;
}

void system_init() {
  event_init();

  Device button = {
    .id = 1,
    .pin = 4,
    .init = button_init,
    .poll = button_poll,
    .render = NULL,
    .on_event = NULL
  };

  Device oled = {
    .id = 2,
    .pin = 0,
    .init = oled_init,
    .poll = NULL,
    .render = oled_render,
    .on_event = oled_on_event
  };

  register_device(button);
  register_device(oled);

  for (int i = 0; i < device_count; i++) {
    if (devices[i].init)
      devices[i].init(&devices[i]);
  }

  Serial.println("REGISTERED DEVICES:");
  for (int i = 0; i < device_count; i++) {
    Serial.println(devices[i].id);
  }
}

void system_loop() {
  // Serial.println("POLLING DEVICES");

  // for (int i = 0; i < device_count; i++) {
  //   Serial.print("Device ");
  //   Serial.print(i);
  //   Serial.print(" poll ptr: ");
  //   Serial.println((uint32_t)devices[i].poll);
  // }

  // POLL
  for (int i = 0; i < device_count; i++) {
    if (devices[i].poll)
      devices[i].poll(&devices[i]);
  }

  Event e;
  while (event_pop(&e)) {
    for (int i = 0; i < device_count; i++) {
      if (devices[i].on_event)
        devices[i].on_event(&devices[i], &e);
    }
  }

  // RENDER
  for (int i = 0; i < device_count; i++) {
    if (devices[i].render)
      devices[i].render(&devices[i]);
  }
}