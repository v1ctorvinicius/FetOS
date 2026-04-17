// app_airdrop.cpp
// AirDrop v1 — transferência de arquivos .fvm via ESP-NOW/FetLink
#include "app_airdrop.h"
#include "system.h"
#include "oled.h"
#include "display_hal.h"
#include "fetlink.h"
#include "fetlink_dual.h"
#include "button_gesture.h"
#include "Arduino.h"
#include "LittleFS.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

App app_airdrop;

// ═══════════════════════════════════════════════════════════════
// ESTADO INTERNO
// ═══════════════════════════════════════════════════════════════

enum AirDropMode {
  MODE_MENU,          // tela inicial: Enviar / Receber
  MODE_SEND,          // lista de arquivos locais para enviar
  MODE_SEND_CONFIRM,  // confirmação antes de transmitir
  MODE_SENDING,       // transmissão em andamento
  MODE_RECV,          // escuta passiva, lista de ofertas
  MODE_RECV_DONE      // download concluído
};

static AirDropMode s_mode = MODE_MENU;
static int s_sel = 0;  // índice selecionado na lista atual
static bool s_redraw = true;

// ── Arquivos locais (sender) ──────────────────────────────────
#define LOCAL_FILES_MAX 12
static char s_local_files[LOCAL_FILES_MAX][AIRDROP_FILENAME_MAX];
static size_t s_local_sizes[LOCAL_FILES_MAX];
static int s_local_count = 0;

// ── Estado de envio ───────────────────────────────────────────
static uint8_t s_send_offer_id = 0;
static uint32_t s_send_last_ann = 0;  // ms do último anúncio
static bool s_sending_active = false;
static uint32_t s_send_filesize = 0;

// ── Ofertas recebidas (receiver) ─────────────────────────────
static AirDropOffer s_offers[AIRDROP_MAX_OFFERS];

// ── Estado de recebimento ─────────────────────────────────────
static bool s_recv_active = false;
static char s_recv_filename[AIRDROP_FILENAME_MAX] = { 0 };
static bool s_recv_done = false;
static uint8_t s_recv_offer_id = 0;

// ═══════════════════════════════════════════════════════════════
// UTILIDADES
// ═══════════════════════════════════════════════════════════════

static void scan_local_files() {
  s_local_count = 0;
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) return;

  File f = root.openNextFile();
  while (f && s_local_count < LOCAL_FILES_MAX) {
    String name = f.name();
    if (name.endsWith(".fvm")) {
      // Armazena só o nome, sem barra inicial
      const char* raw = name.c_str();
      if (raw[0] == '/') raw++;
      strncpy(s_local_files[s_local_count], raw, AIRDROP_FILENAME_MAX - 1);
      s_local_files[s_local_count][AIRDROP_FILENAME_MAX - 1] = '\0';
      s_local_sizes[s_local_count] = f.size();
      s_local_count++;
    }
    f.close();
    f = root.openNextFile();
  }
  root.close();
}

// Gera um offer_id baseado no node_id + contador local
static uint8_t next_offer_id() {
  static uint8_t counter = 0;
  counter++;
  return (fetlink_node_id() & 0x0F) | ((counter & 0x0F) << 4);
}

// Expira ofertas antigas
static void expire_offers() {
  uint32_t now = millis();
  for (int i = 0; i < AIRDROP_MAX_OFFERS; i++) {
    if (s_offers[i].active && (now - s_offers[i].last_seen_ms) > AIRDROP_OFFER_TTL_MS) {
      s_offers[i].active = false;
    }
  }
}

static int count_active_offers() {
  int n = 0;
  for (int i = 0; i < AIRDROP_MAX_OFFERS; i++)
    if (s_offers[i].active) n++;
  return n;
}

// Retorna o índice da N-ésima oferta ativa (para mapear s_sel → offer)
static int nth_active_offer(int n) {
  int count = 0;
  for (int i = 0; i < AIRDROP_MAX_OFFERS; i++) {
    if (s_offers[i].active) {
      if (count == n) return i;
      count++;
    }
  }
  return -1;
}

