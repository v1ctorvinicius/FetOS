//fetlink.cpp
#include "fetlink.h"
#include "system.h"
#include <LittleFS.h>
#include <string.h>
#include <stdlib.h>

// ── estado interno ────────────────────────────────────────────

static uint8_t s_node_id = 0;
static FetLinkReceiveCb s_on_receive = nullptr;
static FetLinkParser s_parser;
static FetLinkReassembly s_reassembly;
static uint8_t s_seq = 0;

// ── seen cache ────────────────────────────────────────────────
// deduplicação por par (src, seq) — circular, last-write-wins

typedef struct {
  uint8_t src;
  uint8_t seq;
  bool valid;
} SeenEntry;
static SeenEntry s_seen[FETLINK_SEEN_CACHE];
static uint8_t s_seen_head = 0;

static bool seen_check(uint8_t src, uint8_t seq) {
  for (int i = 0; i < FETLINK_SEEN_CACHE; i++) {
    if (s_seen[i].valid && s_seen[i].src == src && s_seen[i].seq == seq)
      return true;
  }
  return false;
}

static void seen_mark(uint8_t src, uint8_t seq) {
  s_seen[s_seen_head] = { src, seq, true };
  s_seen_head = (s_seen_head + 1) % FETLINK_SEEN_CACHE;
}

// ── checksum XOR (VER até último byte do PAYLOAD) ────────────

static uint8_t calc_csum(const uint8_t* data, uint8_t len) {
  uint8_t csum = 0;
  for (uint8_t i = 0; i < len; i++) csum ^= data[i];
  return csum;
}

// ── send ──────────────────────────────────────────────────────

static bool send_frame(uint8_t dst, uint8_t type, uint8_t seq,
                       const uint8_t* payload, uint8_t plen) {
  uint8_t frame[FETLINK_FRAME_MAX];
  frame[0] = FETLINK_STX;
  frame[1] = FETLINK_VERSION;
  frame[2] = s_node_id;
  frame[3] = dst;
  frame[4] = type;
  frame[5] = seq;
  frame[6] = plen;
  if (plen > 0) memcpy(&frame[7], payload, plen);
  // csum cobre VER..PAYLOAD (frame[1] até frame[6+plen])
  frame[7 + plen] = calc_csum(&frame[1], 6 + plen);
  return fetlink_transport_write(frame, 8 + plen);
}

bool fetlink_send(uint8_t dst, FetLinkType type,
                  const uint8_t* data, uint16_t len) {
  if (len <= FETLINK_PAYLOAD_MAX) {
    // frame único — SEQ normal
    bool ok = send_frame(dst, type, s_seq, data, (uint8_t)len);
    s_seq++;
    return ok;
  }

  // fragmentação — SEQ vira índice de fragmento
  uint8_t frag_idx = 0;
  uint16_t offset = 0;

  while (offset < len) {
    uint16_t chunk = len - offset;
    if (chunk > FETLINK_PAYLOAD_MAX) chunk = FETLINK_PAYLOAD_MAX;
    bool is_last = (offset + chunk >= len);

    uint8_t seq = FLINK_SEQ_MAKE_FRAG(frag_idx, is_last);
    if (!send_frame(dst, type, seq, data + offset, (uint8_t)chunk))
      return false;

    offset += chunk;
    frag_idx++;
    delay(20);  // backoff mínimo entre fragmentos
  }
  return true;
}

// ── parser ────────────────────────────────────────────────────

static void parser_reset() {
  s_parser.state = FLINK_PARSE_IDLE;
  s_parser.pos = 0;
}

