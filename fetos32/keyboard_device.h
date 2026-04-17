//keyboard_device.h
#pragma once
#include "device.h"
#include "capability.h"

// ── Códigos de teclas especiais ───────────────────────────────
// Valores acima de 127 para não conflitar com ASCII imprimível.
// Retornados por kbd:get quando não é um caractere normal.

#define KEY_NONE 0
#define KEY_ENTER '\r'      // 13
#define KEY_BACKSPACE '\b'  // 8
#define KEY_ESC 27
#define KEY_TAB '\t'  // 9
#define KEY_UP 128
#define KEY_DOWN 129
#define KEY_LEFT 130
#define KEY_RIGHT 131
#define KEY_F1 132
#define KEY_F2 133
#define KEY_DELETE 134
#define KEY_HOME 135
#define KEY_END 136

// ── Estado do device ──────────────────────────────────────────
// Visível externamente para o app_keyboard poder ler status.

#define KBD_BUFFER_SIZE 64

typedef struct {
  uint8_t buf[KBD_BUFFER_SIZE];  // fila circular de bytes
  uint8_t head;                  // próxima leitura
  uint8_t tail;                  // próxima escrita
  bool connected;                // BLE pareado e ativo
  bool caps_lock;                // estado do caps lock
  uint32_t last_key_ms;          // para auto-repeat
  uint8_t repeat_key;            // tecla em hold para repeat
  uint32_t repeat_next_ms;       // quando disparar o próximo repeat
} KeyboardState;

// ── API C direta ──────────────────────────────────────────────
// Usada por: BLE callback, task_uart_keyboard, app_keyboard

void keyboard_inject(char c);
void keyboard_inject_key(uint8_t key_code);

// Retorna próximo char da fila sem remover (0 = vazia)
uint8_t keyboard_peek_raw();

// Retorna próximo char da fila e remove (0 = vazia)
uint8_t keyboard_get_raw();

// Retorna ponteiro para o estado (para o app de UI)
KeyboardState* keyboard_get_state();

// ── Device init ───────────────────────────────────────────────

void keyboard_device_init(Device* dev);