#include "device_net.h"
#include "fetlink.h"
#include "system.h"
#include "fvm_process.h"
#include "fvm_app.h"
#include <string.h>
#include <stdlib.h>

// ── ESTADO GLOBAL DO DEVICE ───────────────────────────────────
extern bool s_connected;

// Estrutura para unificar o estado da rede por processo
#define NET_SUBSCRIBERS_MAX 8

typedef struct {
  void* fvm_proc;       // ID único do processo FVM
  NetRecvState* state;  // Fila de mensagens compartilhada
  char topic[32];       // Tópico assinado
} NetSubscriber;

static NetSubscriber s_subscribers[NET_SUBSCRIBERS_MAX];
static uint8_t s_sub_count = 0;

// ── FUNÇÕES DE FILA (Ring Buffer) ──────────────────────────────

static bool queue_push(NetRecvState* st, int32_t value) {
  if (!st) return false;

  if (st->count >= NET_RECV_QUEUE_SIZE) {
    // Fila cheia: avança cauda para sobrescrever o mais antigo
    st->tail = (st->tail + 1) % NET_RECV_QUEUE_SIZE;
    st->count--;
  }

  st->values[st->head] = value;
  st->head = (st->head + 1) % NET_RECV_QUEUE_SIZE;
  st->count++;
  return true;
}

static bool queue_pop(NetRecvState* st, int32_t* out) {
  if (!st || st->count == 0) return false;

  *out = st->values[st->tail];
  st->tail = (st->tail + 1) % NET_RECV_QUEUE_SIZE;
  st->count--;
  return true;
}

// ── HELPER: BUSCA ESTADO POR PROCESSO ──────────────────────────

static NetRecvState* find_process_state(void* fvm_proc) {
  for (uint8_t i = 0; i < s_sub_count; i++) {
    if (s_subscribers[i].fvm_proc == fvm_proc) {
      return s_subscribers[i].state;
    }
  }
  return nullptr;
}

// ── PUBLISH: CHAMADO PELO RADIO (FETLINK) ──────────────────────

void net_device_publish(const char* topic, int32_t value) {
  Serial.printf("[Net Debug] Chegou no roteador: tópico='%s', valor=%d\n", topic, value);

  for (uint8_t i = 0; i < s_sub_count; i++) {
    if (strcmp(s_subscribers[i].topic, topic) == 0) {
      if (queue_push(s_subscribers[i].state, value)) {
        Serial.printf("[Net Debug] MATCH! Entregue ao processo 0x%p (Fila: %d)\n",
                      s_subscribers[i].fvm_proc, s_subscribers[i].state->count);
      }
    }
  }
}

// ── HANDLERS DE CAPABILITY (FVM) ───────────────────────────────

// net:subscribe
static RequestResult handle_subscribe(Device* dev, const RequestPayload* payload, CallerContext* caller) {
  if (!caller || caller->type != CALLER_FVM) return REQ_IGNORED;

  const RequestParam* topic_p = payload_get(payload, "topic");
  if (!topic_p || !topic_p->str_value) return REQ_IGNORED;

  NetRecvState* st = find_process_state(caller->fvm_proc);

  if (!st) {
    // Novo assinante: aloca estado
    if (s_sub_count >= NET_SUBSCRIBERS_MAX) return REQ_OOM;

    st = (NetRecvState*)malloc(sizeof(NetRecvState));
    if (!st) return REQ_OOM;
    memset(st, 0, sizeof(NetRecvState));

    s_subscribers[s_sub_count].fvm_proc = caller->fvm_proc;
    s_subscribers[s_sub_count].state = st;
    s_sub_count++;

    Serial.printf("[Net] Novo subscriber: 0x%p\n", caller->fvm_proc);
  }

  strncpy(s_subscribers[s_sub_count - 1].topic, topic_p->str_value, 31);
  Serial.printf("[Net] Subscribed: '%s'\n", topic_p->str_value);

  return REQ_ACCEPTED;
}

// net:recv
static RequestResult handle_recv(Device* dev, const RequestPayload* payload, CallerContext* caller) {
  if (!caller || caller->type != CALLER_FVM) return REQ_IGNORED;

  NetRecvState* st = find_process_state(caller->fvm_proc);
  if (!st) return REQ_IGNORED;

  const RequestParam* result_p = payload_get(payload, "result");
  if (!result_p && payload->count > 0) result_p = &payload->params[0];

  int32_t value = 0;
  if (!queue_pop(st, &value)) {
    if (result_p) ((RequestParam*)result_p)->int_value = -1;
  } else {
    // Cast para (RequestParam*) para poder entregar o valor à FVM
    if (result_p) ((RequestParam*)result_p)->int_value = value;
    Serial.printf("[Net] FVM consumiu valor: %d\n", value);
  }

  return REQ_ACCEPTED;
}

// net:send
static RequestResult handle_send(Device* dev, const RequestPayload* payload, CallerContext* caller) {
  const RequestParam* dst_p = payload_get(payload, "dst");
  const RequestParam* topic_p = payload_get(payload, "topic");
  const RequestParam* value_p = payload_get(payload, "value");

  if (!topic_p || !topic_p->str_value || !value_p) return REQ_IGNORED;

  uint8_t dst = dst_p ? (uint8_t)dst_p->int_value : 0x00;

  uint8_t buf[64];
  uint8_t topic_len = (uint8_t)strlen(topic_p->str_value);
  if (topic_len > 30) topic_len = 30;

  buf[0] = topic_len;
  memcpy(&buf[1], topic_p->str_value, topic_len);
  uint8_t pos = 1 + topic_len;

  buf[pos++] = 0x02;  // Type Int16/Int32
  int32_t val = value_p->int_value;
  buf[pos++] = (uint8_t)((val >> 8) & 0xFF);
  buf[pos++] = (uint8_t)(val & 0xFF);

  return fetlink_send(dst, FLINK_PUB, buf, pos) ? REQ_ACCEPTED : REQ_IGNORED;
}

// net:node_id
static RequestResult handle_node_id(Device* dev, const RequestPayload* payload, CallerContext* caller) {
  const RequestParam* result_p = payload_get(payload, "result");
  // Fazemos o cast para (RequestParam*) para permitir a escrita
  if (result_p) {
    ((RequestParam*)result_p)->int_value = (int32_t)fetlink_node_id();
  }
  return REQ_ACCEPTED;
}

// ── INICIALIZAÇÃO ──────────────────────────────────────────────

void net_device_init(Device* dev) {
  memset(s_subscribers, 0, sizeof(s_subscribers));
  s_sub_count = 0;

  system_register_capability("net:subscribe", handle_subscribe, dev);
  system_register_capability("net:recv", handle_recv, dev);
  system_register_capability("net:send", handle_send, dev);
  system_register_capability("net:node_id", handle_node_id, dev);

  Serial.println("[Net] Device Hub iniciado");
}