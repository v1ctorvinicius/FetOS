//oled.h
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "device.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

typedef struct {
  Adafruit_SSD1306* display;
  char text[64];
  bool dirty;
  bool boot_screen;
} OledState;

void oled_init(Device* dev);
void oled_render(Device* dev);
void oled_on_event(Device* dev, Event* e);