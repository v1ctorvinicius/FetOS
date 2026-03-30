#pragma once
#include <Arduino.h>
#include "capability.h"

// ── opcodes ───────────────────────────────────────────────────

typedef enum : uint8_t
{
  // stack
  OP_PUSH_INT = 0x01,  // [i16]     empilha inteiro
  OP_PUSH_STR = 0x02,  // [u8]      empilha string por índice
  OP_PUSH_BOOL = 0x03, // [u8]      empilha bool (0/1)
  OP_PUSH_NIL = 0x04,  // —         empilha nil
  OP_POP = 0x05,       // —         descarta topo

  // heap (variáveis locais — slots de 4 bytes)
  OP_STORE_H = 0x10, // [u8]      pop → heap[idx]
  OP_LOAD_H = 0x11,  // [u8]      heap[idx] → push

  // aritmética
  OP_ADD = 0x20, // —
  OP_SUB = 0x21, // —
  OP_MUL = 0x22, // —
  OP_DIV = 0x23, // —

  // comparação (push VAL_BOOL)
  OP_EQ = 0x30,  // —
  OP_LT = 0x31,  // —
  OP_GT = 0x32,  // —
  OP_AND = 0x33, // —
  OP_OR = 0x34,  // —
  OP_NOT = 0x35, // —         inverte topo bool

  // controle de fluxo
  OP_JMP = 0x40,  // [u16]     salta para endereço absoluto
  OP_JIF = 0x41,  // [u16]     salta se topo == true
  OP_JNIF = 0x42, // [u16]     salta se topo == false
  OP_CALL = 0x43, // [u16]     push PC na call stack, JMP
  OP_RET = 0x44,  // —         pop call stack, JMP
  OP_HALT = 0x45, // —         para execução

  // sistema
  OP_SYS_REQ = 0x50, // [u8 cap_idx][u8 n_params]
                     // monta payload com n_params pares (key_idx, val) da stack
                     // sempre empilha resultado (VAL_INT com RequestResult ou valor)
  OP_SYS_EVT = 0x51, // [u8 type]  empurra evento no barramento

  // persistência
  OP_STORE_P = 0x60, // [u8 key_idx]   persiste topo com chave da string_table[idx]
  OP_LOAD_P = 0x61,  // [u8 key_idx]   carrega valor persistido → push
} FvmOpcode;

typedef enum : uint8_t
{
  VAL_INT,
  VAL_BOOL,
  VAL_STRING,
  VAL_NIL
} ValType;

typedef struct
{
  ValType type;
  union
  {
    int32_t i;
    bool b;
    uint8_t str_idx;
  };
} Val;

#define MAX_CAPABILITY_STATES 8

typedef struct
{
  const char *capability;
  void *data;
} CapabilityState;

#define FVM_STACK_SIZE 16
#define FVM_CALL_STACK_SIZE 8
#define FVM_HEAP_SLOTS 16

typedef struct
{

  const uint8_t *bytecode;
  uint16_t bytecode_len;
  uint16_t pc;

  Val stack[FVM_STACK_SIZE];
  uint8_t sp;

  uint16_t call_stack[FVM_CALL_STACK_SIZE];
  uint8_t csp; // call stack pointer

  int32_t heap[FVM_HEAP_SLOTS];

  const char **string_table;
  uint8_t n_strings;

  uint16_t steps_per_quantum;
  bool halted;
  bool error;
  uint8_t error_code;

  // bit 0: audio, bit 1: display, bit 2: led, bit 3: buffer, bit 4: net
  uint8_t permissions;

  // lazy allocated
  CapabilityState cap_states[MAX_CAPABILITY_STATES];
  uint8_t n_cap_states;

  const char *name;
  uint8_t process_id;
} FvmProcess;

typedef enum : uint8_t
{
  FVM_ERR_NONE = 0,
  FVM_ERR_STACK_OVER = 1,  // data stack overflow
  FVM_ERR_STACK_UNDER = 2, // data stack underflow
  FVM_ERR_CALL_OVER = 3,   // call stack overflow
  FVM_ERR_CALL_UNDER = 4,  // call stack underflow
  FVM_ERR_DIV_ZERO = 5,
  FVM_ERR_BAD_TYPE = 6, // operação em tipo errado
  FVM_ERR_BAD_OP = 7,   // opcode desconhecido
  FVM_ERR_OOB = 8,      // acesso fora dos limites (heap, string_table)
  FVM_ERR_OOM = 9,      // sem memória para capability state
} FvmError;

// ── API ───────────────────────────────────────────────────────

FvmProcess *fvm_process_create(const char *name, const uint8_t *bytecode, uint16_t len);
void fvm_process_destroy(FvmProcess *proc);

void fvm_run(FvmProcess *proc);

void **fvm_get_cap_state(FvmProcess *proc, const char *capability);
CallerContext fvm_make_caller(FvmProcess *proc, const char *capability);

FvmProcess *fvm_load_from_memory(const char *name, const uint8_t *data, uint16_t total_len);

void fvm_process_reset(FvmProcess *proc);