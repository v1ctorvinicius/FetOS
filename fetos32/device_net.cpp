// device_net.cpp
#include "device_net.h"
#include "fetlink.h"
#include "fetlink_dual.h"  // Adicionado para enxergar fetlink_ble_init
#include "system.h"
#include "fvm_process.h"
#include "fvm_app.h"
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"  // Necessário para ler o MAC
#include "esp_mac.h"     // Necessário para ler o MAC

extern bool s_connected;

// --- A REFORMA DO ROTEADOR ---
#define NET_SUBSCRIBERS_MAX 16

typedef struct
{
  void *fvm_proc;       // ID do processo FVM
  NetRecvState *state;  // Fila EXCLUSIVA para este tópico
  char topic[32];       // Nome do Tópico
} NetSubscriber;

static NetSubscriber s_subscribers[NET_SUBSCRIBERS_MAX];
static uint8_t s_sub_count = 0;

// ── FUNÇÕES DE FILA (Ring Buffer) ──────────────────────────────
static bool queue_push(NetRecvState *st, int32_t value) {
  if (!st)
    return false;
  if (st->count >= NET_RECV_QUEUE_SIZE) {
    st->tail = (st->tail + 1) % NET_RECV_QUEUE_SIZE;
    st->count--;
  }
  st->values[st->head] = value;
  st->head = (st->head + 1) % NET_RECV_QUEUE_SIZE;
  st->count++;
  return true;
}

static bool queue_pop(NetRecvState *st, int32_t *out) {
  if (!st || st->count == 0)
    return false;
  *out = st->values[st->tail];
  st->tail = (st->tail + 1) % NET_RECV_QUEUE_SIZE;
  st->count--;
  return true;
}

// ── HELPER ATUALIZADO: BUSCA ESTADO POR (PROCESSO + TÓPICO) ────
static NetRecvState *find_process_topic_state(void *fvm_proc, const char *topic) {
  for (uint8_t i = 0; i < s_sub_count; i++) {
    if (s_subscribers[i].fvm_proc == fvm_proc && strcmp(s_subscribers[i].topic, topic) == 0) {
      return s_subscribers[i].state;
    }
  }
  return nullptr;
}

// ── PUBLISH: CHAMADO PELO RADIO (FETLINK) ──────────────────────
void net_device_publish(const char *topic, int32_t value) {
  Serial.printf("[Net Debug] Chegou no roteador: tópico='%s', valor=%d\n", topic, value);

  for (uint8_t i = 0; i < s_sub_count; i++) {
    if (strcmp(s_subscribers[i].topic, topic) == 0) {
      if (queue_push(s_subscribers[i].state, value)) {
        Serial.printf("[Net Debug] MATCH! Entregue a proc 0x%p (Tópico: %s, Fila: %d)\n",
                      s_subscribers[i].fvm_proc, s_subscribers[i].topic, s_subscribers[i].state->count);
      }
    }
  }
}

// ── HANDLERS DE CAPABILITY (FVM) ───────────────────────────────

static RequestResult handle_subscribe(Device *dev, const RequestPayload *payload, CallerContext *caller) {
  if (!caller || caller->type != CALLER_FVM)
    return REQ_IGNORED;

  const RequestParam *topic_p = payload_get(payload, "topic");
  if (!topic_p || !topic_p->str_value)
    return REQ_IGNORED;

  NetRecvState *st = find_process_topic_state(caller->fvm_proc, topic_p->str_value);

  if (st) {
    Serial.printf("[Net] 0x%p ja assinava '%s'\n", caller->fvm_proc, topic_p->str_value);
    return REQ_ACCEPTED;
  }

  if (s_sub_count >= NET_SUBSCRIBERS_MAX)
    return REQ_OOM;

  st = (NetRecvState *)malloc(sizeof(NetRecvState));
  if (!st)
    return REQ_OOM;
  memset(st, 0, sizeof(NetRecvState));

  s_subscribers[s_sub_count].fvm_proc = caller->fvm_proc;
  s_subscribers[s_sub_count].state = st;
  strncpy(s_subscribers[s_sub_count].topic, topic_p->str_value, 31);
  s_sub_count++;

  Serial.printf("[Net] Nova Assinatura: Proc 0x%p -> Topico '%s'\n", caller->fvm_proc, topic_p->str_value);

  return REQ_ACCEPTED;
}

static RequestResult handle_recv(Device *dev, const RequestPayload *payload, CallerContext *caller) {
  if (!caller || caller->type != CALLER_FVM)
    return REQ_IGNORED;

  const RequestParam *topic_p = payload_get(payload, "topic");
  if (!topic_p || !topic_p->str_value)
    return REQ_IGNORED;

  NetRecvState *st = find_process_topic_state(caller->fvm_proc, topic_p->str_value);
  if (!st)
    return REQ_IGNORED;

  const RequestParam *result_p = payload_get(payload, "result");
  if (!result_p && payload->count > 0)
    result_p = &payload->params[0];

  int32_t value = 0;
  if (!queue_pop(st, &value)) {
    if (result_p)
      ((RequestParam *)result_p)->int_value = -1;
  } else {
    if (result_p)
      ((RequestParam *)result_p)->int_value = value;
    Serial.printf("[Net] FVM consumiu valor %d do tópico '%s'\n", value, topic_p->str_value);
  }

  return REQ_ACCEPTED;
}

static RequestResult handle_send(Device *dev, const RequestPayload *payload, CallerContext *caller) {
  const RequestParam *dst_p = payload_get(payload, "dst");
  const RequestParam *topic_p = payload_get(payload, "topic");
  const RequestParam *value_p = payload_get(payload, "value");

  if (!topic_p || !topic_p->str_value || !value_p)
    return REQ_IGNORED;

  uint8_t dst = dst_p ? (uint8_t)dst_p->int_value : 0x00;

  uint8_t buf[64];
  uint8_t topic_len = (uint8_t)strlen(topic_p->str_value);
  if (topic_len > 30)
    topic_len = 30;

  buf[0] = topic_len;
  memcpy(&buf[1], topic_p->str_value, topic_len);
  uint8_t pos = 1 + topic_len;

  buf[pos++] = 0x02;
  int32_t val = value_p->int_value;
  buf[pos++] = (uint8_t)((val >> 8) & 0xFF);
  buf[pos++] = (uint8_t)(val & 0xFF);

  return fetlink_send(dst, FLINK_PUB, buf, pos) ? REQ_ACCEPTED : REQ_IGNORED;
}

static RequestResult handle_node_id(Device *dev, const RequestPayload *payload, CallerContext *caller) {
  const RequestParam *result_p = payload_get(payload, "result");
  if (result_p) {
    ((RequestParam *)result_p)->int_value = (int32_t)fetlink_node_id();
  }
  return REQ_ACCEPTED;
}

// ── INICIALIZAÇÃO ──────────────────────────────────────────────

void net_device_init(Device *dev) {
  memset(s_subscribers, 0, sizeof(s_subscribers));
  s_sub_count = 0;

  system_register_capability("net:subscribe", handle_subscribe, dev);
  system_register_capability("net:recv", handle_recv, dev);
  system_register_capability("net:send", handle_send, dev);
  system_register_capability("net:node_id", handle_node_id, dev);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  uint8_t my_node_id = mac[5];

  fetlink_ble_init(my_node_id);

  Serial.printf("[Net] Device Hub iniciado (Node ID: 0x%02X)\n", my_node_id);
}