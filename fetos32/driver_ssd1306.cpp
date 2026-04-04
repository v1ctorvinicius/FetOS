#include "driver_ssd1306.h"
#include <stdlib.h>

static void ssd_init(DisplayDriver *d)
{
  Ssd1306State *s = (Ssd1306State *)d->state;
  s->display->begin(SSD1306_SWITCHCAPVCC, 0x3C);
  s->display->clearDisplay();
  s->display->setTextColor(SSD1306_WHITE);
  s->display->setTextWrap(false);
}
static void ssd_clear(DisplayDriver *d)
{
  ((Ssd1306State *)d->state)->display->clearDisplay();
}
static void ssd_flush(DisplayDriver *d)
{
  ((Ssd1306State *)d->state)->display->display();
}
static void ssd_draw_pixel(DisplayDriver *d, int16_t x, int16_t y, uint8_t c)
{
  ((Ssd1306State *)d->state)->display->drawPixel(x, y, c ? SSD1306_WHITE : SSD1306_BLACK);
}
static void ssd_draw_rect(DisplayDriver *d, int16_t x, int16_t y,
                          int16_t w, int16_t h, uint8_t c, bool filled)
{
  auto *disp = ((Ssd1306State *)d->state)->display;
  uint16_t col = c ? SSD1306_WHITE : SSD1306_BLACK;
  if (filled)
    disp->fillRect(x, y, w, h, col);
  else
    disp->drawRect(x, y, w, h, col);
}
static void ssd_draw_line(DisplayDriver *d, int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1, uint8_t c)
{
  ((Ssd1306State *)d->state)->display->drawLine(x0, y0, x1, y1, c ? SSD1306_WHITE : SSD1306_BLACK);
}
static void ssd_print_str(DisplayDriver *d, int16_t x, int16_t y,
                          const char *str, uint8_t size, uint8_t c)
{

  if (!str || str[0] == '\0')
    return;

  auto *disp = ((Ssd1306State *)d->state)->display;

  disp->setTextSize(size);
  disp->setCursor(x, y);

  if (c == DISPLAY_COLOR_INVERT)
  {
    disp->setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  }

  disp->print(str);

  if (c == DISPLAY_COLOR_INVERT)
  {
    disp->setTextColor(SSD1306_WHITE);
  }
}
static void ssd_print_char(DisplayDriver *d, int16_t x, int16_t y,
                           char c, uint8_t size, uint8_t color)
{
  char buf[2] = {c, '\0'};
  ssd_print_str(d, x, y, buf, size, color);
}
static uint16_t ssd_text_width(DisplayDriver *d, const char *str, uint8_t size)
{
  auto *disp = ((Ssd1306State *)d->state)->display;
  int16_t x1, y1;
  uint16_t w, h;
  disp->setTextSize(size);
  disp->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}
static void ssd_set_contrast(DisplayDriver *d, uint8_t v)
{
  auto *disp = ((Ssd1306State *)d->state)->display;
  disp->ssd1306_command(SSD1306_SETCONTRAST);
  disp->ssd1306_command(v);
}

DisplayDriver *driver_ssd1306_create(uint8_t i2c_addr, TwoWire *wire)
{
  DisplayDriver *drv = (DisplayDriver *)malloc(sizeof(DisplayDriver));
  Ssd1306State *st = (Ssd1306State *)malloc(sizeof(Ssd1306State));
  if (!drv || !st)
  {
    free(drv);
    free(st);
    return nullptr;
  }

  st->display = new Adafruit_SSD1306(128, 64, wire, -1);

  drv->name = "SSD1306";
  drv->device_id = DISPLAY_DEFAULT_ID;
  drv->geometry = {128, 64};
  drv->init = ssd_init;
  drv->clear = ssd_clear;
  drv->flush = ssd_flush;
  drv->draw_pixel = ssd_draw_pixel;
  drv->draw_rect = ssd_draw_rect;
  drv->draw_line = ssd_draw_line;
  drv->print_str = ssd_print_str;
  drv->print_char = ssd_print_char;
  drv->text_width = ssd_text_width;
  drv->set_contrast = ssd_set_contrast;
  drv->state = st;
  return drv;
}