#pragma once
#include <Arduino.h>
#include "display_hal.h"

// ── Hardware Profile ──────────────────────────────────────────
// Lê /hardware.json do LittleFS e instancia os drivers correspondentes.
// Se o arquivo não existir, aplica o perfil padrão hardcoded (OLED SSD1306).
//
// Formato do JSON:
// {
//   "profile": "NOME_OPCIONAL",
//   "devices": [
//     { "id": 1, "type": "BUTTON_GPIO",          "pin": 4 },
//     { "id": 2, "type": "DISPLAY_SSD1306",       "address": "0x3C" },
//     { "id": 2, "type": "DISPLAY_LCD1602_PAR",   "pins": {"rs":12,"en":13,"d4":14,"d5":27,"d6":26,"d7":25} },
//     { "id": 3, "type": "BUZZER_GPIO",            "pin": 15 },
//     { "id": 4, "type": "RGB_LED",               "pin": 16 }
//   ]
// }
//
// Tipos suportados:
//   DISPLAY_SSD1306       — OLED I2C via Adafruit
//   DISPLAY_LCD1602_PAR   — LCD 16x2 paralelo via LiquidCrystal
//   BUTTON_GPIO           — botão com pullup (configurado no button.cpp)
//   BUZZER_GPIO           — buzzer passivo (configurado no buzzer.cpp)
//   RGB_LED               — LED RGB (configurado no rgb_led.cpp)
//
// IDs repetidos no JSON habilitam broadcast — todos os drivers com o
// mesmo ID recebem o mesmo comando da capability layer.

// Carrega o perfil do LittleFS e registra os DisplayDrivers.
// Retorna true se carregou do arquivo, false se usou o padrão.
bool hardware_profile_load();

// Retorna o nome do perfil carregado (ex: "ESP32_PROTOBOARD_V1")
const char* hardware_profile_name();

// Estrutura de configuração de pin para GPIO simples
typedef struct {
  uint8_t device_id;
  char    type[32];
  int     pin;
  uint8_t i2c_address;
  // pinos paralelos para LCD
  int     lcd_rs, lcd_en, lcd_d4, lcd_d5, lcd_d6, lcd_d7;
} HwDeviceConfig;

#define HW_PROFILE_MAX_DEVICES 8

// Após hardware_profile_load(), esses arrays contêm os dispositivos
// não-display que o system.cpp precisa configurar (botão, buzzer, etc.)
extern HwDeviceConfig hw_gpio_devices[HW_PROFILE_MAX_DEVICES];
extern int            hw_gpio_device_count;