// ─── Envio de anúncio de oferta via FLINK_PUB "ad:offer" ─────
// O payload do PUB só carrega um int16. Usamos offer_id como
// índice de lookup — o receiver usa "ad:info" para obter detalhes.
// MAS net:send não está disponível aqui (C++ nativo).
// Chamamos fetlink_send direto.
//
// Formato do PUB "ad:offer":
//   topic_len(1) + "ad:offer"(8) + data_type(1=int16) + value_be(2)
static void send_offer_announcement(uint8_t offer_id,
                                    const char* filename,
                                    uint32_t filesize) {
  // Payload custom: "ad:offer\0" + offer_id + name_len + filename + size(4)
  // Usamos FLINK_PUB mas com um payload estendido que o receiver decodifica
  // em airdrop_on_pub. name_len+filename cabem em 128 bytes de payload.
  uint8_t buf[128];
  uint8_t pos = 0;

  // Formato: [topic_len:1][topic:N][0x03:1=custom_type][offer_id:1][name_len:1][name:M][size_be:4]
  const char* topic = "ad:offer";
  uint8_t topic_len = (uint8_t)strlen(topic);
  buf[pos++] = topic_len;
  memcpy(&buf[pos], topic, topic_len);
  pos += topic_len;

  buf[pos++] = 0x03;  // custom type: airdrop offer descriptor

  uint8_t name_len = (uint8_t)strlen(filename);
  if (name_len > 30) name_len = 30;

  buf[pos++] = offer_id;
  buf[pos++] = name_len;
  memcpy(&buf[pos], filename, name_len);
  pos += name_len;

  // filesize big-endian 4 bytes
  buf[pos++] = (uint8_t)((filesize >> 24) & 0xFF);
  buf[pos++] = (uint8_t)((filesize >> 16) & 0xFF);
  buf[pos++] = (uint8_t)((filesize >> 8) & 0xFF);
  buf[pos++] = (uint8_t)(filesize & 0xFF);

  fetlink_send(0x00, FLINK_PUB, buf, pos);
}

// ─── Envia "ad:get" para solicitar um arquivo ────────────────
static void send_get_request(uint8_t offer_id) {
  uint8_t buf[16];
  uint8_t pos = 0;
  const char* topic = "ad:get";
  uint8_t topic_len = (uint8_t)strlen(topic);
  buf[pos++] = topic_len;
  memcpy(&buf[pos], topic, topic_len);
  pos += topic_len;
  buf[pos++] = 0x02;  // int16
  buf[pos++] = 0x00;
  buf[pos++] = offer_id;
  fetlink_send(0x00, FLINK_PUB, buf, pos);
}

// ─── Envia "ad:done" para confirmar recebimento ──────────────
static void send_done(uint8_t offer_id) {
  uint8_t buf[16];
  uint8_t pos = 0;
  const char* topic = "ad:done";
  uint8_t topic_len = (uint8_t)strlen(topic);
  buf[pos++] = topic_len;
  memcpy(&buf[pos], topic, topic_len);
  pos += topic_len;
  buf[pos++] = 0x02;
  buf[pos++] = 0x00;
  buf[pos++] = offer_id;
  fetlink_send(0x00, FLINK_PUB, buf, pos);
}

