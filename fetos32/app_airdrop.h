#pragma once
// app_airdrop.h
// AirDrop v1 — transferência de arquivos .fvm via ESP-NOW/FetLink
//
// Protocolo:
//   SENDER → broadcast FLINK_PUB "ad:offer" value=offer_id  (periódico)
//   RECEIVER → broadcast FLINK_PUB "ad:get"  value=offer_id  (aceitar)
//   SENDER → FLINK_APP payload:
//             [offer_id:1][name_len:1][filename:name_len][fvm_data:...]
//   on_fetlink_receive → airdrop_on_receive_file() grava direto no LittleFS
//   RECEIVER → broadcast FLINK_PUB "ad:done" value=offer_id  (confirmação)
//
// Integração no kernel:
//   1. system.cpp: adicionar app_airdrop_setup() em system_init()
//   2. fetlink_dual.cpp: em on_fetlink_receive(), adicionar:
//        else if (type == FLINK_APP) { airdrop_on_receive_file(data, len); }
//   3. fetlink.h: FETLINK_FRAG_BUF_MAX de 512 → 4096 (suporta arquivos até 4KB)
//      OU implementar reassembly direto no FS (ver airdrop_on_receive_file)

#include "app.h"
#include "event.h"
#include <stdint.h>

// ── Limites ───────────────────────────────────────────────────
#define AIRDROP_MAX_OFFERS   8    // ofertas simultâneas visíveis
#define AIRDROP_OFFER_TTL_MS 3000 // oferta some se não renovada em 3s
#define AIRDROP_OFFER_INTERVAL_MS 800  // sender anuncia a cada 800ms
#define AIRDROP_FILENAME_MAX 32

// ── Estrutura de oferta ───────────────────────────────────────
typedef struct {
  uint8_t  offer_id;                    // ID único desta oferta
  uint8_t  sender_node;                 // node_id do remetente
  char     filename[AIRDROP_FILENAME_MAX]; // nome do arquivo
  uint32_t filesize;                    // tamanho em bytes
  uint32_t last_seen_ms;                // timestamp da última renovação
  bool     active;
} AirDropOffer;

// ── API pública ───────────────────────────────────────────────

extern App app_airdrop;

void app_airdrop_setup();
void app_airdrop_on_enter();
void app_airdrop_on_exit();
void app_airdrop_on_event(Event* e);
void app_airdrop_render();

// Chamada pelo fetlink_dual.cpp quando chega FLINK_APP
// ou FLINK_PUB com topic "ad:*"
void airdrop_on_receive_file(uint8_t sender, const uint8_t* data, uint16_t len);
void airdrop_on_pub(uint8_t sender, const char* topic, int32_t value);

// Chamada por on_fetlink_receive para PUBs "ad:offer" com payload completo
void airdrop_on_receive_offer_raw(uint8_t sender,
                                  const uint8_t* payload,
                                  uint16_t plen);