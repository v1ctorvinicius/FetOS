#include "system.h"
#include "oled.h"
#include "capability.h"
#include "Arduino.h"
#include "fvm_app.h"

static bool can_draw(CallerContext *caller);

static OledState *get_oled_state(Device *dev)
{
  if (!dev || !dev->state)
    return nullptr;
  return (OledState *)dev->state;
}

void oled_set_owner(OledOwner owner)
{
  Device *dev = system_get_device_by_id(2);
  if (!dev || !dev->state)
    return;

  OledState *st = (OledState *)dev->state;
  st->owner = owner;
  st->frame_ready = false;

  st->display->clearDisplay();
}

static RequestResult handle_text(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  (void)caller;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  const RequestParam *text = payload_get(payload, "text");
  const RequestParam *x = payload_get(payload, "x");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *size = payload_get(payload, "size");

  if (!text || !text->str_value)
    return REQ_IGNORED;

  ui_text(oled,
          x ? (int)x->int_value : 0,
          y ? (int)y->int_value : 0,
          text->str_value,
          size ? (int)size->int_value : 1);

  return REQ_ACCEPTED;
}

static RequestResult handle_text_center(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  (void)caller;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  const RequestParam *text = payload_get(payload, "text");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *size = payload_get(payload, "size");

  if (!text || !text->str_value)
    return REQ_IGNORED;

  ui_center_text(oled,
                 y ? (int)y->int_value : 0,
                 text->str_value,
                 size ? (int)size->int_value : 1);

  return REQ_ACCEPTED;
}

static RequestResult handle_text_invert(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  (void)caller;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  const RequestParam *text = payload_get(payload, "text");
  const RequestParam *x = payload_get(payload, "x");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *size = payload_get(payload, "size");

  if (!text || !text->str_value)
    return REQ_IGNORED;

  ui_text_invert(oled,
                 x ? (int)x->int_value : 0,
                 y ? (int)y->int_value : 0,
                 text->str_value,
                 size ? (int)size->int_value : 1);

  return REQ_ACCEPTED;
}

static RequestResult handle_clear(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;
  oled_clear(oled);
  oled->frame_ready = false;
  return REQ_ACCEPTED;
}

static RequestResult handle_flush(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  if (caller && caller->type == CALLER_FVM)
  {
    if (!can_draw(caller))
      return REQ_IGNORED;

    oled->display->display();
    oled->frame_ready = false;
  }
  else
  {
    oled_flush(oled);
  }

  return REQ_ACCEPTED;
}

static RequestResult handle_rect(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  const RequestParam *x = payload_get(payload, "x");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *w = payload_get(payload, "w");
  const RequestParam *h = payload_get(payload, "h");
  const RequestParam *fill = payload_get(payload, "fill");

  int _x = x ? x->int_value : 0;
  int _y = y ? y->int_value : 0;
  int _w = w ? w->int_value : 10;
  int _h = h ? h->int_value : 10;
  bool _fill = fill ? (fill->int_value != 0) : true;

  if (_fill)
    oled->display->fillRect(_x, _y, _w, _h, SSD1306_WHITE);
  else
    oled->display->drawRect(_x, _y, _w, _h, SSD1306_WHITE);

  return REQ_ACCEPTED;
}

static RequestResult handle_list_scroll(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  const RequestParam *selected = payload_get(payload, "selected");
  const RequestParam *start_y = payload_get(payload, "start_y");
  const RequestParam *visible = payload_get(payload, "visible");
  const RequestParam *items_p = payload_get(payload, "items");

  if (!items_p || !items_p->str_value)
    return REQ_IGNORED;

  static const char *item_ptrs[32];
  static char item_buf[512];

  strncpy(item_buf, items_p->str_value, sizeof(item_buf) - 1);
  item_buf[sizeof(item_buf) - 1] = '\0';

  int count = 0;
  char *cursor = item_buf;
  item_ptrs[count++] = cursor;

  while (*cursor && count < 32)
  {
    if (*cursor == '\n')
    {
      *cursor = '\0';
      if (*(cursor + 1))
        item_ptrs[count++] = cursor + 1;
    }
    cursor++;
  }

  ui_list_scroll(oled,
                 item_ptrs, count,
                 selected ? (int)selected->int_value : 0,
                 start_y ? (int)start_y->int_value : 0,
                 visible ? (int)visible->int_value : 0);

  return REQ_ACCEPTED;
}

static RequestResult handle_text_scroll(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  OledState *oled = get_oled_state(dev);
  if (!oled)
    return REQ_NO_DEVICE;

  const RequestParam *scroll = payload_get(payload, "scroll");
  const RequestParam *start_y = payload_get(payload, "start_y");
  const RequestParam *visible = payload_get(payload, "visible");
  const RequestParam *items_p = payload_get(payload, "items");

  if (!items_p || !items_p->str_value)
    return REQ_IGNORED;

  static const char *item_ptrs[32];
  static char item_buf[512];

  strncpy(item_buf, items_p->str_value, sizeof(item_buf) - 1);
  item_buf[sizeof(item_buf) - 1] = '\0';

  int count = 0;
  char *cursor = item_buf;
  item_ptrs[count++] = cursor;

  while (*cursor && count < 32)
  {
    if (*cursor == '\n')
    {
      *cursor = '\0';
      if (*(cursor + 1))
        item_ptrs[count++] = cursor + 1;
    }
    cursor++;
  }

  ui_text_scroll(oled,
                 item_ptrs, count,
                 scroll ? (int)scroll->int_value : 0,
                 start_y ? (int)start_y->int_value : 0,
                 visible ? (int)visible->int_value : 0);

  return REQ_ACCEPTED;
}