// ─── Transmite o arquivo via FLINK_APP ───────────────────────
// Formato do payload FLINK_APP:
//   [offer_id:1][name_len:1][filename:name_len][fvm_data:...]
// fetlink_send() fragmenta automaticamente.
static bool send_file(uint8_t offer_id, const char* filename) {
  char path[AIRDROP_FILENAME_MAX + 2];
  snprintf(path, sizeof(path), "/%s", filename);

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[AirDrop] ERRO: nao encontrou %s\n", path);
    return false;
  }

  uint32_t filesize = f.size();
  s_send_filesize = filesize;

  // Monta payload: header + dados do arquivo
  // FETLINK_FRAG_BUF_MAX deve ser >= filesize+2+name_len
  // Para v1 limitamos a 4KB (ajuste FETLINK_FRAG_BUF_MAX=4096 em fetlink.h)
  uint8_t name_len = (uint8_t)strlen(filename);
  size_t header_size = 2 + name_len;
  size_t total = header_size + filesize;

  // Limite de segurança: FETLINK_FRAG_BUF_MAX no receptor define o
  // tamanho máximo de reassembly. Arquivos maiores seriam descartados
  // com "Reassembly overflow". Alertamos antes de sequer tentar.
  if (total > FETLINK_FRAG_BUF_MAX) {
    f.close();
    Serial.printf("[AirDrop] ERRO: arquivo muito grande (%u bytes > FETLINK_FRAG_BUF_MAX %d)\n",
                  (unsigned)total, FETLINK_FRAG_BUF_MAX);
    return false;
  }

  uint8_t* buf = (uint8_t*)malloc(total);
  if (!buf) {
    f.close();
    Serial.println("[AirDrop] ERRO: sem RAM para buffer de envio");
    return false;
  }

  buf[0] = offer_id;
  buf[1] = name_len;
  memcpy(&buf[2], filename, name_len);
  f.read(&buf[header_size], filesize);
  f.close();

  Serial.printf("[AirDrop] Enviando %s (%lu bytes) offer_id=0x%02X\n",
                filename, (unsigned long)filesize, offer_id);

  bool ok = fetlink_send(0x00, FLINK_APP, buf, (uint16_t)total);  // total <= FRAG_BUF_MAX <= 65535, cast seguro
  free(buf);
  return ok;
}

// ═══════════════════════════════════════════════════════════════
// CALLBACKS DO FETLINK (chamados por fetlink_dual.cpp)
// ═══════════════════════════════════════════════════════════════

// Chamado quando chega FLINK_PUB com topic "ad:*"
// Decodifica o payload customizado do send_offer_announcement.
void airdrop_on_pub(uint8_t sender, const char* topic, int32_t value) {
  if (strcmp(topic, "ad:offer") == 0) {
    // value aqui é o offer_id extraído pelo decodificador padrão,
    // mas o payload completo precisaria ser parseado em on_fetlink_receive.
    // A decodificação completa está em airdrop_on_receive_pub_raw().
    // Este fallback só trata o caso em que o roteador padrão extraiu int16.
    (void)sender;
    (void)value;
    return;
  }

  if (strcmp(topic, "ad:get") == 0) {
    // Um receiver quer o arquivo que estamos anunciando
    uint8_t requested_id = (uint8_t)(value & 0xFF);
    if (s_sending_active && requested_id == s_send_offer_id) {
      Serial.printf("[AirDrop] Pedido de download recebido para offer 0x%02X\n", requested_id);
      send_file(s_send_offer_id, s_local_files[s_sel]);
    }
    return;
  }

  if (strcmp(topic, "ad:done") == 0) {
    uint8_t done_id = (uint8_t)(value & 0xFF);
    if (s_sending_active && done_id == s_send_offer_id) {
      Serial.printf("[AirDrop] Confirmação de recebimento para offer 0x%02X\n", done_id);
      s_sending_active = false;
      s_mode = MODE_MENU;
      s_redraw = true;
    }
    return;
  }
}

