#include "text_buffer_device.h"
#include "system.h"
#include <stdlib.h>
#include <string.h>

#define MAX_TEXT_BUFFERS 4

typedef struct {
  uint8_t* data;
  uint16_t size;
  uint16_t capacity;
} TextBufState;

static TextBufState s_buffers[MAX_TEXT_BUFFERS];



// API C pura para o UI Device conseguir desenhar o texto direto!
const char* textbuf_get_buffer(uint8_t id) {
  if (id >= MAX_TEXT_BUFFERS || !s_buffers[id].data) return "";
  return (const char*)s_buffers[id].data;
}

// Helper para pegar o buffer correto baseado no parâmetro "id"
static TextBufState* get_buf(const RequestPayload* p) {
  const RequestParam* id_p = payload_get(p, "id");
  if (!id_p) return nullptr;
  int id = id_p->int_value;
  if (id < 0 || id >= MAX_TEXT_BUFFERS) return nullptr;
  return &s_buffers[id];
}

static RequestResult handle_size(Device* dev, const RequestPayload* p, CallerContext* c) {
  TextBufState* st = get_buf(p);
  if (!st) return REQ_IGNORED;
  p->params[0].int_value = st->size;
  return REQ_ACCEPTED;
}

static RequestResult handle_get_char(Device* dev, const RequestPayload* p, CallerContext* c) {
  TextBufState* st = get_buf(p);
  if (!st || !st->data) return REQ_IGNORED;
const RequestParam* pos_p = payload_get(p, "pos");
  if (!pos_p) return REQ_IGNORED;
  int pos = pos_p->int_value;
  if (pos < 0 || pos >= (int)st->size) {
    p->params[0].int_value = -1;
    return REQ_ACCEPTED;
  }
  p->params[0].int_value = st->data[pos];
  return REQ_ACCEPTED;
}

static RequestResult handle_alloc(Device* dev, const RequestPayload* p, CallerContext* c) {
  TextBufState* st = get_buf(p);
  if (!st) return REQ_IGNORED;
  const RequestParam* sz = payload_get(p, "size");
  if (!sz) return REQ_IGNORED;
  int req = sz->int_value;

  if (st->data) free(st->data);
  st->data = (uint8_t*)malloc(req + 1);  // +1 garante o \0 para exibir o texto
  if (!st->data) return REQ_OOM;
  memset(st->data, 0, req + 1);
  st->capacity = req;
  st->size = 0;
  return REQ_ACCEPTED;
}

static RequestResult handle_insert(Device* dev, const RequestPayload* p, CallerContext* c) {
  TextBufState* st = get_buf(p);
  if (!st || !st->data) return REQ_IGNORED;
  int pos = payload_get(p, "pos") ? payload_get(p, "pos")->int_value : 0;
  int ch = payload_get(p, "char") ? payload_get(p, "char")->int_value : 0;

  if (st->size >= st->capacity) return REQ_ACCEPTED;
  if (pos < 0) pos = 0;
  if (pos > st->size) pos = st->size;

  int to_move = st->size - pos;
  if (to_move > 0) memmove(&st->data[pos + 1], &st->data[pos], to_move);
  st->data[pos] = (uint8_t)ch;
  st->size++;
  st->data[st->size] = '\0';  // Mantém o \0 no fim

  return REQ_ACCEPTED;
}

static RequestResult handle_delete(Device* dev, const RequestPayload* p, CallerContext* c) {
  TextBufState* st = get_buf(p);
  if (!st || !st->data || st->size == 0) return REQ_IGNORED;
  int pos = payload_get(p, "pos") ? payload_get(p, "pos")->int_value : 0;

  if (pos < 0 || pos >= st->size) return REQ_ACCEPTED;

  int to_move = st->size - pos - 1;
  if (to_move > 0) memmove(&st->data[pos], &st->data[pos + 1], to_move);
  st->size--;
  st->data[st->size] = '\0';
  return REQ_ACCEPTED;
}

static RequestResult handle_clear(Device* dev, const RequestPayload* p, CallerContext* c) {
  TextBufState* st = get_buf(p);
  if (!st || !st->data) return REQ_IGNORED;
  st->size = 0;
  st->data[0] = '\0';
  return REQ_ACCEPTED;
}

void textbuf_device_init(Device* dev) {
  memset(s_buffers, 0, sizeof(s_buffers));
  system_register_capability("textbuf:alloc", handle_alloc, dev);
  system_register_capability("textbuf:insert", handle_insert, dev);
  system_register_capability("textbuf:delete", handle_delete, dev);
  system_register_capability("textbuf:clear", handle_clear, dev);
  system_register_capability("textbuf:size", handle_size, dev);
  system_register_capability("textbuf:get_char", handle_get_char, dev);
}

// Em text_buffer_device.cpp
void textbuf_raw_insert(uint8_t id, char c) {
  if (id >= 4 || !s_buffers[id].data) return;
  TextBufState* st = &s_buffers[id];
  if (st->size < st->capacity) {
    st->data[st->size++] = (uint8_t)c;
    st->data[st->size] = '\0';
  }
}