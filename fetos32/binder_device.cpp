//binder_device.cpp
#include "binder_device.h"
#include "system.h"
#include "persistence.h"
#include <string.h>
#include <stdio.h>

// ── estado global ─────────────────────────────────────────────

static Binding s_bindings[BINDER_MAX_BINDINGS];
static int     s_count = 0;

// ── persistência ──────────────────────────────────────────────
// Cada binding é salvo em duas chaves NVS:
//   bnd_src_N  →  source string
//   bnd_tgt_N  →  target string

static void save_all() {
  for (int i = 0; i < BINDER_MAX_BINDINGS; i++) {
    char ks[12], kt[12];
    snprintf(ks, sizeof(ks), "bnd_src_%d", i);
    snprintf(kt, sizeof(kt), "bnd_tgt_%d", i);
    if (i < s_count && s_bindings[i].active) {
      persistence_write_string(ks, s_bindings[i].source);
      persistence_write_string(kt, s_bindings[i].target);
    } else {
      persistence_write_string(ks, "");
      persistence_write_string(kt, "");
    }
  }
}

static void load_all() {
  s_count = 0;
  char ks[12], kt[12];
  char src[32], tgt[32];
  for (int i = 0; i < BINDER_MAX_BINDINGS; i++) {
    snprintf(ks, sizeof(ks), "bnd_src_%d", i);
    snprintf(kt, sizeof(kt), "bnd_tgt_%d", i);
    persistence_read_string(ks, src, sizeof(src));
    persistence_read_string(kt, tgt, sizeof(tgt));
    if (src[0] != '\0' && tgt[0] != '\0') {
      strncpy(s_bindings[s_count].source, src, 31);
      strncpy(s_bindings[s_count].target, tgt, 31);
      s_bindings[s_count].active = true;
      s_count++;
    }
  }
}

// ── API C ─────────────────────────────────────────────────────

int binder_add(const char* source, const char* target) {
  if (s_count >= BINDER_MAX_BINDINGS) return -1;
  int idx = s_count++;
  strncpy(s_bindings[idx].source, source, 31);
  strncpy(s_bindings[idx].target, target, 31);
  s_bindings[idx].active = true;
  save_all();
  return idx;
}

void binder_remove(int idx) {
  if (idx < 0 || idx >= s_count) return;
  for (int i = idx; i < s_count - 1; i++) {
    s_bindings[i] = s_bindings[i + 1];
  }
  s_bindings[--s_count].active = false;
  save_all();
}

void binder_clear() {
  s_count = 0;
  save_all();
}

int binder_count() { return s_count; }

const Binding* binder_get(int idx) {
  if (idx < 0 || idx >= s_count) return nullptr;
  return &s_bindings[idx];
}

void binder_dispatch(const char* source_cap, const RequestPayload* payload) {
  for (int i = 0; i < s_count; i++) {
    if (s_bindings[i].active &&
        strncmp(s_bindings[i].source, source_cap, 31) == 0) {
      system_request(s_bindings[i].target, payload);
    }
  }
}

// ── handlers de capability ────────────────────────────────────

// bind:add   source(str) target(str)  → índice ou -1
static RequestResult h_add(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const RequestParam* src_p = payload_get(p, "source");
  const RequestParam* tgt_p = payload_get(p, "target");
  if (!src_p || !src_p->str_value || !tgt_p || !tgt_p->str_value) return REQ_IGNORED;

  int idx = binder_add(src_p->str_value, tgt_p->str_value);
  const RequestParam* r = payload_get(p, "result");
  if (r) ((RequestParam*)r)->int_value = idx;
  return REQ_ACCEPTED;
}

// bind:remove   idx(int)
static RequestResult h_remove(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const RequestParam* idx_p = payload_get(p, "idx");
  if (!idx_p) return REQ_IGNORED;
  binder_remove((int)idx_p->int_value);
  return REQ_ACCEPTED;
}

// bind:clear
static RequestResult h_clear(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)p; (void)c;
  binder_clear();
  return REQ_ACCEPTED;
}

// bind:count  → int
static RequestResult h_count(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const RequestParam* r = payload_get(p, "result");
  if (r) ((RequestParam*)r)->int_value = s_count;
  return REQ_ACCEPTED;
}

// bind:get_source   idx(int)  → str_value (via result como int = índice string)
// Como a FVM não tem str retorno direto, encodamos source/target como índices
// nas strings do kernel. Para uso em apps FetScript usamos bind:list_str.
static RequestResult h_get_source(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const RequestParam* idx_p = payload_get(p, "idx");
  if (!idx_p) return REQ_IGNORED;
  int idx = (int)idx_p->int_value;
  if (idx < 0 || idx >= s_count) return REQ_IGNORED;

  // Escreve comprimento da string source no result (usável para display)
  const RequestParam* r = payload_get(p, "result");
  if (r) ((RequestParam*)r)->int_value = (int32_t)strlen(s_bindings[idx].source);
  // str_value aponta para o buffer estático
  if (r) ((RequestParam*)r)->str_value = s_bindings[idx].source;
  return REQ_ACCEPTED;
}

static RequestResult h_get_target(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev; (void)c;
  const RequestParam* idx_p = payload_get(p, "idx");
  if (!idx_p) return REQ_IGNORED;
  int idx = (int)idx_p->int_value;
  if (idx < 0 || idx >= s_count) return REQ_IGNORED;
  const RequestParam* r = payload_get(p, "result");
  if (r) ((RequestParam*)r)->int_value = (int32_t)strlen(s_bindings[idx].target);
  if (r) ((RequestParam*)r)->str_value = s_bindings[idx].target;
  return REQ_ACCEPTED;
}

// ── init ──────────────────────────────────────────────────────

void binder_device_init(Device* dev) {
  dev->state = nullptr;
  load_all();

  system_register_capability("bind:add",        h_add,        dev);
  system_register_capability("bind:remove",     h_remove,     dev);
  system_register_capability("bind:clear",      h_clear,      dev);
  system_register_capability("bind:count",      h_count,      dev);
  system_register_capability("bind:get_source", h_get_source, dev);
  system_register_capability("bind:get_target", h_get_target, dev);

  Serial.printf("[Binder] %d bindings carregados\n", s_count);
}