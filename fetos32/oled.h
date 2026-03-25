//oled.h
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "device.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define UI_HEADER_H 16
#define UI_BODY_Y 18

typedef struct {
  Adafruit_SSD1306* display;
  char text[64];
  bool dirty;
} OledState;

void oled_init(Device* dev);
void oled_render(Device* dev);
void oled_on_event(Device* dev, Event* e);
void oled_show_boot_screen(OledState* st);
void oled_clear(OledState* st);
void oled_draw_text(OledState* st, int x, int y, const char* text, int size);
void oled_flush(OledState* st);

void ui_text(OledState* st, int x, int y, const char* text, int size);
void ui_text_invert(OledState* st, int x, int y, const char* text, int size);
void ui_center_text(OledState* st, int y, const char* text, int size);
void ui_list(OledState* st, const char** items, int count, int selected);
void ui_list_scroll(OledState* st, const char** items, int count, int selected, int start_y);