// Chamado por on_fetlink_receive() quando type == FLINK_APP
// Salva o arquivo diretamente no LittleFS.
// Formato esperado: [offer_id:1][name_len:1][filename:name_len][data:...]
void airdrop_on_receive_file(uint8_t sender, const uint8_t* data, uint16_t len) {
  if (len < 3) return;

  uint8_t offer_id = data[0];
  uint8_t name_len = data[1];

  if (name_len == 0 || name_len > 30 || (uint16_t)(2 + name_len) >= len) {
    Serial.println("[AirDrop] Pacote FLINK_APP malformado");
    return;
  }

  char filename[AIRDROP_FILENAME_MAX];
  memcpy(filename, &data[2], name_len);
  filename[name_len] = '\0';

  const uint8_t* file_data = &data[2 + name_len];
  uint16_t file_len = len - 2 - name_len;

  Serial.printf("[AirDrop] Recebendo arquivo: '%s' (%d bytes) de nó 0x%02X\n",
                filename, file_len, sender);

  // Salva com prefixo "received_" para não sobrescrever apps locais sem confirmação
  char save_path[AIRDROP_FILENAME_MAX + 12];
  snprintf(save_path, sizeof(save_path), "/received_%s", filename);

  File f = LittleFS.open(save_path, "w");
  if (!f) {
    Serial.printf("[AirDrop] ERRO: nao conseguiu abrir %s para escrita\n", save_path);
    return;
  }

  f.write(file_data, file_len);
  f.close();

  Serial.printf("[AirDrop] Salvo em %s\n", save_path);

  // Registra o app no launcher sem precisar reiniciar.
  // system_discover_fs_apps() já ignora paths duplicados.
  // É seguro chamar aqui: o AirDrop é app nativo (não FVM),
  // então is_system_mutating não bloqueia a chamada.
  Serial.println("[AirDrop] Registrando app recebido no launcher...");
  system_discover_fs_apps();

  // Atualiza estado do app
  strncpy(s_recv_filename, filename, AIRDROP_FILENAME_MAX - 1);
  s_recv_filename[AIRDROP_FILENAME_MAX - 1] = '\0';
  s_recv_done = true;
  s_recv_offer_id = offer_id;
  s_recv_active = false;

  if (s_mode == MODE_RECV) {
    s_mode = MODE_RECV_DONE;
    s_redraw = true;
  }

  // Confirma recebimento para o sender
  send_done(offer_id);
}

// Chamado por on_fetlink_receive para PUBs "ad:offer" com payload completo
// (distingue do roteador padrão que só extrai int16)
void airdrop_on_receive_offer_raw(uint8_t sender, const uint8_t* payload, uint16_t plen) {
  // payload: [topic_len:1][topic:N][0x03:1][offer_id:1][name_len:1][name:M][size:4]
  // O topic já foi identificado antes de chamar esta função,
  // então recebemos o payload da mensagem FLINK_PUB original.
  //
  // Mas on_fetlink_receive já extraiu o topic e chama esta função apenas
  // com os bytes após o data_type.
  // Aqui: payload = [offer_id:1][name_len:1][name:M][size:4]
  if (plen < 3) return;

  uint8_t offer_id = payload[0];
  uint8_t name_len = payload[1];
  if (name_len == 0 || name_len > 30 || (uint16_t)(2 + name_len + 4) > plen) return;

  char filename[AIRDROP_FILENAME_MAX];
  memcpy(filename, &payload[2], name_len);
  filename[name_len] = '\0';

  uint32_t filesize = ((uint32_t)payload[2 + name_len] << 24)
                      | ((uint32_t)payload[2 + name_len + 1] << 16)
                      | ((uint32_t)payload[2 + name_len + 2] << 8)
                      | (uint32_t)payload[2 + name_len + 3];

  // Encontra ou cria slot para esta oferta
  uint32_t now = millis();
  int slot = -1;
  for (int i = 0; i < AIRDROP_MAX_OFFERS; i++) {
    if (s_offers[i].active && s_offers[i].offer_id == offer_id && s_offers[i].sender_node == sender) {
      slot = i;  // renovação
      break;
    }
  }
  if (slot == -1) {
    for (int i = 0; i < AIRDROP_MAX_OFFERS; i++) {
      if (!s_offers[i].active) {
        slot = i;
        break;
      }
    }
  }
  if (slot == -1) slot = 0;  // sobrescreve o mais antigo

  s_offers[slot].active = true;
  s_offers[slot].offer_id = offer_id;
  s_offers[slot].sender_node = sender;
  s_offers[slot].filesize = filesize;
  s_offers[slot].last_seen_ms = now;
  strncpy(s_offers[slot].filename, filename, AIRDROP_FILENAME_MAX - 1);
  s_offers[slot].filename[AIRDROP_FILENAME_MAX - 1] = '\0';

  if (s_mode == MODE_RECV) s_redraw = true;
}

// ═══════════════════════════════════════════════════════════════
// RENDER
// ═══════════════════════════════════════════════════════════════

