#include "event.h"
#include "device.h"
#include "button.h"
#include "oled.h"
#include "system.h"

#define DEVICE_COUNT 2

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  system_init();
}

void loop() {
  system_loop();
}