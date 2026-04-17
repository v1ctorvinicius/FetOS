#include "fvm_process.h"
#include "system.h"
#include "persistence.h"
#include "event.h"
#include <stdlib.h>
#include <string.h>

static uint8_t next_process_id = 1;

// ── helpers de stack ──────────────────────────────────────────

static bool stack_push(FvmProcess* p, Val v) {
  if (p->sp >= p->stack_size) {
    p->error = true;
    p->error_code = FVM_ERR_STACK_OVER;
    return false;
  }
  p->stack[p->sp++] = v;
  return true;
}

static bool stack_pop(FvmProcess* p, Val* out) {
  if (p->sp == 0) {
    p->error = true;
    p->error_code = FVM_ERR_STACK_UNDER;
    return false;
  }
  *out = p->stack[--p->sp];
  return true;
}

static Val val_int(int32_t i) {
  Val v;
  v.type = VAL_INT;
  v.i = i;
  return v;
}
static Val val_bool(bool b) {
  Val v;
  v.type = VAL_BOOL;
  v.b = b;
  return v;
}
static Val val_str(uint8_t s) {
  Val v;
  v.type = VAL_STRING;
  v.str_idx = s;
  return v;
}
static Val val_nil() {
  Val v;
  v.type = VAL_NIL;
  v.i = 0;
  return v;
}

static bool val_as_bool(const Val& v) {
  switch (v.type) {
    case VAL_BOOL: return v.b;
    case VAL_INT: return v.i != 0;
    case VAL_NIL: return false;
    default: return false;
  }
}

// ── helpers de fetch ──────────────────────────────────────────

static uint8_t fetch_u8(FvmProcess* p) {
  if (p->pc >= p->bytecode_len) {
    p->halted = true;
    return 0;
  }
  return p->bytecode[p->pc++];
}

static int16_t fetch_i16(FvmProcess* p) {
  uint8_t hi = fetch_u8(p);
  uint8_t lo = fetch_u8(p);
  return (int16_t)((hi << 8) | lo);
}

static uint16_t fetch_u16(FvmProcess* p) {
  uint8_t hi = fetch_u8(p);
  uint8_t lo = fetch_u8(p);
  return (uint16_t)((hi << 8) | lo);
}

// ── aritmética ────────────────────────────────────────────────

static void arith_op(FvmProcess* p, FvmOpcode op) {
  Val b, a;
  if (!stack_pop(p, &b)) return;
  if (!stack_pop(p, &a)) return;
  if (a.type != VAL_INT || b.type != VAL_INT) {
    p->error = true;
    p->error_code = FVM_ERR_BAD_TYPE;
    return;
  }
  int32_t result = 0;
  switch (op) {
    case OP_ADD: result = a.i + b.i; break;
    case OP_SUB: result = a.i - b.i; break;
    case OP_MUL: result = a.i * b.i; break;
    case OP_DIV:
      if (b.i == 0) {
        p->error = true;
        p->error_code = FVM_ERR_DIV_ZERO;
        return;
      }
      result = a.i / b.i;
      break;
    default: break;
  }
  stack_push(p, val_int(result));
}

static void compare_op(FvmProcess* p, FvmOpcode op) {
  Val b, a;
  if (!stack_pop(p, &b)) return;
  if (!stack_pop(p, &a)) return;
  bool result = false;
  switch (op) {
    case OP_EQ:
      if (a.type == VAL_INT && b.type == VAL_INT) result = (a.i == b.i);
      else if (a.type == VAL_BOOL && b.type == VAL_BOOL) result = (a.b == b.b);
      else if (a.type == VAL_INT && b.type == VAL_BOOL) result = ((a.i != 0) == b.b);
      else if (a.type == VAL_BOOL && b.type == VAL_INT) result = (a.b == (b.i != 0));
      else if (a.type == VAL_NIL && b.type == VAL_NIL) result = true;
      break;
    case OP_LT:
      if (a.type != VAL_INT || b.type != VAL_INT) {
        p->error = true;
        p->error_code = FVM_ERR_BAD_TYPE;
        return;
      }
      result = (a.i < b.i);
      break;
    case OP_GT:
      if (a.type != VAL_INT || b.type != VAL_INT) {
        p->error = true;
        p->error_code = FVM_ERR_BAD_TYPE;
        return;
      }
      result = (a.i > b.i);
      break;
    case OP_AND: result = val_as_bool(a) && val_as_bool(b); break;
    case OP_OR: result = val_as_bool(a) || val_as_bool(b); break;
    default: break;
  }
  stack_push(p, val_bool(result));
}

