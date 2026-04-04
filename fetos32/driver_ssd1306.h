#pragma once
#include "display_hal.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


typedef struct {
  Adafruit_SSD1306* display;
} Ssd1306State;


DisplayDriver* driver_ssd1306_create(uint8_t i2c_addr, TwoWire* wire);