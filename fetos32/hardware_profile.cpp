#include "hardware_profile.h"
#include "driver_ssd1306.h"
#include "driver_lcd1602.h"
#include <LittleFS.h>
#include "ArduinoJson.h"
#include <Wire.h>
#include <string.h>

static char s_profile_name[32] = "DEFAULT";

HwDeviceConfig hw_gpio_devices[HW_PROFILE_MAX_DEVICES];
int            hw_gpio_device_count = 0;

// ── perfil padrão — usado se hardware.json não existir ────────

static void apply_default_profile() {
  Serial.println("[HwProfile] Arquivo hardware.json não encontrado.");
  Serial.println("[HwProfile] Aplicando perfil padrão: OLED SSD1306 + GPIO padrao");

  strncpy(s_profile_name, "DEFAULT", sizeof(s_profile_name));

  // Display: OLED SSD1306 no I2C padrão
  DisplayDriver* oled = driver_ssd1306_create(0x3C, &Wire);
  if (oled) {
    oled->device_id = 2;
    display_hal_register(oled);
  }

  // GPIO padrão — button, buzzer, rgb
  hw_gpio_devices[hw_gpio_device_count++] = { 1, "BUTTON_GPIO",  4,  0 };
  hw_gpio_devices[hw_gpio_device_count++] = { 3, "BUZZER_GPIO",  15, 0 };
  hw_gpio_devices[hw_gpio_device_count++] = { 4, "RGB_LED",      16, 0 };
}

// ── carregamento do JSON ───────────────────────────────────────

bool hardware_profile_load() {
  if (!LittleFS.exists("/hardware.json")) {
    apply_default_profile();
    return false;
  }

  File f = LittleFS.open("/hardware.json", "r");
  if (!f) {
    apply_default_profile();
    return false;
  }

  // ArduinoJson — capacidade para um perfil razoável
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[HwProfile] Erro ao parsear hardware.json: %s\n",
                  err.c_str());
    apply_default_profile();
    return false;
  }

  const char* profile_name = doc["profile"] | "UNNAMED";
  strncpy(s_profile_name, profile_name, sizeof(s_profile_name) - 1);
  Serial.printf("[HwProfile] Carregando perfil: %s\n", s_profile_name);

  JsonArray devices = doc["devices"].as<JsonArray>();

  for (JsonObject device : devices) {
    uint8_t     id   = device["id"]   | 0;
    const char* type = device["type"] | "";

    // ── displays ──────────────────────────────────────────────
    if (strcmp(type, "DISPLAY_SSD1306") == 0) {
      const char* addr_str = device["address"] | "0x3C";
      uint8_t addr = (uint8_t)strtol(addr_str, nullptr, 16);

      DisplayDriver* drv = driver_ssd1306_create(addr, &Wire);
      if (drv) {
        drv->device_id = id;
        display_hal_register(drv);
        Serial.printf("[HwProfile] SSD1306 id=%d addr=0x%02X\n", id, addr);
      }
    }
    else if (strcmp(type, "DISPLAY_LCD1602_PAR") == 0) {
      JsonObject pins = device["pins"];
      int rs = pins["rs"] | -1;
      int en = pins["en"] | -1;
      int d4 = pins["d4"] | -1;
      int d5 = pins["d5"] | -1;
      int d6 = pins["d6"] | -1;
      int d7 = pins["d7"] | -1;

      if (rs < 0 || en < 0 || d4 < 0 || d5 < 0 || d6 < 0 || d7 < 0) {
        Serial.println("[HwProfile] LCD1602: pinos incompletos, ignorado");
        continue;
      }

      DisplayDriver* drv = driver_lcd1602_create(rs, en, d4, d5, d6, d7);
      if (drv) {
        drv->device_id = id;
        display_hal_register(drv);
        Serial.printf("[HwProfile] LCD1602 id=%d rs=%d en=%d\n", id, rs, en);
      }
    }
    // ── gpio / outros ─────────────────────────────────────────
    else {
      if (hw_gpio_device_count >= HW_PROFILE_MAX_DEVICES) continue;

      HwDeviceConfig cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.device_id = id;
      strncpy(cfg.type, type, sizeof(cfg.type) - 1);
      cfg.pin = device["pin"] | -1;

      hw_gpio_devices[hw_gpio_device_count++] = cfg;
      Serial.printf("[HwProfile] GPIO device: id=%d type=%s pin=%d\n",
                    id, type, cfg.pin);
    }
  }

  // Aviso se nenhum display foi registrado
  if (display_hal_get_primary(DISPLAY_DEFAULT_ID) == nullptr) {
    Serial.println("[HwProfile] AVISO: nenhum display registrado para id=2");
    Serial.println("[HwProfile] Ativando SSD1306 padrão como fallback");
    DisplayDriver* fallback = driver_ssd1306_create(0x3C, &Wire);
    if (fallback) { fallback->device_id = 2; display_hal_register(fallback); }
  }

  return true;
}

const char* hardware_profile_name() {
  return s_profile_name;
}