// ── SYS_REQ ───────────────────────────────────────────────────

static void exec_sys_req(FvmProcess* p, uint8_t cap_idx, uint8_t n_params) {
  if (cap_idx >= p->n_strings) {
    p->error = true;
    p->error_code = FVM_ERR_OOB;
    return;
  }
  const char* cap_name = p->string_table[cap_idx];

  static RequestParam params[8];
  static RequestParam result_param = { "result", nullptr, 0 };
  params[0] = result_param;
  params[0].int_value = 0;

  for (int i = 0; i < n_params && i < 7; i++) {
    Val val, key;
    if (!stack_pop(p, &val)) return;
    if (!stack_pop(p, &key)) return;
    if (key.type != VAL_STRING || key.str_idx >= p->n_strings) {
      p->error = true;
      p->error_code = FVM_ERR_OOB;
      return;
    }
    params[i + 1].key = p->string_table[key.str_idx];
    params[i + 1].str_value = (val.type == VAL_STRING && val.str_idx < p->n_strings)
                                ? p->string_table[val.str_idx]
                                : nullptr;
    params[i + 1].int_value = (val.type == VAL_INT)    ? val.i
                              : (val.type == VAL_BOOL) ? (int32_t)val.b
                                                       : 0;
  }

  RequestPayload payload = { params, (uint8_t)(n_params + 1) };
  CallerContext caller = fvm_make_caller(p, cap_name);
  RequestResult res = system_request_fvm(cap_name, &payload, &caller);

  stack_push(p, val_int(res == REQ_ACCEPTED ? params[0].int_value : (int32_t)res));
}

// ── execute ───────────────────────────────────────────────────