static void render_menu() {
  hal_clear(DISPLAY_DEFAULT_ID);
  hal_text_center(DISPLAY_DEFAULT_ID, 0, "AIRDROP v1", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 12, 128, 1, true);

  if (s_sel == 0) {
    hal_text_invert(DISPLAY_DEFAULT_ID, 10, 22, "ENVIAR", 1);
    hal_text(DISPLAY_DEFAULT_ID, 10, 38, "RECEBER", 1);
  } else {
    hal_text(DISPLAY_DEFAULT_ID, 10, 22, "ENVIAR", 1);
    hal_text_invert(DISPLAY_DEFAULT_ID, 10, 38, "RECEBER", 1);
  }

  hal_text_center(DISPLAY_DEFAULT_ID, 55, "Tap:mudar Long:entrar", 1);
  hal_flush(DISPLAY_DEFAULT_ID);
}

static void render_send() {
  hal_clear(DISPLAY_DEFAULT_ID);
  hal_text_center(DISPLAY_DEFAULT_ID, 0, "ENVIAR ARQUIVO", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 12, 128, 1, true);

  if (s_local_count == 0) {
    hal_text_center(DISPLAY_DEFAULT_ID, 30, "Nenhum .fvm", 1);
  } else {
    // Mostra até 4 arquivos com scroll simples
    int start = s_sel > 3 ? s_sel - 3 : 0;
    for (int i = 0; i < 4 && (start + i) < s_local_count; i++) {
      int idx = start + i;
      int y = 16 + i * 10;

      // Nome truncado a 18 chars + tamanho
      char line[24];
      size_t sz = s_local_sizes[idx];
      if (sz >= 1024)
        snprintf(line, sizeof(line), "%-14.14s%3dK", s_local_files[idx], (int)(sz / 1024));
      else
        snprintf(line, sizeof(line), "%-14.14s%3db", s_local_files[idx], (int)sz);

      if (idx == s_sel)
        hal_text_invert(DISPLAY_DEFAULT_ID, 0, y, line, 1);
      else
        hal_text(DISPLAY_DEFAULT_ID, 0, y, line, 1);
    }
  }

  hal_text_center(DISPLAY_DEFAULT_ID, 55, "Tap:prox Long:enviar", 1);
  hal_flush(DISPLAY_DEFAULT_ID);
}

static void render_send_confirm() {
  hal_clear(DISPLAY_DEFAULT_ID);
  hal_text_center(DISPLAY_DEFAULT_ID, 0, "CONFIRMAR ENVIO", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 12, 128, 1, true);

  char line[AIRDROP_FILENAME_MAX];
  snprintf(line, sizeof(line), "%s", s_local_files[s_sel]);
  hal_text_center(DISPLAY_DEFAULT_ID, 22, line, 1);
  hal_text_center(DISPLAY_DEFAULT_ID, 34, "para TODOS os nos", 1);

  hal_text_center(DISPLAY_DEFAULT_ID, 48, "Long: enviar", 1);
  hal_text_center(DISPLAY_DEFAULT_ID, 56, "Tap:  cancelar", 1);
  hal_flush(DISPLAY_DEFAULT_ID);
}

static void render_sending() {
  hal_clear(DISPLAY_DEFAULT_ID);
  hal_text_center(DISPLAY_DEFAULT_ID, 0, "TRANSMITINDO...", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 12, 128, 1, true);

  hal_text_center(DISPLAY_DEFAULT_ID, 22, s_local_files[s_sel], 1);

  char info[24];
  snprintf(info, sizeof(info), "No: 0x%02X", fetlink_node_id());
  hal_text_center(DISPLAY_DEFAULT_ID, 36, info, 1);

  snprintf(info, sizeof(info), "%lu bytes", (unsigned long)s_send_filesize);
  hal_text_center(DISPLAY_DEFAULT_ID, 46, info, 1);

  hal_text_center(DISPLAY_DEFAULT_ID, 56, "Aguardando ACK...", 1);
  hal_flush(DISPLAY_DEFAULT_ID);
}

