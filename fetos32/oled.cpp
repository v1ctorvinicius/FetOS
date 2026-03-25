#include "system.h"
#include "oled.h"
#include "button_gesture.h"
#include "Arduino.h"

void oled_show_boot_screen(OledState* st) {
  st->display->clearDisplay();

  st->display->setTextColor(SSD1306_WHITE);
  st->display->setTextSize(1);
  st->display->setCursor(100, 0);
  st->display->println("v0.1");

  st->display->setCursor(20, 17);
  st->display->setTextSize(3);
  st->display->printf("FetOS");

  st->display->setTextSize(1);
  st->display->printf("32");

  st->display->setCursor(5, 50);
  st->display->setTextSize(1);
  st->display->println("Long press to unlock");

  st->display->display();
}

void oled_init(Device* dev) {
  OledState* st = (OledState*)malloc(sizeof(OledState));
  if (!st) return;

  st->display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
  st->display->begin(SSD1306_SWITCHCAPVCC, 0x3C);

  st->dirty = false;

  oled_show_boot_screen(st);

  dev->state = st;
}

void oled_render(Device* dev) {
  if (!dev || !dev->state) return;

  system_render();
}

void oled_on_event(Device* dev, Event* e) {}

void oled_clear(OledState* st) {
  if (!st || !st->display) return;
  st->display->clearDisplay();
}

void oled_flush(OledState* st) {
  if (!st || !st->display) return;
  st->display->display();
}

void ui_text(OledState* st, int x, int y, const char* text, int size) {
  st->display->setTextSize(size);
  st->display->setTextColor(SSD1306_WHITE);
  st->display->setCursor(x, y);
  st->display->print(text);
}

void ui_text_invert(OledState* st, int x, int y, const char* text, int size) {
  st->display->setTextSize(size);
  st->display->setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  st->display->setCursor(x, y);
  st->display->print(text);
  st->display->setTextColor(SSD1306_WHITE);
}

void ui_center_text(OledState* st, int y, const char* text, int size) {
  int16_t x1, y1;
  uint16_t w, h;

  st->display->setTextSize(size);
  st->display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - w) / 2;

  ui_text(st, x, y, text, size);
}

void ui_list_scroll(OledState* st, const char** items, int count, int selected, int start_y) {
  int visible = 4;
  int item_height = 10;

  if (count <= visible) {
    int y = start_y;
    for (int i = 0; i < count; i++) {
      if (i == selected) {
        ui_text_invert(st, 0, y, items[i], 1);
      } else {
        ui_text(st, 0, y, items[i], 1);
      }
      y += item_height;
    }
    return;
  }

  int start = selected - (visible / 2);

  if (start < 0) start = 0;
  if (start > count - visible) start = count - visible;

  int y = 0;

  for (int i = 0; i < visible; i++) {
    int idx = start + i;

    if (idx >= count) break;

    if (idx == selected) {
      ui_text_invert(st, 0, y, items[idx], 1);
    } else {
      ui_text(st, 0, y, items[idx], 1);
    }

    y += item_height;
  }
}