static void execute(FvmProcess* p) {
  FvmOpcode op = (FvmOpcode)fetch_u8(p);
  switch (op) {
    case OP_PUSH_INT:
      {
        int16_t v = fetch_i16(p);
        stack_push(p, val_int((int32_t)v));
        break;
      }
    case OP_PUSH_STR:
      {
        uint8_t idx = fetch_u8(p);
        if (idx >= p->n_strings) {
          p->error = true;
          p->error_code = FVM_ERR_OOB;
          break;
        }
        stack_push(p, val_str(idx));
        break;
      }
    case OP_PUSH_BOOL:
      {
        stack_push(p, val_bool(fetch_u8(p) != 0));
        break;
      }
    case OP_PUSH_NIL:
      {
        stack_push(p, val_nil());
        break;
      }
    case OP_POP:
      {
        Val v;
        stack_pop(p, &v);
        break;
      }
    case OP_DUP:
      {
        Val v;
        if (stack_pop(p, &v)) {
          stack_push(p, v);
          stack_push(p, v);
        }
        break;
      }
    case OP_STORE_H:
      {
        uint8_t idx = fetch_u8(p);
        if (idx >= p->heap_slots) {
          p->error = true;
          p->error_code = FVM_ERR_OOB;
          break;
        }
        Val v;
        if (!stack_pop(p, &v)) break;
        p->heap[idx] = (v.type == VAL_INT) ? v.i : (v.type == VAL_BOOL) ? (int32_t)v.b
                                                                        : 0;
        break;
      }
    case OP_LOAD_H:
      {
        uint8_t idx = fetch_u8(p);
        if (idx >= p->heap_slots) {
          p->error = true;
          p->error_code = FVM_ERR_OOB;
          break;
        }
        stack_push(p, val_int(p->heap[idx]));
        break;
      }

    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV: arith_op(p, op); break;
    case OP_EQ:
    case OP_LT:
    case OP_GT:
    case OP_AND:
    case OP_OR: compare_op(p, op); break;
    case OP_NOT:
      {
        Val v;
        if (!stack_pop(p, &v)) break;
        stack_push(p, val_bool(!val_as_bool(v)));
        break;
      }

    case OP_JMP:
      {
        p->pc = fetch_u16(p);
        break;
      }
    case OP_JIF:
      {
        uint16_t addr = fetch_u16(p);
        Val v;
        if (!stack_pop(p, &v)) break;
        if (val_as_bool(v)) p->pc = addr;
        break;
      }
    case OP_JNIF:
      {
        uint16_t addr = fetch_u16(p);
        Val v;
        if (!stack_pop(p, &v)) break;
        if (!val_as_bool(v)) p->pc = addr;
        break;
      }
    case OP_CALL:
      {
        uint16_t addr = fetch_u16(p);
        if (p->csp >= p->call_stack_size) {
          p->error = true;
          p->error_code = FVM_ERR_CALL_OVER;
          break;
        }
        p->call_stack[p->csp++] = p->pc;
        p->pc = addr;
        break;
      }
    case OP_RET:
      {
        if (p->csp == 0) {
          p->error = true;
          p->error_code = FVM_ERR_CALL_UNDER;
          break;
        }
        p->pc = p->call_stack[--p->csp];
        break;
      }
    case OP_HALT: p->halted = true; break;

    case OP_SYS_REQ:
      {
        uint8_t ci = fetch_u8(p), np = fetch_u8(p);
        exec_sys_req(p, ci, np);
        break;
      }
    case OP_SYS_EVT:
      {
        uint8_t type = fetch_u8(p);
        Event e;
        e.type = (EventType)type;
        e.device_id = 0;
        e.has_payload = false;
        event_push(e);
        break;
      }
    case OP_STORE_P:
      {
        uint8_t ki = fetch_u8(p);
        if (ki >= p->n_strings) {
          p->error = true;
          p->error_code = FVM_ERR_OOB;
          break;
        }
        Val v;
        if (!stack_pop(p, &v)) break;
        int32_t ts = (v.type == VAL_INT) ? v.i : (v.type == VAL_BOOL) ? (int32_t)v.b
                                                                      : 0;
        persistence_write_int(p->string_table[ki], (int)ts);
        break;
      }
    case OP_LOAD_P:
      {
        uint8_t ki = fetch_u8(p);
        if (ki >= p->n_strings) {
          p->error = true;
          p->error_code = FVM_ERR_OOB;
          break;
        }
        stack_push(p, val_int((int32_t)persistence_read_int(p->string_table[ki], 0)));
        break;
      }
    default:
      p->error = true;
      p->error_code = FVM_ERR_BAD_OP;
      break;
  }
}

void fvm_run(FvmProcess* proc) {
  if (!proc || proc->halted || proc->error) return;
  uint16_t steps = proc->steps_per_quantum;
  while (steps-- > 0 && !proc->halted && !proc->error) execute(proc);
}

// ── criação com arena dinâmica ────────────────────────────────

