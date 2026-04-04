#pragma once
#include <Arduino.h>

// ── geometria ─────────────────────────────────────────────────

typedef struct {
  uint16_t width;
  uint16_t height;
} DisplayGeometry;

// ── cores canônicas ───────────────────────────────────────────
#define DISPLAY_COLOR_OFF    0
#define DISPLAY_COLOR_ON     1
#define DISPLAY_COLOR_INVERT 2

// ── vtable do driver ──────────────────────────────────────────

typedef struct DisplayDriver DisplayDriver;

struct DisplayDriver {
  const char*     name;
  uint8_t         device_id;   // ID lógico — múltiplos drivers podem ter o mesmo ID (broadcast)
  DisplayGeometry geometry;

  void     (*init)        (DisplayDriver* drv);
  void     (*clear)       (DisplayDriver* drv);
  void     (*flush)       (DisplayDriver* drv);
  void     (*draw_pixel)  (DisplayDriver* drv, int16_t x, int16_t y, uint8_t color);
  void     (*draw_rect)   (DisplayDriver* drv, int16_t x, int16_t y,
                           int16_t w, int16_t h, uint8_t color, bool filled);
  void     (*draw_line)   (DisplayDriver* drv, int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1, uint8_t color);
  void     (*print_str)   (DisplayDriver* drv, int16_t x, int16_t y,
                           const char* str, uint8_t size, uint8_t color);
  void     (*print_char)  (DisplayDriver* drv, int16_t x, int16_t y,
                           char c, uint8_t size, uint8_t color);
  uint16_t (*text_width)  (DisplayDriver* drv, const char* str, uint8_t size);
  void     (*set_contrast)(DisplayDriver* drv, uint8_t value);

  void* state;
};

// ── registry multi-driver ─────────────────────────────────────
// Múltiplos drivers podem ter o mesmo device_id — isso habilita broadcast.
// Ex: OLED (id=2) + LCD (id=2) → hal_clear(2) limpa os dois.

#define DISPLAY_HAL_MAX_DRIVERS 4

void display_hal_register(DisplayDriver* drv);

// Primeiro driver com o device_id dado — para leitura de geometria
DisplayDriver* display_hal_get_primary(uint8_t device_id);

// Preenche out[] com todos os drivers do device_id — retorna quantos encontrou
int display_hal_get_all(uint8_t device_id, DisplayDriver** out, int max_out);

// ── ID padrão do display principal ───────────────────────────
#define DISPLAY_DEFAULT_ID 2

// ── helpers broadcast (operam em TODOS os drivers do device_id) ──────────

void hal_clear        (uint8_t device_id);
void hal_flush        (uint8_t device_id);
void hal_text         (uint8_t device_id, int16_t x, int16_t y, const char* str, uint8_t size);
void hal_text_invert  (uint8_t device_id, int16_t x, int16_t y, const char* str, uint8_t size);
void hal_text_center  (uint8_t device_id, int16_t y, const char* str, uint8_t size);
void hal_rect         (uint8_t device_id, int16_t x, int16_t y, int16_t w, int16_t h, bool filled);
void hal_set_contrast (uint8_t device_id, uint8_t value);
void hal_list_scroll  (uint8_t device_id, const char** items, int count,
                       int selected, int start_y, int visible);

// ── helpers por driver direto (operações num driver específico) ───────────

void hal_text_on        (DisplayDriver* drv, int16_t x, int16_t y, const char* str, uint8_t size);
void hal_text_invert_on (DisplayDriver* drv, int16_t x, int16_t y, const char* str, uint8_t size);
void hal_text_center_on (DisplayDriver* drv, int16_t y, const char* str, uint8_t size);
void hal_rect_on        (DisplayDriver* drv, int16_t x, int16_t y, int16_t w, int16_t h, bool filled);