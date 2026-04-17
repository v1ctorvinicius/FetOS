#pragma once
#include <Arduino.h>
#include "capability.h"

// ── versão do formato .fvm ────────────────────────────────────
#define FVM_FORMAT_V1 "FVM1"   // legado — stack/heap fixos
#define FVM_FORMAT_V2 "FVM2"   // novo — header dinâmico

// ── opcodes ───────────────────────────────────────────────────

typedef enum : uint8_t {
  OP_PUSH_INT  = 0x01,
  OP_PUSH_STR  = 0x02,
  OP_PUSH_BOOL = 0x03,
  OP_PUSH_NIL  = 0x04,
  OP_POP       = 0x05,
  OP_DUP       = 0x06,
  OP_STORE_H   = 0x10,
  OP_LOAD_H    = 0x11,
  OP_ADD       = 0x20,
  OP_SUB       = 0x21,
  OP_MUL       = 0x22,
  OP_DIV       = 0x23,
  OP_EQ        = 0x30,
  OP_LT        = 0x31,
  OP_GT        = 0x32,
  OP_AND       = 0x33,
  OP_OR        = 0x34,
  OP_NOT       = 0x35,
  OP_JMP       = 0x40,
  OP_JIF       = 0x41,
  OP_JNIF      = 0x42,
  OP_CALL      = 0x43,
  OP_RET       = 0x44,
  OP_HALT      = 0x45,
  OP_SYS_REQ   = 0x50,
  OP_SYS_EVT   = 0x51,
  OP_STORE_P   = 0x60,
  OP_LOAD_P    = 0x61,
} FvmOpcode;

// ── tipos de valor ────────────────────────────────────────────

typedef enum : uint8_t {
  VAL_INT, VAL_BOOL, VAL_STRING, VAL_NIL
} ValType;

typedef struct {
  ValType type;
  union { int32_t i; bool b; uint8_t str_idx; };
} Val;

// ── capability state ──────────────────────────────────────────

#define MAX_CAPABILITY_STATES 8

typedef struct {
  const char* capability;
  void*       data;
} CapabilityState;

// ── limites padrão (FVM1 legado) ─────────────────────────────
#define FVM_STACK_SIZE_DEFAULT      16
#define FVM_CALL_STACK_SIZE_DEFAULT  8
#define FVM_HEAP_SLOTS_DEFAULT      16

// ── limites máximos (FVM2) ────────────────────────────────────
#define FVM_STACK_SIZE_MAX      64
#define FVM_CALL_STACK_SIZE_MAX 32
#define FVM_HEAP_SLOTS_MAX     128
#define FVM_RAM_REQ_MAX      49152   // 48KB — teto razoável no ESP32

// ── processo FVM ──────────────────────────────────────────────

typedef struct {
  // bytecode
  const uint8_t* bytecode;
  uint16_t       bytecode_len;
  uint16_t       pc;

  // stack dinâmica — alocada no create, liberada no destroy
  Val*     stack;
  uint16_t stack_size;   // capacidade real alocada
  uint16_t sp;

  // call stack dinâmica
  uint16_t* call_stack;
  uint8_t   call_stack_size;
  uint8_t   csp;

  // heap dinâmico
  int32_t* heap;
  uint8_t  heap_slots;

  // string table
  const char** string_table;
  uint8_t      n_strings;

  // execução
  uint16_t steps_per_quantum;
  bool     halted;
  bool     error;
  uint8_t  error_code;

  // permissões
  uint8_t permissions;

  // capability states
  CapabilityState cap_states[MAX_CAPABILITY_STATES];
  uint8_t         n_cap_states;

  // metadados
  const char* name;
  uint8_t     process_id;

  // FVM2: RAM alocada dinamicamente para stack+heap+call_stack
  // nullptr em FVM1 (memória estática legado não suportada — FVM1 usa defaults dinâmicos também)
  void*    arena;          // bloco único: stack | call_stack | heap
  uint16_t arena_size;     // tamanho total do bloco
} FvmProcess;

// ── erros de runtime ──────────────────────────────────────────

typedef enum : uint8_t {
  FVM_ERR_NONE        = 0,
  FVM_ERR_STACK_OVER  = 1,
  FVM_ERR_STACK_UNDER = 2,
  FVM_ERR_CALL_OVER   = 3,
  FVM_ERR_CALL_UNDER  = 4,
  FVM_ERR_DIV_ZERO    = 5,
  FVM_ERR_BAD_TYPE    = 6,
  FVM_ERR_BAD_OP      = 7,
  FVM_ERR_OOB         = 8,
  FVM_ERR_OOM         = 9,
} FvmError;

// ── API ───────────────────────────────────────────────────────

// Cria processo com tamanhos customizados (FVM2)
FvmProcess* fvm_process_create_ex(const char* name,
                                   const uint8_t* bytecode, uint16_t len,
                                   uint16_t stack_size,
                                   uint8_t  call_stack_size,
                                   uint8_t  heap_slots);

// Cria processo com defaults (compatibilidade FVM1)
FvmProcess* fvm_process_create(const char* name,
                                const uint8_t* bytecode, uint16_t len);

void fvm_process_destroy(FvmProcess* proc);
void fvm_run(FvmProcess* proc);
void** fvm_get_cap_state(FvmProcess* proc, const char* capability);
CallerContext fvm_make_caller(FvmProcess* proc, const char* capability);
FvmProcess* fvm_load_from_memory(const char* name,
                                  const uint8_t* data, uint16_t total_len);
void fvm_process_reset(FvmProcess* proc);
