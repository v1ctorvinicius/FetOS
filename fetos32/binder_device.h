//binder_device.h
#pragma once
#include "device.h"
#include "capability.h"

#define BINDER_MAX_BINDINGS 8

typedef struct {
  char source[32];   // capability de origem (ex: "input:button")
  char target[32];   // capability de destino (ex: "led:set")
  bool active;
} Binding;

// ── API C direta (usada pelo kernel e apps nativos) ───────────

// adiciona binding; retorna índice ou -1 se cheio
int  binder_add(const char* source, const char* target);
void binder_remove(int idx);
void binder_clear();
int  binder_count();
const Binding* binder_get(int idx);

// dispara todos os bindings cujo source == cap
// chamado pelo sistema de eventos quando um evento com capability ocorre
void binder_dispatch(const char* source_cap, const RequestPayload* payload);

// ── device init ───────────────────────────────────────────────

void binder_device_init(Device* dev);