static void process_complete_frame() {
  // layout: [0]=STX [1]=VER [2]=SRC [3]=DST [4]=TYPE [5]=SEQ [6]=LEN
  //         [7..7+LEN-1]=PAYLOAD  [7+LEN]=CSUM
  uint8_t ver = s_parser.buf[1];
  uint8_t src = s_parser.buf[2];
  uint8_t dst = s_parser.buf[3];
  uint8_t type = s_parser.buf[4];
  uint8_t seq = s_parser.buf[5];
  uint8_t plen = s_parser.buf[6];

  // versão
  if (ver != FETLINK_VERSION) {
    Serial.printf("[FetLink] VER inválido: 0x%02X\n", ver);
    return;
  }

  // 🔴 REMOVIDO: A trava antiga que matava a osmose ficava aqui:
  // if (dst != s_node_id && dst != 0x00) return;

  // checksum (Valida ANTES de pensar em propagar)
  uint8_t expected = calc_csum(&s_parser.buf[1], 6 + plen);
  uint8_t actual = s_parser.buf[7 + plen];
  if (expected != actual) {
    Serial.printf("[FetLink] CSUM erro src=0x%02X exp=0x%02X got=0x%02X\n",
                  src, expected, actual);
    return;
  }

  const uint8_t* payload = &s_parser.buf[7];
  bool is_for_me = (dst == s_node_id || dst == 0x00);

  // ── frame único ───────────────────────────────────────────
  if (!FLINK_SEQ_IS_FRAG(seq)) {
    // 1. Cache de Duplicatas (O Motor da Osmose)
    if (seen_check(src, seq)) return;  // Já vi, ignora e não causa tempestade de broadcast
    seen_mark(src, seq);               // Anota que viu

    // 2. Lógica de Relay (Nó Repetidor)
    if (!is_for_me) {
      Serial.printf("[FetLink] Osmose: Relay de src=0x%02X para dst=0x%02X\n", src, dst);
      // Reenvia o frame original bruto que está no buffer (8 bytes de header/csum + payload)
      fetlink_transport_write(s_parser.buf, 8 + plen);
      return;
    }

    // 3. É para mim (ou broadcast) -> Entrega para a Camada Lógica (device_net)
    if (s_on_receive) s_on_receive(src, type, payload, plen);
    return;
  }

  // ── fragmento — reassembly ────────────────────────────────
  // Se for um fragmento (ex: app .fvm viajando pela rede), nós também fazemos o relay bruto dele!
  if (!is_for_me) {
    if (seen_check(src, seq)) return;  // Usa SRC + SEQ (que contém o ID do fragmento) para cache
    seen_mark(src, seq);

    Serial.printf("[FetLink] Osmose Frag: Relay src=0x%02X dst=0x%02X seq=0x%02X\n", src, dst, seq);
    fetlink_transport_write(s_parser.buf, 8 + plen);
    return;
  }

  // Se chegou aqui, é um fragmento E é para mim (ou broadcast). Fazemos a montagem:
  uint8_t idx = FLINK_SEQ_FRAG_IDX(seq);
  bool is_last = FLINK_SEQ_IS_LAST(seq);

  if (idx == 0) {
    // primeiro fragmento — (re)inicia buffer e rastreamento
    s_reassembly.active = true;
    s_reassembly.src = src;
    s_reassembly.type = type;
    s_reassembly.total_len = 0;
    s_reassembly.last_activity_ms = millis();
    s_reassembly.received_mask = 0;
    s_reassembly.last_frag_idx = 0xFF;  // ainda não sabemos qual é o last
  }

  if (!s_reassembly.active || s_reassembly.src != src) return;

  uint16_t offset = (uint16_t)idx * FETLINK_PAYLOAD_MAX;
  if (offset + plen > FETLINK_FRAG_BUF_MAX) {
    Serial.println("[FetLink] Reassembly overflow — abortando");
    s_reassembly.active = false;
    return;
  }

  memcpy(s_reassembly.buf + offset, payload, plen);
  s_reassembly.total_len = offset + plen;
  s_reassembly.last_activity_ms = millis();

  // Marca este fragmento como recebido (idx <= 63 garantido pelo campo de 6 bits)
  s_reassembly.received_mask |= ((uint64_t)1 << idx);
  if (is_last) {
    s_reassembly.last_frag_idx = idx;
  }

  if (is_last) {
    // Verifica integridade: todos os fragmentos 0..idx devem ter chegado
    uint64_t expected_mask = (idx < 63)
      ? (((uint64_t)1 << (idx + 1)) - 1)  // bits 0..idx setados
      : UINT64_MAX;                         // idx==63: todos 64 bits

    if ((s_reassembly.received_mask & expected_mask) != expected_mask) {
      // Há buracos — fragmento(s) perdido(s). Aborta para não salvar FVM corrompido.
      uint64_t missing = expected_mask & ~s_reassembly.received_mask;
      Serial.printf("[FetLink] INTEGRIDADE FALHOU — fragmentos perdidos: 0x%llX — descartando\n",
                    (unsigned long long)missing);
      s_reassembly.active = false;
      return;
    }

    s_reassembly.active = false;
    if (s_on_receive)
      s_on_receive(s_reassembly.src, s_reassembly.type,
                   s_reassembly.buf, s_reassembly.total_len);
  }
}

