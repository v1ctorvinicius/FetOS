#pragma once
#include <Arduino.h>


// ── versão ────────────────────────────────────────────────────
#define FETLINK_VERSION      0x01
#define FETLINK_STX          0x02

// ── tamanhos ──────────────────────────────────────────────────
#define FETLINK_HEADER_SIZE    7    // STX + VER + SRC + DST + TYPE + SEQ + LEN
#define FETLINK_PAYLOAD_MAX  128
#define FETLINK_FRAME_MAX    136    // header + payload + csum
#define FETLINK_SEEN_CACHE    32    // últimos N pares (src, seq) vistos
#define FETLINK_FRAG_BUF_MAX 512   // buffer de reassembly de fragmentos

// ── timeouts (ms) ─────────────────────────────────────────────
#define FETLINK_FRAME_TIMEOUT      100
#define FETLINK_INTERBYTE_TIMEOUT   50
#define FETLINK_REASSEMBLY_TIMEOUT 2000

// ── tipos de mensagem ─────────────────────────────────────────
typedef enum : uint8_t {
  FLINK_HELLO = 0x01,  // descoberta de nó
  FLINK_PUB   = 0x02,  // publicação de dado (topic-first payload)
  FLINK_CMD   = 0x03,  // comando — payload é capability string
  FLINK_ACK   = 0x04,  // confirmação de recebimento
  FLINK_APP   = 0x05,  // payload é um .fvm (fragmentado)
} FetLinkType;

// ── SEQ dual-mode ─────────────────────────────────────────────
// bit7=FRAG_FLAG: 0 → SEQ normal p/ ACK correlation
//                 1 → fragmento: bit6=LAST, bits5-0=IDX
#define FLINK_SEQ_IS_FRAG(seq)        ((seq) & 0x80)
#define FLINK_SEQ_IS_LAST(seq)        ((seq) & 0x40)
#define FLINK_SEQ_FRAG_IDX(seq)       ((seq) & 0x3F)
#define FLINK_SEQ_MAKE_FRAG(idx, last) \
  (uint8_t)(0x80 | ((last) ? 0x40 : 0x00) | ((idx) & 0x3F))

// ── frame decodificado ────────────────────────────────────────
typedef struct {
  uint8_t ver;
  uint8_t src;
  uint8_t dst;
  uint8_t type;
  uint8_t seq;
  uint8_t len;
  uint8_t payload[FETLINK_PAYLOAD_MAX];
  uint8_t csum;
} FetLinkFrame;

// ── parser state machine ──────────────────────────────────────
typedef enum : uint8_t {
  FLINK_PARSE_IDLE,
  FLINK_PARSE_HEADER,
  FLINK_PARSE_PAYLOAD,
} FetLinkParseState;

typedef struct {
  FetLinkParseState state;
  uint8_t  buf[FETLINK_FRAME_MAX];
  uint8_t  pos;
  uint32_t last_byte_ms;
  uint32_t frame_start_ms;
} FetLinkParser;

// ── reassembly de fragmentos ──────────────────────────────────
typedef struct {
  bool     active;
  uint8_t  src;
  uint8_t  type;
  uint8_t  buf[FETLINK_FRAG_BUF_MAX];
  uint16_t total_len;
  uint32_t last_activity_ms;
} FetLinkReassembly;

// ── callback de recepção ──────────────────────────────────────
// chamado quando um frame completo ou mensagem reassemblada chega
typedef void (*FetLinkReceiveCb)(
  uint8_t src,
  uint8_t type,
  const uint8_t* data,
  uint16_t len
);

// ── API pública ───────────────────────────────────────────────
void    fetlink_init(uint8_t node_id, FetLinkReceiveCb on_receive);
bool    fetlink_send(uint8_t dst, FetLinkType type, const uint8_t* data, uint16_t len);
void    fetlink_feed(uint8_t byte);   // alimentar byte a byte do transporte físico
void    fetlink_tick();               // chamar periodicamente para timeouts
uint8_t fetlink_node_id();

// transporte — implementado em fetlink_ble.cpp
extern bool fetlink_transport_write(const uint8_t* data, uint8_t len);