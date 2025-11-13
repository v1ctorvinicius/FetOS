#include "hal.h"
#include <stdio.h>
#include <stdbool.h>

void hal_init(void) {
  printf("HAL init (simulado)\n");
}

void hal_tick(void) {}

void hal_led_set(uint8_t id, bool on) {
  printf("LED[%d]=%s\n", id, on ? "ON" : "OFF");
}

void hal_oled_draw(uint8_t* buffer) {
  // envia pro FetHub no formato reconhecido
  printf("LCD:PRINT,%s\n", buffer);
}

uint16_t hal_adc_read(uint8_t ch) {
  return ch * 42; // dummy
}
