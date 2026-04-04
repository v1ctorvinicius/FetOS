#pragma once
#include <Arduino.h>
#include "device.h"
#include "capability.h"
#include "display_hal.h"

typedef enum
{
  OLED_OWNER_KERNEL,
  OLED_OWNER_FVM
} OledOwner;

typedef struct
{
  bool dirty;
  OledOwner owner;
  bool frame_ready;
} OledState;

void oled_init(Device *dev);
void oled_render(Device *dev);
void oled_on_event(Device *dev, Event *e);
void oled_set_owner(OledOwner owner);

// ── primitivas legadas (compatibilidade com apps nativos) ─────
// Delegam ao HAL com DISPLAY_DEFAULT_ID (broadcast automático)
void oled_clear(OledState *st);
void oled_flush(OledState *st);
void ui_text(OledState *st, int x, int y, const char *text, int size);
void ui_text_invert(OledState *st, int x, int y, const char *text, int size);
void ui_center_text(OledState *st, int y, const char *text, int size);
void ui_list_scroll(OledState *st, const char **items, int count,
                    int selected, int start_y, int visible = 0);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define UI_HEADER_H 16
#define UI_BODY_Y 18