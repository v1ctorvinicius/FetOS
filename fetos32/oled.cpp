// oled.cpp
#include "oled.h"
#include "event.h"

void oled_init(Device* dev) {
  OledState* st = (OledState*)malloc(sizeof(OledState));
  if (!st) return;

  st->display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
  st->display->begin(SSD1306_SWITCHCAPVCC, 0x3C);
  st->display->clearDisplay();

  st->display->setTextSize(1);
  st->display->setTextColor(SSD1306_WHITE);
  st->display->setCursor(0, 0);
  st->display->setTextSize(2);
  st->display->printf("FetOS");
  st->display->setTextSize(1);
  st->display->printf("32");
  st->display->setTextSize(2);
  st->display->printf("v0.1");
  st->display->display();

  st->boot_screen = true;
  st->dirty = false;

  dev->state = st;
}

void oled_render(Device* dev) {
  if (!dev || !dev->state) return;

  OledState* st = (OledState*)dev->state;

  if (st->boot_screen) return;

  if (!st->dirty) return;

  st->display->clearDisplay();
  st->display->setTextSize(1);
  st->display->setTextColor(SSD1306_WHITE);
  st->display->setCursor(0, 0);
  st->display->printf("%s", st->text);
  st->display->display();

  st->dirty = false;
}

void oled_on_event(Device* dev, Event* e) {
  if (!dev || !dev->state || !e) return;

  OledState* st = (OledState*)dev->state;

  if (e->type == EVENT_BUTTON_PRESS) {
    st->boot_screen = false;

    strcpy(st->text, "BUTTON PRESS!");
    st->dirty = true;
  }
}