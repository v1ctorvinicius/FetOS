#include "event.h"
#include "device.h"
#include "button.h"
#include "oled.h"
#include "system.h"

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  system_init();
}

void loop() {
  system_loop();
}