FvmProcess* fvm_process_create_ex(const char* name,
                                  const uint8_t* bytecode, uint16_t len,
                                  uint16_t stack_size,
                                  uint8_t call_stack_size,
                                  uint8_t heap_slots) {
  // clamp nos máximos
  if (stack_size > FVM_STACK_SIZE_MAX) stack_size = FVM_STACK_SIZE_MAX;
  if (call_stack_size > FVM_CALL_STACK_SIZE_MAX) call_stack_size = FVM_CALL_STACK_SIZE_MAX;
  if (heap_slots > FVM_HEAP_SLOTS_MAX) heap_slots = FVM_HEAP_SLOTS_MAX;

  // aloca arena única: [Val * stack_size] [uint16_t * call_stack_size] [int32_t * heap_slots]
  uint16_t arena_size = (uint16_t)(stack_size * sizeof(Val) + call_stack_size * sizeof(uint16_t) + heap_slots * sizeof(int32_t));

  FvmProcess* proc = (FvmProcess*)malloc(sizeof(FvmProcess));
  if (!proc) return nullptr;
  memset(proc, 0, sizeof(FvmProcess));

  void* arena = malloc(arena_size);
  if (!arena) {
    free(proc);
    return nullptr;
  }
  memset(arena, 0, arena_size);

  uint8_t* cursor = (uint8_t*)arena;
  proc->stack = (Val*)cursor;
  cursor += stack_size * sizeof(Val);
  proc->call_stack = (uint16_t*)cursor;
  cursor += call_stack_size * sizeof(uint16_t);
  proc->heap = (int32_t*)cursor;

  proc->arena = arena;
  proc->arena_size = arena_size;
  proc->stack_size = stack_size;
  proc->call_stack_size = call_stack_size;
  proc->heap_slots = heap_slots;

  proc->bytecode = bytecode;
  proc->bytecode_len = len;
  proc->steps_per_quantum = 512;
  proc->permissions = 0xFF;
  proc->name = strdup(name);
  proc->process_id = next_process_id++;
  if (next_process_id == 0) next_process_id = 1;

  Serial.printf("[FVM] Processo criado: %s  arena=%dB stack=%d heap=%d\n",
                name, arena_size, stack_size, heap_slots);
  return proc;
}

FvmProcess* fvm_process_create(const char* name,
                               const uint8_t* bytecode, uint16_t len) {
  return fvm_process_create_ex(name, bytecode, len,
                               FVM_STACK_SIZE_DEFAULT,
                               FVM_CALL_STACK_SIZE_DEFAULT,
                               FVM_HEAP_SLOTS_DEFAULT);
}

void fvm_process_destroy(FvmProcess* proc) {
  if (!proc) return;
  if (proc->name) free((void*)proc->name);
  if (proc->string_table) free(proc->string_table);
  if (proc->arena) free(proc->arena);  // libera stack+call_stack+heap de uma vez
  for (int i = 0; i < proc->n_cap_states; i++)
    if (proc->cap_states[i].data) free(proc->cap_states[i].data);
  free(proc);
}

// ── loader .fvm — suporta FVM1 e FVM2 ────────────────────────

