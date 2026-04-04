#include "display_hal.h"
#include <string.h>

static DisplayDriver *s_drivers[DISPLAY_HAL_MAX_DRIVERS];
static int s_count = 0;

void display_hal_register(DisplayDriver *drv)
{
  if (!drv || s_count >= DISPLAY_HAL_MAX_DRIVERS)
    return;
  s_drivers[s_count++] = drv;
  if (drv->init)
    drv->init(drv);
  Serial.printf("[DisplayHAL] Registrado: %s (id=%d, %dx%d)\n",
                drv->name, drv->device_id,
                drv->geometry.width, drv->geometry.height);
}

DisplayDriver *display_hal_get_primary(uint8_t device_id)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id)
      return s_drivers[i];
  }
  return nullptr;
}

int display_hal_get_all(uint8_t device_id, DisplayDriver **out, int max_out)
{
  int found = 0;
  for (int i = 0; i < s_count && found < max_out; i++)
  {
    if (s_drivers[i]->device_id == device_id)
    {
      out[found++] = s_drivers[i];
    }
  }
  return found;
}

void hal_clear(uint8_t device_id)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id && s_drivers[i]->clear)
    {
      s_drivers[i]->clear(s_drivers[i]);
    }
  }
}

void hal_flush(uint8_t device_id)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id && s_drivers[i]->flush)
    {
      s_drivers[i]->flush(s_drivers[i]);
    }
  }
}

void hal_set_contrast(uint8_t device_id, uint8_t value)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id && s_drivers[i]->set_contrast)
    {
      s_drivers[i]->set_contrast(s_drivers[i], value);
    }
  }
}

void hal_text(uint8_t device_id, int16_t x, int16_t y,
              const char *str, uint8_t size)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id)
    {
      hal_text_on(s_drivers[i], x, y, str, size);
    }
  }
}

void hal_text_invert(uint8_t device_id, int16_t x, int16_t y,
                     const char *str, uint8_t size)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id)
    {
      hal_text_invert_on(s_drivers[i], x, y, str, size);
    }
  }
}

void hal_text_center(uint8_t device_id, int16_t y,
                     const char *str, uint8_t size)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id)
    {
      hal_text_center_on(s_drivers[i], y, str, size);
    }
  }
}

void hal_rect(uint8_t device_id, int16_t x, int16_t y,
              int16_t w, int16_t h, bool filled)
{
  for (int i = 0; i < s_count; i++)
  {
    if (s_drivers[i]->device_id == device_id)
    {
      hal_rect_on(s_drivers[i], x, y, w, h, filled);
    }
  }
}

void hal_list_scroll(uint8_t device_id, const char **items, int count,
                     int selected, int start_y, int visible)
{
  for (int d = 0; d < s_count; d++)
  {
    DisplayDriver *drv = s_drivers[d];
    if (drv->device_id != device_id)
      continue;

    int item_h = 10;
    int vis = visible > 0 ? visible : (drv->geometry.height - start_y) / item_h;

    int start = selected - (vis / 2);
    if (start < 0)
      start = 0;
    if (count > vis && start > count - vis)
      start = count - vis;

    int y = start_y;
    for (int i = 0; i < vis; i++)
    {
      int idx = start + i;
      if (idx >= count)
        break;
      if (idx == selected)
        hal_text_invert_on(drv, 0, y, items[idx], 1);
      else
        hal_text_on(drv, 0, y, items[idx], 1);
      y += item_h;
    }
  }
}

void hal_text_on(DisplayDriver *drv, int16_t x, int16_t y,
                 const char *str, uint8_t size)
{
  if (!drv || !drv->print_str)
    return;
  drv->print_str(drv, x, y, str, size, DISPLAY_COLOR_ON);
}

void hal_text_invert_on(DisplayDriver *drv, int16_t x, int16_t y,
                        const char *str, uint8_t size)
{
  if (!drv || !drv->print_str)
    return;
  drv->print_str(drv, x, y, str, size, DISPLAY_COLOR_INVERT);
}

void hal_text_center_on(DisplayDriver *drv, int16_t y,
                        const char *str, uint8_t size)
{
  if (!drv || !str)
    return;

  uint16_t w;
  if (drv->text_width)
  {
    w = drv->text_width(drv, str, size);
  }
  else
  {
    w = (uint16_t)(strlen(str) * 6 * size);
  }

  int16_t x = (int16_t)((drv->geometry.width > w)
                            ? (drv->geometry.width - w) / 2
                            : 0);
  hal_text_on(drv, x, y, str, size);
}

void hal_rect_on(DisplayDriver *drv, int16_t x, int16_t y,
                 int16_t w, int16_t h, bool filled)
{
  if (!drv || !drv->draw_rect)
    return;
  drv->draw_rect(drv, x, y, w, h, DISPLAY_COLOR_ON, filled);
}