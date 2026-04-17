#include "keyboard_device.h"
#include "system.h"
#include "Arduino.h"
#include <NimBLEDevice.h>
#include "scheduler.h"
#include "capability.h"  // Adicionado para o RequestPayload e capability_request_native

// ── Estado global ─────────────────────────────────────────────

static KeyboardState s_kbd;
static Device* s_dev = nullptr;
static bool s_ble_init = false;

// ── Auto-repeat ───────────────────────────────────────────────
#define REPEAT_DELAY_MS 500
#define REPEAT_RATE_MS 80

// ── Fila circular ─────────────────────────────────────────────

static inline bool buf_full() {
  return ((s_kbd.tail + 1) % KBD_BUFFER_SIZE) == s_kbd.head;
}

static inline bool buf_empty() {
  return s_kbd.head == s_kbd.tail;
}

static void buf_push(uint8_t c) {
  if (buf_full()) return;
  s_kbd.buf[s_kbd.tail] = c;
  s_kbd.tail = (s_kbd.tail + 1) % KBD_BUFFER_SIZE;
}

static uint8_t buf_pop() {
  if (buf_empty()) return 0;
  uint8_t c = s_kbd.buf[s_kbd.head];
  s_kbd.head = (s_kbd.head + 1) % KBD_BUFFER_SIZE;
  return c;
}

static uint8_t buf_peek_val() {
  if (buf_empty()) return 0;
  return s_kbd.buf[s_kbd.head];
}

// ── HID tables ────────────────────────────────────────────────
// MOVIDAS PARA CIMA: Assim o C++ conhece as tabelas antes de usá-las

static const char hid_to_ascii_lower[128] = {
  0, 0, 0, 0,
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0',
  '\r', 27, '\b', '\t', ' ',
  '-', '=', '[', ']', '\\', '#', ';', '\'',
  '`', ',', '.', '/',
  0,
  KEY_F1, KEY_F2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, KEY_HOME, 0, KEY_DELETE, KEY_END, 0,
  KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP
};

static const char hid_to_ascii_upper[128] = {
  0, 0, 0, 0,
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
  '\r', 27, '\b', '\t', ' ',
  '_', '+', '{', '}', '|', '~', ':', '"',
  '~', '<', '>', '?',
  0,
  KEY_F1, KEY_F2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, KEY_HOME, 0, KEY_DELETE, KEY_END, 0,
  KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP
};


// ── API pública ───────────────────────────────────────────────

// --- TRADUÇÃO E INJEÇÃO ---
void keyboard_inject(char c) {
  Serial.printf("[KBD] inject: '%c' (%d)\n", c, (int)c);

  // Caps lock
  if (s_kbd.caps_lock && c >= 'a' && c <= 'z') c -= 32;

  // Alimenta a fila circular interna — consumida via kbd:get
  buf_push((uint8_t)c);

  // Auto-repeat: registra tecla para repeat
  s_kbd.repeat_key = (uint8_t)c;
  s_kbd.repeat_next_ms = millis() + REPEAT_DELAY_MS;
}

void keyboard_inject_key(uint8_t key_code) {
  buf_push(key_code);
  s_kbd.last_key_ms = millis();
  s_kbd.repeat_key = key_code;
  s_kbd.repeat_next_ms = millis() + REPEAT_DELAY_MS;
}

uint8_t keyboard_peek_raw() {
  return buf_peek_val();
}
uint8_t keyboard_get_raw() {
  return buf_pop();
}
KeyboardState* keyboard_get_state() {
  return &s_kbd;
}

// ── NimBLE globals ────────────────────────────────────────────

static bool s_scanning = false;
static uint32_t s_last_scan = 0;
static NimBLEClient* s_client = nullptr;


// ── Client callbacks ──────────────────────────────────────────

class KbdClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* client) override {
    s_kbd.connected = true;
    Serial.println("[KBD] conectado");
  }

  void onDisconnect(NimBLEClient* client, int reason) override {
    s_kbd.connected = false;
    Serial.println("[KBD] desconectado");
  }
};


// ── Scan callbacks ────────────────────────────────────────────

class KbdScanCallbacks : public NimBLEScanCallbacks {
public:
  NimBLEAddress found_addr;
  bool found = false;

