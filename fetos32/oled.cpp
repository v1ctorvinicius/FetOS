#include "oled.h"
#include "display_hal.h"
#include "system.h"
#include "capability.h"
#include "fvm_app.h"
#include "driver_ssd1306.h"

static bool can_draw(CallerContext *caller);

static RequestResult handle_number(Device *dev, const RequestPayload *payload, CallerContext *ctx)
{

  if (!can_draw(ctx))
    return REQ_IGNORED;

  const RequestParam *pX = payload_get(payload, "x");
  const RequestParam *pY = payload_get(payload, "y");
  const RequestParam *pV = payload_get(payload, "v");

  if (!pV)
    return REQ_IGNORED;

  char buf[16];
  itoa(pV->int_value, buf, 10);

  hal_text(DISPLAY_DEFAULT_ID,
           pX ? (int16_t)pX->int_value : 0,
           pY ? (int16_t)pY->int_value : 0,
           buf,
           2);

  return REQ_ACCEPTED;
}

void oled_set_owner(OledOwner owner)
{
  Device *dev = system_get_device_by_id(2);
  if (!dev || !dev->state)
    return;
  OledState *st = (OledState *)dev->state;
  st->owner = owner;
  st->frame_ready = false;
  hal_clear(DISPLAY_DEFAULT_ID);
}

static RequestResult handle_text(Device *dev, const RequestPayload *payload,
                                 CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  const RequestParam *text = payload_get(payload, "text");
  const RequestParam *x = payload_get(payload, "x");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *size = payload_get(payload, "size");
  if (!text || !text->str_value)
    return REQ_IGNORED;
  hal_text(DISPLAY_DEFAULT_ID,
           x ? (int16_t)x->int_value : 0,
           y ? (int16_t)y->int_value : 0,
           text->str_value,
           size ? (uint8_t)size->int_value : 1);
  return REQ_ACCEPTED;
}

static RequestResult handle_text_center(Device *dev, const RequestPayload *payload,
                                        CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  const RequestParam *text = payload_get(payload, "text");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *size = payload_get(payload, "size");
  if (!text || !text->str_value)
    return REQ_IGNORED;
  hal_text_center(DISPLAY_DEFAULT_ID,
                  y ? (int16_t)y->int_value : 0,
                  text->str_value,
                  size ? (uint8_t)size->int_value : 1);
  return REQ_ACCEPTED;
}

static RequestResult handle_text_invert(Device *dev, const RequestPayload *payload,
                                        CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  const RequestParam *text = payload_get(payload, "text");
  const RequestParam *x = payload_get(payload, "x");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *size = payload_get(payload, "size");
  if (!text || !text->str_value)
    return REQ_IGNORED;
  hal_text_invert(DISPLAY_DEFAULT_ID,
                  x ? (int16_t)x->int_value : 0,
                  y ? (int16_t)y->int_value : 0,
                  text->str_value,
                  size ? (uint8_t)size->int_value : 1);
  return REQ_ACCEPTED;
}

static RequestResult handle_clear(Device *dev, const RequestPayload *payload,
                                  CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  OledState *st = (OledState *)dev->state;
  hal_clear(DISPLAY_DEFAULT_ID);
  st->frame_ready = false;
  return REQ_ACCEPTED;
}

static RequestResult handle_flush(Device *dev, const RequestPayload *payload,
                                  CallerContext *caller)
{
  OledState *st = (OledState *)dev->state;
  if (!st)
    return REQ_NO_DEVICE;
  if (caller && caller->type == CALLER_FVM)
  {
    if (!can_draw(caller))
      return REQ_IGNORED;
  }
  hal_flush(DISPLAY_DEFAULT_ID);
  return REQ_ACCEPTED;
}

static RequestResult handle_rect(Device *dev, const RequestPayload *payload,
                                 CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
  const RequestParam *x = payload_get(payload, "x");
  const RequestParam *y = payload_get(payload, "y");
  const RequestParam *w = payload_get(payload, "w");
  const RequestParam *h = payload_get(payload, "h");
  const RequestParam *fill = payload_get(payload, "fill");
  hal_rect(DISPLAY_DEFAULT_ID,
           x ? (int16_t)x->int_value : 0,
           y ? (int16_t)y->int_value : 0,
           w ? (int16_t)w->int_value : 10,
           h ? (int16_t)h->int_value : 10,
           fill ? (fill->int_value != 0) : true);
  return REQ_ACCEPTED;
}

static RequestResult handle_list_scroll(Device *dev, const RequestPayload *payload,
                                        CallerContext *caller)
{
  if (!can_draw(caller))
    return REQ_IGNORED;
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
  hal_list_scroll(DISPLAY_DEFAULT_ID,
                  item_ptrs, count,
                  selected ? (int)selected->int_value : 0,
                  start_y ? (int)start_y->int_value : 0,
                  visible ? (int)visible->int_value : 0);
  return REQ_ACCEPTED;
}

void oled_init(Device *dev)
{
  OledState *st = (OledState *)malloc(sizeof(OledState));
  if (!st)
    return;
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
  system_register_capability("display:number", handle_number, dev);
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

void oled_clear(OledState *st)
{
  hal_clear(DISPLAY_DEFAULT_ID);
}
void oled_flush(OledState *st)
{
  hal_flush(DISPLAY_DEFAULT_ID);
}

void ui_text(OledState *st, int x, int y, const char *text, int size)
{
  hal_text(DISPLAY_DEFAULT_ID, x, y, text, size);
}
void ui_text_invert(OledState *st, int x, int y, const char *text, int size)
{
  hal_text_invert(DISPLAY_DEFAULT_ID, x, y, text, size);
}
void ui_center_text(OledState *st, int y, const char *text, int size)
{
  hal_text_center(DISPLAY_DEFAULT_ID, y, text, size);
}
void ui_list_scroll(OledState *st, const char **items, int count,
                    int selected, int start_y, int visible)
{
  hal_list_scroll(DISPLAY_DEFAULT_ID, items, count, selected, start_y, visible);
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
    return (caller->type != CALLER_FVM);

  if (caller->type == CALLER_FVM)
  {
    if (!current_app || !current_app->update_ctx)
      return false;
    FvmAppContext *fctx = (FvmAppContext *)current_app->update_ctx;
    return (caller->fvm_proc == (void *)fctx->proc);
  }
  return true;
}