FvmProcess* fvm_load_from_memory(const char* name,
                                 const uint8_t* data, uint16_t total_len) {
  Serial.printf("[FVM] Carregando: %s (%d bytes)\n", name, total_len);

  if (total_len < 7) {
    Serial.println("[FVM] Muito curto");
    return nullptr;
  }

  bool is_v2 = (memcmp(data, FVM_FORMAT_V2, 4) == 0);
  bool is_v1 = (memcmp(data, FVM_FORMAT_V1, 4) == 0);
  if (!is_v1 && !is_v2) {
    Serial.println("[FVM] Assinatura inválida");
    return nullptr;
  }

  uint16_t ptr = 4;

  // ── parse do header dinâmico (FVM2) ──────────────────────────
  uint16_t stack_size = FVM_STACK_SIZE_DEFAULT;
  uint8_t call_stack_size = FVM_CALL_STACK_SIZE_DEFAULT;
  uint8_t heap_slots = FVM_HEAP_SLOTS_DEFAULT;
  uint16_t ram_req = 0;

  if (is_v2) {
    if (total_len < 11) {
      Serial.println("[FVM] Header FVM2 incompleto");
      return nullptr;
    }
    // [FLAGS:1][RAM_REQ:2 BE][STACK_SIZE:1][HEAP_SLOTS:1]
    uint8_t flags = data[ptr++];
    ram_req = (uint16_t)((data[ptr] << 8) | data[ptr + 1]);
    ptr += 2;
    stack_size = data[ptr++];
    heap_slots = data[ptr++];
    (void)flags;

    if (stack_size == 0) stack_size = FVM_STACK_SIZE_DEFAULT;
    if (heap_slots == 0) heap_slots = FVM_HEAP_SLOTS_DEFAULT;

    // clamp
    if (stack_size > FVM_STACK_SIZE_MAX) stack_size = FVM_STACK_SIZE_MAX;
    if (heap_slots > FVM_HEAP_SLOTS_MAX) heap_slots = FVM_HEAP_SLOTS_MAX;

    // verificação de RAM disponível
    if (ram_req > 0) {
      size_t free_ram = ESP.getFreeHeap();
      if ((uint32_t)ram_req > free_ram) {
        Serial.printf("[FVM] OOM: app precisa de %dB, disponível %dB\n",
                      ram_req, free_ram);
        return nullptr;
      }
      Serial.printf("[FVM] RAM req=%dB disponível=%dB  OK\n", ram_req, free_ram);
    }
  }

  // ── parse da string table (comum FVM1/FVM2) ───────────────────
  uint8_t n_strings = data[ptr++];
  const char** string_table = nullptr;
  if (n_strings > 0) {
    string_table = (const char**)malloc(n_strings * sizeof(char*));
    if (!string_table) return nullptr;
    for (int i = 0; i < n_strings; i++) {
      if (ptr >= total_len) {
        free(string_table);
        return nullptr;
      }
      string_table[i] = (const char*)&data[ptr];
      ptr += strlen(string_table[i]) + 1;
    }
  }

  if (ptr + 2 > total_len) {
    free(string_table);
    return nullptr;
  }
  uint16_t bc_size = (uint16_t)((data[ptr] << 8) | data[ptr + 1]);
  ptr += 2;
  Serial.printf("[FVM] Bytecode offset:%d size:%d\n", ptr, bc_size);

  const uint8_t* bytecode_ptr = &data[ptr];
  FvmProcess* proc = fvm_process_create_ex(name, bytecode_ptr, bc_size,
                                           stack_size, call_stack_size, heap_slots);
  if (proc) {
    proc->string_table = string_table;
    proc->n_strings = n_strings;
  } else {
    if (string_table) free(string_table);
  }
  return proc;
}

// ── capability state ──────────────────────────────────────────

void** fvm_get_cap_state(FvmProcess* proc, const char* capability) {
  if (!proc || !capability) return nullptr;
  for (int i = 0; i < proc->n_cap_states; i++)
    if (strcmp(proc->cap_states[i].capability, capability) == 0)
      return &proc->cap_states[i].data;
  if (proc->n_cap_states >= MAX_CAPABILITY_STATES) return nullptr;
  int idx = proc->n_cap_states++;
  proc->cap_states[idx].capability = capability;
  proc->cap_states[idx].data = nullptr;
  return &proc->cap_states[idx].data;
}

CallerContext fvm_make_caller(FvmProcess* proc, const char* capability) {
  CallerContext ctx;
  ctx.type = CALLER_FVM;
  ctx.state = fvm_get_cap_state(proc, capability);
  ctx.fvm_proc = (void*)proc;
  return ctx;
}

void fvm_process_reset(FvmProcess* proc) {
  if (!proc) return;
  proc->pc = 0;
  proc->sp = 0;
  proc->csp = 0;
  proc->halted = false;
  proc->error = false;
  proc->error_code = FVM_ERR_NONE;
  if (proc->arena) memset(proc->arena, 0, proc->arena_size);
}