  void onResult(const NimBLEAdvertisedDevice* device) override {
    Serial.printf("[KBD] Vi algo: %s | RSSI: %d\n",
                  device->getName().c_str(),
                  device->getRSSI());
    if (device->isAdvertisingService(NimBLEUUID("1812"))) {
      Serial.printf("[KBD] encontrado: %s\n", device->getAddress().toString().c_str());

      found_addr = device->getAddress();
      found = true;

      NimBLEDevice::getScan()->stop();
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    s_scanning = false;
  }
};

static KbdClientCallbacks s_client_cbs;
static KbdScanCallbacks s_scan_cbs;


// ── HID notify ────────────────────────────────────────────────

static void hid_notify_cb(
  NimBLERemoteCharacteristic* chr,
  uint8_t* data,
  size_t len,
  bool is_notify) {

  // Log 4: Chegou algum pacote bruto do Bluetooth?
  Serial.printf("[KBD-DEBUG] NOTIFY: len=%d | bytes: ", (int)len);
  for (int i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
  Serial.println();

  if (len < 3) return;

  uint8_t modifiers = data[0];
  uint8_t keycode = data[2];  // Alguns teclados usam o byte 2, outros o 3.

  if (keycode == 0) return;  // Tecla solta ou buffer vazio

  if (keycode == 0x39) {
    s_kbd.caps_lock = !s_kbd.caps_lock;
    return;
  }

  // Log 5: Tecla identificada antes da tradução
  Serial.printf("[KBD-DEBUG] Keycode detectado: 0x%02X | Modifiers: 0x%02X\n", keycode, modifiers);

  bool shifted = (modifiers & 0x22) != 0;
  char c = 0;
  if (keycode < 128) {
    bool upper = shifted ^ s_kbd.caps_lock;
    c = upper ? hid_to_ascii_upper[keycode]
              : hid_to_ascii_lower[keycode];
  }

  if (c) {
    keyboard_inject(c);
  } else {
    Serial.println("[KBD-DEBUG] Keycode não possui mapeamento ASCII.");
  }
}


// ── BLE connect ───────────────────────────────────────────────

static void ble_connect(NimBLEAddress addr) {
  if (!s_client) {
    s_client = NimBLEDevice::createClient();
    s_client->setClientCallbacks(&s_client_cbs, false);
  }

  // Segurança ativada
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  Serial.println("[KBD] Conectando...");
  if (!s_client->connect(addr)) {
    Serial.println("[KBD] Falha na conexao física.");
    return;
  }

  // --- O PULO DO GATO ---
  // Forçamos a descoberta de TODOS os serviços e características
  // Sem isso, o getCharacteristics() pode retornar um vetor vazio por "preguiça" do driver
  Serial.println("[KBD] Descobrindo atributos...");
  s_client->discoverAttributes();

  auto svc = s_client->getService("1812");
  if (!svc) {
    Serial.println("[KBD] Erro: Servico HID (1812) nao encontrado!");
    return;
  }

  // Tenta forçar o Modo Boot
  auto protocolMode = svc->getCharacteristic("2A4E");
  if (protocolMode) {
    uint8_t bootMode = 0x00;
    protocolMode->writeValue(&bootMode, 1, false);
    Serial.println("[KBD] Protocol Mode -> BOOT");
  }

  // Busca as características agora que os atributos foram "descobertos"
  const std::vector<NimBLERemoteCharacteristic*>& chars = svc->getCharacteristics();

  int subscribed_count = 0;
  for (auto const& chr : chars) {
    // Log para a gente ver no Serial o que ele está achando
    Serial.printf("[KBD-DEBUG] Achado: %s | Notify: %s\n",
                  chr->getUUID().toString().c_str(),
                  chr->canNotify() ? "SIM" : "NAO");

    if (chr->canNotify()) {
      if (chr->subscribe(true, hid_notify_cb)) {
        Serial.printf("[KBD] INSCRITO: %s\n", chr->getUUID().toString().c_str());
        subscribed_count++;
      }
    }
  }

  if (subscribed_count > 0) {
    Serial.printf("[KBD] HID PRONTO! (%d canais)\n", subscribed_count);
  } else {
    Serial.println("[KBD] Erro: Nenhuma caracteristica de Notify disponivel.");
  }
}

// ── Poll task ─────────────────────────────────────────────────

static void kbd_poll_task(void*) {

  uint32_t now = millis();

  if (s_kbd.repeat_key && buf_empty()) {
    if (now >= s_kbd.repeat_next_ms) {
      buf_push(s_kbd.repeat_key);
      s_kbd.repeat_next_ms = now + REPEAT_RATE_MS;
    }
  }

  if (s_kbd.connected) return;

  if (s_scan_cbs.found) {
    s_scan_cbs.found = false;
    ble_connect(s_scan_cbs.found_addr);
    return;
  }
}


// ── Capabilities ──────────────────────────────────────────────

static RequestResult h_get(Device*, const RequestPayload* p, CallerContext*) {
  // Se o FetScript chamou como sys_result, o valor vai pro primeiro slot disponível
  p->params[0].int_value = buf_pop();
  return REQ_ACCEPTED;
}

static RequestResult h_status(Device*, const RequestPayload* p, CallerContext*) {
  p->params[0].int_value = s_kbd.connected;  // Direto no slot 0
  return REQ_ACCEPTED;
}

static RequestResult h_scan(Device*, const RequestPayload*, CallerContext*) {
  Serial.println("\n[KBD] >>> COMANDO DE SCAN RECEBIDO VIA FVM! <<<");
  if (!s_ble_init) {
    NimBLEDevice::init("FetOS32");
    s_ble_init = true;
  }

  if (s_scanning) return REQ_ACCEPTED;

  s_scan_cbs.found = false;
  s_scanning = true;

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&s_scan_cbs);
  scan->setActiveScan(true);
  scan->start(5000, false);

  return REQ_ACCEPTED;
}

static RequestResult h_connect(Device*, const RequestPayload*, CallerContext*) {

  if (!s_scan_cbs.found) return REQ_ACCEPTED;

  s_scan_cbs.found = false;
  ble_connect(s_scan_cbs.found_addr);

  return REQ_ACCEPTED;
}

static RequestResult h_found(Device*, const RequestPayload* p, CallerContext*) {
  p->params[0].int_value = s_scan_cbs.found;
  return REQ_ACCEPTED;
}

static RequestResult h_scanning(Device*, const RequestPayload* p, CallerContext*) {
  p->params[0].int_value = s_scanning;
  return REQ_ACCEPTED;
}


// ── Init ──────────────────────────────────────────────────────

void keyboard_device_init(Device* dev) {

  memset(&s_kbd, 0, sizeof(s_kbd));
  s_dev = dev;
  dev->state = &s_kbd;


  system_register_capability("kbd:get", h_get, dev);
  system_register_capability("kbd:status", h_status, dev);
  system_register_capability("kbd:scan", h_scan, dev);
  system_register_capability("kbd:connect", h_connect, dev);
  system_register_capability("kbd:found", h_found, dev);
  system_register_capability("kbd:scanning", h_scanning, dev);

  scheduler_add(kbd_poll_task, dev, 100, TASK_PRIORITY_LOW, "kbd");

  Serial.println("[KBD] ready");
}