static void render_recv() {
  hal_clear(DISPLAY_DEFAULT_ID);
  hal_text_center(DISPLAY_DEFAULT_ID, 0, "OFERTAS NA REDE", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 12, 128, 1, true);

  expire_offers();
  int active = count_active_offers();

  if (active == 0) {
    hal_text_center(DISPLAY_DEFAULT_ID, 30, "Aguardando...", 1);
  } else {
    // Mostra até 4 ofertas
    int shown = 0;
    for (int i = 0; i < AIRDROP_MAX_OFFERS && shown < 4; i++) {
      if (!s_offers[i].active) continue;
      int y = 16 + shown * 10;

      char line[24];
      size_t sz = s_offers[i].filesize;
      if (sz >= 1024)
        snprintf(line, sizeof(line), "%02X %-10.10s%3dK",
                 s_offers[i].sender_node, s_offers[i].filename, (int)(sz / 1024));
      else
        snprintf(line, sizeof(line), "%02X %-10.10s%3db",
                 s_offers[i].sender_node, s_offers[i].filename, (int)sz);

      if (shown == s_sel)
        hal_text_invert(DISPLAY_DEFAULT_ID, 0, y, line, 1);
      else
        hal_text(DISPLAY_DEFAULT_ID, 0, y, line, 1);

      shown++;
    }
  }

  hal_text_center(DISPLAY_DEFAULT_ID, 55, "Tap:prox Long:baixar", 1);
  hal_flush(DISPLAY_DEFAULT_ID);
}

static void render_recv_done() {
  hal_clear(DISPLAY_DEFAULT_ID);
  hal_text_center(DISPLAY_DEFAULT_ID, 0, "DOWNLOAD OK!", 1);
  hal_rect(DISPLAY_DEFAULT_ID, 0, 12, 128, 1, true);

  char line[AIRDROP_FILENAME_MAX + 10];
  snprintf(line, sizeof(line), "received_%s", s_recv_filename);
  hal_text_center(DISPLAY_DEFAULT_ID, 25, line, 1);
  hal_text_center(DISPLAY_DEFAULT_ID, 38, "salvo no FS", 1);
  hal_text_center(DISPLAY_DEFAULT_ID, 52, "Tap: voltar ao menu", 1);
  hal_flush(DISPLAY_DEFAULT_ID);
}

// ═══════════════════════════════════════════════════════════════
// EVENTOS
// ═══════════════════════════════════════════════════════════════

void app_airdrop_on_event(Event* e) {
  if (!e || !e->has_payload) return;
  int gesture = e->payload.value;

  // Double tap: sai do app de qualquer modo
  if (gesture == GESTURE_DOUBLE_TAP) {
    system_set_app(&launcher_app);
    return;
  }

  switch (s_mode) {

    case MODE_MENU:
      if (gesture == GESTURE_TAP) {
        s_sel = 1 - s_sel;
        s_redraw = true;
      }
      if (gesture == GESTURE_LONG_PRESS) {
        if (s_sel == 0) {
          scan_local_files();
          s_sel = 0;
          s_mode = MODE_SEND;
        } else {
          memset(s_offers, 0, sizeof(s_offers));
          s_sel = 0;
          s_recv_done = false;
          s_recv_active = false;
          s_mode = MODE_RECV;
        }
        s_redraw = true;
      }
      break;

    case MODE_SEND:
      if (gesture == GESTURE_TAP) {
        if (s_local_count > 0) {
          s_sel = (s_sel + 1) % s_local_count;
          s_redraw = true;
        }
      }
      if (gesture == GESTURE_LONG_PRESS) {
        if (s_local_count > 0) {
          s_mode = MODE_SEND_CONFIRM;
          s_redraw = true;
        }
      }
      break;

    case MODE_SEND_CONFIRM:
      if (gesture == GESTURE_TAP) {
        // Cancelar: volta para a lista
        s_mode = MODE_SEND;
        s_redraw = true;
      }
      if (gesture == GESTURE_LONG_PRESS) {
        // Confirma: começa a transmitir
        s_send_offer_id = next_offer_id();
        s_send_last_ann = 0;  // força anúncio imediato no próximo update
        s_sending_active = true;
        s_mode = MODE_SENDING;
        s_redraw = true;
      }
      break;

    case MODE_SENDING:
      // Tap cancela o envio
      if (gesture == GESTURE_TAP) {
        s_sending_active = false;
        s_mode = MODE_MENU;
        s_sel = 0;
        s_redraw = true;
      }
      break;

    case MODE_RECV:
      {
        expire_offers();
        int active = count_active_offers();
        if (gesture == GESTURE_TAP) {
          if (active > 0) {
            s_sel = (s_sel + 1) % active;
            s_redraw = true;
          }
        }
        if (gesture == GESTURE_LONG_PRESS) {
          int oi = nth_active_offer(s_sel);
          if (oi >= 0) {
            s_recv_offer_id = s_offers[oi].offer_id;
            s_recv_active = true;
            strncpy(s_recv_filename, s_offers[oi].filename, AIRDROP_FILENAME_MAX - 1);
            s_recv_filename[AIRDROP_FILENAME_MAX - 1] = '\0';
            send_get_request(s_recv_offer_id);
            Serial.printf("[AirDrop] Pedido de download: '%s' offer=0x%02X\n",
                          s_recv_filename, s_recv_offer_id);
            s_redraw = true;
          }
        }
        break;
      }

    case MODE_RECV_DONE:
      if (gesture == GESTURE_TAP) {
        s_mode = MODE_MENU;
        s_sel = 0;
        s_redraw = true;
      }
      break;
  }
}