void fetlink_feed(uint8_t byte) {
  uint32_t now = millis();

  // timeouts — retorna ao IDLE
  if (s_parser.state != FLINK_PARSE_IDLE) {
    if ((now - s_parser.last_byte_ms) > FETLINK_INTERBYTE_TIMEOUT || (now - s_parser.frame_start_ms) > FETLINK_FRAME_TIMEOUT) {
      Serial.println("[FetLink] Timeout de frame — resetando parser");
      parser_reset();
    }
  }

  s_parser.last_byte_ms = now;

  switch (s_parser.state) {

    case FLINK_PARSE_IDLE:
      if (byte == FETLINK_STX) {
        s_parser.buf[0] = byte;
        s_parser.pos = 1;
        s_parser.frame_start_ms = now;
        s_parser.state = FLINK_PARSE_HEADER;
      }
      break;

    case FLINK_PARSE_HEADER:
      s_parser.buf[s_parser.pos++] = byte;
      // header completo: STX(1) + VER+SRC+DST+TYPE+SEQ+LEN(6) = 7 bytes
      if (s_parser.pos == FETLINK_HEADER_SIZE) {
        uint8_t plen = s_parser.buf[6];
        if (plen > FETLINK_PAYLOAD_MAX) {
          // LEN inválido
          Serial.printf("[FetLink] LEN inválido: %d\n", plen);
          parser_reset();
        } else if (plen == 0) {
          // sem payload — vai direto para verificação
          s_parser.buf[7] = 0;  // csum virá no próximo byte
          s_parser.state = FLINK_PARSE_PAYLOAD;
        } else {
          s_parser.state = FLINK_PARSE_PAYLOAD;
        }
      }
      break;

    case FLINK_PARSE_PAYLOAD:
      {
        s_parser.buf[s_parser.pos++] = byte;
        uint8_t plen = s_parser.buf[6];
        // frame completo = 7 bytes header + plen payload + 1 byte csum
        if (s_parser.pos == (uint8_t)(FETLINK_HEADER_SIZE + plen + 1)) {
          process_complete_frame();
          parser_reset();
        }
        break;
      }

    default:
      parser_reset();
      break;
  }
}

void fetlink_tick() {
  // reassembly timeout
  if (s_reassembly.active && (millis() - s_reassembly.last_activity_ms) > FETLINK_REASSEMBLY_TIMEOUT) {
    Serial.println("[FetLink] Reassembly timeout — descartando fragmentos");
    s_reassembly.active = false;
  }
}

void fetlink_init(uint8_t node_id, FetLinkReceiveCb on_receive) {
  s_node_id = node_id;
  s_on_receive = on_receive;
  s_seq = 0;
  memset(&s_parser, 0, sizeof(s_parser));
  memset(&s_reassembly, 0, sizeof(s_reassembly));
  memset(s_seen, 0, sizeof(s_seen));
  s_parser.state = FLINK_PARSE_IDLE;
  Serial.printf("[FetLink] Iniciado — node_id: 0x%02X\n", node_id);
}

uint8_t fetlink_node_id() {
  return s_node_id;
}