void oled_init(Device *dev)
{
  OledState *st = (OledState *)malloc(sizeof(OledState));
  if (!st)
    return;

  st->display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
  st->display->begin(SSD1306_SWITCHCAPVCC, 0x3C);
  st->dirty = false;
  st->owner = OLED_OWNER_KERNEL;
  st->frame_ready = false;

  dev->state = st;

  system_register_capability("display:text", handle_text, dev);
  system_register_capability("display:text_center", handle_text_center, dev);
  system_register_capability("display:text_invert", handle_text_invert, dev);
  system_register_capability("display:clear", handle_clear, dev);
  system_register_capability("display:flush", handle_flush, dev);
  system_register_capability("display:rect", handle_rect, dev);
  system_register_capability("display:list_scroll", handle_list_scroll, dev);
  system_register_capability("display:text_scroll", handle_text_scroll, dev);
}

void oled_render(Device *dev)
{
  if (!dev || !dev->state)
    return;
  OledState *st = (OledState *)dev->state;

  if (st->owner == OLED_OWNER_FVM)
  {

    bool fvm_dead = false;
    if (!current_app || !current_app->update_ctx)
    {
      fvm_dead = true;
    }
    else
    {
      FvmAppContext *fctx = (FvmAppContext *)current_app->update_ctx;
      if (!fctx->proc || fctx->proc->error)
        fvm_dead = true;
    }

    if (fvm_dead)
    {
      st->owner = OLED_OWNER_KERNEL;
      st->frame_ready = false;
    }
    else
    {
      return;
    }
  }

  system_render();
}

void oled_on_event(Device *dev, Event *e) {}

void oled_show_boot_screen(OledState *st)
{
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

void oled_clear(OledState *st)
{
  if (!st || !st->display)
    return;
  st->display->clearDisplay();
}

void oled_flush(OledState *st)
{
  if (!st || !st->display)
    return;
  st->display->display();
}

void ui_text(OledState *st, int x, int y, const char *text, int size)
{
  st->display->setTextSize(size);
  st->display->setTextColor(SSD1306_WHITE);
  st->display->setCursor(x, y);
  st->display->print(text);
}

void ui_text_invert(OledState *st, int x, int y, const char *text, int size)
{
  st->display->setTextSize(size);
  st->display->setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  st->display->setCursor(x, y);
  st->display->print(text);
  st->display->setTextColor(SSD1306_WHITE);
}

void ui_center_text(OledState *st, int y, const char *text, int size)
{
  int16_t x1, y1;
  uint16_t w, h;
  st->display->setTextSize(size);
  st->display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  ui_text(st, x, y, text, size);
}

void ui_list_scroll(OledState *st, const char **items, int count, int selected, int start_y, int visible)
{
  int item_height = 10;
  if (visible <= 0)
    visible = (SCREEN_HEIGHT - start_y) / item_height;

  if (count <= visible)
  {
    int y = start_y;
    for (int i = 0; i < count; i++)
    {
      if (i == selected)
        ui_text_invert(st, 0, y, items[i], 1);
      else
        ui_text(st, 0, y, items[i], 1);
      y += item_height;
    }
    return;
  }

  int start = selected - (visible / 2);
  if (start < 0)
    start = 0;
  if (start > count - visible)
    start = count - visible;

  int y = start_y;
  for (int i = 0; i < visible; i++)
  {
    int idx = start + i;
    if (idx >= count)
      break;
    if (idx == selected)
      ui_text_invert(st, 0, y, items[idx], 1);
    else
      ui_text(st, 0, y, items[idx], 1);
    y += item_height;
  }
}

void ui_text_scroll(OledState *st, const char **items, int count, int scroll_offset, int start_y, int visible)
{
  int item_height = 10;
  if (visible <= 0)
    visible = (SCREEN_HEIGHT - start_y) / item_height;

  int start_idx = scroll_offset;
  if (start_idx > count - visible)
    start_idx = count - visible;
  if (start_idx < 0)
    start_idx = 0;

  int y = start_y;
  for (int i = 0; i < visible; i++)
  {
    int idx = start_idx + i;
    if (idx >= count)
      break;
    ui_text(st, 0, y, items[idx], 1);
    y += item_height;
  }

  if (count > visible)
  {
    int bar_h = (visible * visible * item_height) / count;
    if (bar_h < 3)
      bar_h = 3;
    int max_scroll = count - visible;
    int bar_y = start_y + ((start_idx * (visible * item_height - bar_h)) / max_scroll);
    st->display->fillRect(126, bar_y, 2, bar_h, SSD1306_WHITE);
  }
}

static bool can_draw(CallerContext *caller)
{

  if (!caller)
    return true;

  Device *dev = system_get_device_by_id(2);
  if (!dev || !dev->state)
    return false;
  OledState *st = (OledState *)dev->state;

  if (st->owner == OLED_OWNER_KERNEL)
  {
    return (caller->type != CALLER_FVM);
  }

  if (caller->type == CALLER_FVM)
  {
    if (!current_app || !current_app->update_ctx)
      return false;

    FvmAppContext *fctx = (FvmAppContext *)current_app->update_ctx;

    return (caller->fvm_proc == (void *)fctx->proc);
  }

  return true;
}