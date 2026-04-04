#include "driver_lcd1602.h"
#include <LiquidCrystal.h>
#include <stdlib.h>
#include <string.h>

// ── estado ─────────────────────────────────────────────────────

typedef struct {
  LiquidCrystal* lcd;
  // buffer de texto — o LCD não tem clearDisplay() instantâneo como OLED.
  // Guardamos o conteúdo atual para só reescrever o que mudou.
  char buf[2][17];  // 2 linhas x 16 chars + null
  char pending[2][17];
  bool dirty;
} Lcd1602State;

// ── helpers de coordenada ─────────────────────────────────────
// O LCD 16x2 tem células de 5x8 pixels visíveis como caracteres.
// A HAL trabalha em pixels — convertemos para col/row aqui.
// Cada caractere ocupa ~6px wide x 8px tall no modelo de coordenadas HAL.

#define LCD_CHAR_W 8
#define LCD_CHAR_H 8
#define LCD_COLS 16
#define LCD_ROWS 2

static int px_to_col(int16_t x) {
  int col = x / LCD_CHAR_W;
  if (col >= LCD_COLS) col = LCD_COLS - 1;
  if (col < 0) col = 0;
  return col;
}

static int px_to_row(int16_t y) {
  int row = y / LCD_CHAR_H;
  if (row >= LCD_ROWS) row = LCD_ROWS - 1;
  if (row < 0) row = 0;
  return row;
}

// ── vtable ────────────────────────────────────────────────────

static void lcd_init(DisplayDriver* d) {
  Lcd1602State* s = (Lcd1602State*)d->state;
  s->lcd->begin(LCD_COLS, LCD_ROWS);
  s->lcd->clear();
  memset(s->buf, ' ', sizeof(s->buf));
  memset(s->pending, ' ', sizeof(s->pending));
  for (int r = 0; r < LCD_ROWS; r++) {
    s->buf[r][LCD_COLS] = '\0';
    s->pending[r][LCD_COLS] = '\0';
  }
  s->dirty = false;
}

static void lcd_clear(DisplayDriver* d) {
  Lcd1602State* s = (Lcd1602State*)d->state;
  memset(s->pending, ' ', sizeof(s->pending));
  for (int r = 0; r < LCD_ROWS; r++) s->pending[r][LCD_COLS] = '\0';
  s->dirty = true;
}

static void lcd_flush(DisplayDriver* d) {
  Lcd1602State* s = (Lcd1602State*)d->state;
  if (!s->dirty) return;

  for (int r = 0; r < LCD_ROWS; r++) {
    if (strcmp(s->buf[r], s->pending[r]) != 0) {
      s->lcd->setCursor(0, r);
      s->lcd->print(s->pending[r]);
      memcpy(s->buf[r], s->pending[r], LCD_COLS + 1);
    }
  }
  s->dirty = false;
}

static void lcd_print_str(DisplayDriver* d, int16_t x, int16_t y,
                          const char* str, uint8_t size, uint8_t color) {
  Lcd1602State* s = (Lcd1602State*)d->state;
  int col = px_to_col(x);
  int row = px_to_row(y);

  // size > 1 não é suportado em LCD de texto — ignora escala
  (void)size;

  int len = strlen(str);
  for (int i = 0; i < len && col + i < LCD_COLS; i++) {
    char c = str[i];
    // DISPLAY_COLOR_INVERT: no LCD, simula com caractere diferente
    // Na prática, LiquidCrystal não suporta inversão por célula sem
    // custom chars. Usamos o caractere normalmente.
    s->pending[row][col + i] = (c >= 32 && c < 128) ? c : '?';
  }
  s->dirty = true;
}

static void lcd_print_char(DisplayDriver* d, int16_t x, int16_t y,
                           char c, uint8_t size, uint8_t color) {
  char buf[2] = { c, '\0' };
  lcd_print_str(d, x, y, buf, size, color);
}

static uint16_t lcd_text_width(DisplayDriver* d, const char* str, uint8_t size) {
  // Cada caractere ocupa LCD_CHAR_W pixels no modelo de coordenadas
  return (uint16_t)(strlen(str) * LCD_CHAR_W);
}

// draw_pixel e draw_rect não fazem nada no LCD de texto —
// o LCD não tem modo gráfico. Os ponteiros ficam nullptr.

// ── factory ───────────────────────────────────────────────────

DisplayDriver* driver_lcd1602_create(int rs, int en,
                                     int d4, int d5, int d6, int d7) {
  DisplayDriver* drv = (DisplayDriver*)malloc(sizeof(DisplayDriver));
  Lcd1602State* st = (Lcd1602State*)malloc(sizeof(Lcd1602State));
  if (!drv || !st) {
    free(drv);
    free(st);
    return nullptr;
  }

  st->lcd = new LiquidCrystal(rs, en, d4, d5, d6, d7);
  st->dirty = false;

  drv->name = "LCD1602";
  drv->device_id = DISPLAY_DEFAULT_ID;
  // Geometria em pixels virtuais: 16 chars x 8px, 2 rows x 8px
  drv->geometry = { (uint16_t)(LCD_COLS * LCD_CHAR_W), (uint16_t)(LCD_ROWS * LCD_CHAR_H) };
  drv->init = lcd_init;
  drv->clear = lcd_clear;
  drv->flush = lcd_flush;
  drv->draw_pixel = nullptr;  // LCD texto não suporta pixel
  drv->draw_rect = nullptr;   // LCD texto não suporta rect gráfico
  drv->draw_line = nullptr;
  drv->print_str = lcd_print_str;
  drv->print_char = lcd_print_char;
  drv->text_width = lcd_text_width;
  drv->set_contrast = nullptr;  // contraste via potenciômetro físico
  drv->state = st;
  return drv;
}