// ═══════════════════════════════════════════════════════════════
// UPDATE (chamado pelo scheduler via fvm_update ou app.update)
// Aqui ficam as ações periódicas: re-anunciar oferta a cada 800ms.
// ═══════════════════════════════════════════════════════════════

static void airdrop_update(void* ctx) {
  bool needs_redraw = false;

  if (s_sending_active) {
    uint32_t now = millis();
    if (now - s_send_last_ann >= AIRDROP_OFFER_INTERVAL_MS) {
      s_send_last_ann = now;
      send_offer_announcement(s_send_offer_id,
                              s_local_files[s_sel],
                              s_local_sizes[s_sel]);
      needs_redraw = true;  // atualiza tela de "TRANSMITINDO..."
    }
  }

  if (s_mode == MODE_RECV) {
    expire_offers();
    // Se alguma oferta mudou (expirou ou chegou nova), redesenha
    static int last_active = -1;
    int current_active = count_active_offers();
    if (current_active != last_active) {
      last_active = current_active;
      needs_redraw = true;
    }
  }

  if (needs_redraw) {
    s_redraw = true;
  }
}

// ═══════════════════════════════════════════════════════════════
// RENDER (despachador)
// ═══════════════════════════════════════════════════════════════

void app_airdrop_render() {
  // REMOVIDO: if (!s_redraw) return;
  // Agora sempre renderizamos quando o sistema chamar (como no Pomodoro)

  switch (s_mode) {
    case MODE_MENU: render_menu(); break;
    case MODE_SEND: render_send(); break;
    case MODE_SEND_CONFIRM: render_send_confirm(); break;
    case MODE_SENDING: render_sending(); break;
    case MODE_RECV: render_recv(); break;
    case MODE_RECV_DONE: render_recv_done(); break;
  }

  // Resetamos a flag depois de desenhar
  s_redraw = false;
}

// ═══════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════

void app_airdrop_on_enter() {
  s_mode = MODE_MENU;
  s_sel = 0;
  s_sending_active = false;
  s_recv_active = false;
  s_recv_done = false;
  s_redraw = true;  // força primeiro desenho
  memset(s_offers, 0, sizeof(s_offers));
}

void app_airdrop_on_exit() {
  s_sending_active = false;
  s_recv_active = false;
}

void app_airdrop_setup() {
  app_airdrop.name = "AirDrop";
  app_airdrop.on_enter = app_airdrop_on_enter;
  app_airdrop.on_exit = app_airdrop_on_exit;
  app_airdrop.on_event = app_airdrop_on_event;
  app_airdrop.render = app_airdrop_render;
  app_airdrop.update = airdrop_update;   // ← importante
  app_airdrop.update_interval_ms = 250;  // atualiza a cada 250ms (bom equilíbrio)
  app_airdrop.update_ctx = nullptr;
  app_airdrop._task_id = -1;
}