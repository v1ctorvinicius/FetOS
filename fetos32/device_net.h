#pragma once
#include "device.h"
#include "capability.h"

// ── fila de recepção por processo ─────────────────────────────
// alocada lazy no cap_state do processo via fvm_get_cap_state

#define NET_RECV_QUEUE_SIZE 8

typedef struct {
  char     topic[32];          // tópico filtrado por este processo
  int32_t  values[NET_RECV_QUEUE_SIZE];
  uint8_t  head;
  uint8_t  tail;
  uint8_t  count;
} NetRecvState;

// ── API interna ───────────────────────────────────────────────
// chamada pelo fetlink_ble quando chega FLINK_PUB
void net_device_publish(const char* topic, int32_t value);

void net_device_init(Device* dev);