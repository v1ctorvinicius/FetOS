//ui_device.cpp
#include "ui_device.h"
#include "system.h"
#include "display_hal.h"
#include "text_buffer_device.h"
#include "Arduino.h"
#include <stdlib.h>

#define D DISPLAY_DEFAULT_ID

// ── helpers ───────────────────────────────────────────────────

static const char* get_str(const RequestPayload* p, const char* key) {
  const RequestParam* r = payload_get(p, key);
  return (r && r->str_value) ? r->str_value : nullptr;
}

static int32_t get_int(const RequestPayload* p, const char* key, int32_t def) {
  const RequestParam* r = payload_get(p, key);
  return r ? r->int_value : def;
}

// ── handlers ──────────────────────────────────────────────────

static RequestResult h_num(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  
  const RequestParam* pV = payload_get(p, "v"); // Lendo a chave 'v'
  if (!pV) return REQ_IGNORED;

  int x = get_int(p, "x", 0);
  int y = get_int(p, "y", 0);
  int size = get_int(p, "size", 2); // Tamanho padrão 2 para números maiores

  char buf[16];
  itoa(pV->int_value, buf, 10);

  hal_text(DISPLAY_DEFAULT_ID, x, y, buf, size);

  return REQ_ACCEPTED;
}

static RequestResult h_clear(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)p; (void)c;
  hal_clear(D);
  return REQ_ACCEPTED;
}

static RequestResult h_flush(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)p; (void)c;
  hal_flush(D);
  return REQ_ACCEPTED;
}

static RequestResult h_text(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const char* t = get_str(p, "text");
  if (!t) return REQ_IGNORED;
  hal_text(D, get_int(p, "x", 0), get_int(p, "y", 0), t, get_int(p, "size", 1));
  return REQ_ACCEPTED;
}

// NOVO: Handler para renderizar buffers de texto via ID
static RequestResult h_textbuf(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  int id = get_int(p, "id", 0);
  int x  = get_int(p, "x", 0);
  int y  = get_int(p, "y", 0);
  int size = get_int(p, "size", 1);
  
  // Chama a nova API global do text_buffer_device
  const char* text = textbuf_get_buffer(id);
  if (text && text[0] != '\0') {
    hal_text(D, x, y, text, size);
  }
  return REQ_ACCEPTED;
}

// Header de 16 pixels (0-15) com limpeza de fundo
static RequestResult h_header(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const char* t = get_str(p, "text");
  if (!t) return REQ_IGNORED;

  // Limpa a área do header (retângulo preto)
  hal_rect(D, 0, 0, 128, 16, false); 
  // Texto centralizado (y=4 para ficar no meio dos 16px)
  hal_text_center(D, 4, t, 1); 
  // Linha separadora na base do header
  hal_rect(D, 0, 15, 128, 1, true); 
  
  return REQ_ACCEPTED;
}

static RequestResult h_rect(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  hal_rect(D, get_int(p, "x", 0), get_int(p, "y", 0), 
    get_int(p, "w", 10), get_int(p, "h", 10), get_int(p, "fill", 1) != 0);
  return REQ_ACCEPTED;
}

static RequestResult h_cursor(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  int x = get_int(p, "x", 0);
  int y = get_int(p, "y", 0);
  if ((millis() / 500) % 2) hal_rect(D, x, y, 3, 8, true);
  return REQ_ACCEPTED;
}


// footer: separador em y=54, texto em y=56 (altura 10 px total)
static RequestResult h_footer(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  const char* t = get_str(p, "text");
  if (!t) return REQ_IGNORED;
  hal_rect(D, 0, 54, 128, 1, true);  // separador
  hal_text_center(D, 56, t, 1);
  return REQ_ACCEPTED;
}

// list: items separados por \n, selected=índice selecionado, start_y, visible
static RequestResult h_list(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  const char* raw = get_str(p, "items");
  if (!raw) return REQ_IGNORED;

  // split em \n — máximo 16 itens
  static char buf[512];
  static const char* ptrs[16];
  strncpy(buf, raw, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  int count = 0;
  char* cursor = buf;
  ptrs[count++] = cursor;
  while (*cursor && count < 16) {
    if (*cursor == '\n') {
      *cursor = '\0';
      if (*(cursor + 1)) ptrs[count++] = cursor + 1;
    }
    cursor++;
  }

  int selected = get_int(p, "selected", 0);
  int start_y = get_int(p, "start_y", 16);
  int visible = get_int(p, "visible", 5);

  hal_list_scroll(D, ptrs, count, selected, start_y, visible);
  return REQ_ACCEPTED;
}
static RequestResult h_invert_row(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int y = get_int(p, "y", 0);
  int h = get_int(p, "h", 8);
  // SSD1306 não tem invert_rect nativo — usamos fillRect com cor INVERT via HAL
  DisplayDriver* drv = display_hal_get_primary(D);
  if (drv && drv->draw_rect) {
    drv->draw_rect(drv, 0, y, 128, h, DISPLAY_COLOR_INVERT, true);
  }
  return REQ_ACCEPTED;
}

// ── init ──────────────────────────────────────────────────────

void ui_device_init(Device* dev) {
  dev->state = nullptr;

  system_register_capability("ui:clear",       h_clear,       dev);
  system_register_capability("ui:flush",       h_flush,       dev);
  system_register_capability("ui:text",        h_text,        dev);
  system_register_capability("ui:textbuf",     h_textbuf,     dev);
  system_register_capability("ui:header",      h_header,      dev);
  system_register_capability("ui:rect",        h_rect,        dev);
  system_register_capability("ui:cursor",      h_cursor,      dev);
  system_register_capability("ui:footer",      h_footer,      dev);
  system_register_capability("ui:list",      h_list,      dev);
  system_register_capability("ui:invert_row",      h_invert_row,      dev);
  system_register_capability("ui:num", h_num, dev);
  Serial.println("[UI] Device iniciado");
}




