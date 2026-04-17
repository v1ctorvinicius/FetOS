// shell_device.cpp
#include "shell_device.h"
#include "system.h"
#include "display_hal.h"
#include "text_buffer_device.h"
#include "scheduler.h"
#include "fvm_app.h"
#include "fvm_process.h"
#include "launcher.h"
#include "Arduino.h"
#include <LittleFS.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define D DISPLAY_DEFAULT_ID

// ── Buffer de input interno ───────────────────────────────────
// Espelho em C do input que o FetScript mantém no heap.
// Atualizado via shell:set_input a cada keystroke.

static char s_input_buf[22];  // 21 chars + null
static uint8_t s_input_len = 0;

// ── Cache do índice de arquivos (para ls incremental) ─────────
// Montamos a lista uma vez por comando ls e iteramos por índice.
// LittleFS não tem acesso por índice, então varremos até idx.

// Abre o arquivo na posição idx do LittleFS e preenche name+size.
// Retorna false se idx >= total de arquivos.
static bool fs_get_entry(int idx, char* name_out, size_t name_max, size_t* size_out) {
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) return false;

  int i = 0;
  File f = root.openNextFile();
  while (f) {
    if (i == idx) {
      strncpy(name_out, f.name(), name_max - 1);
      name_out[name_max - 1] = '\0';
      *size_out = f.size();
      f.close();
      root.close();
      return true;
    }
    i++;
    f.close();
    f = root.openNextFile();
  }
  root.close();
  return false;
}

static int fs_count() {
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) return 0;
  int count = 0;
  File f = root.openNextFile();
  while (f) {
    count++;
    f.close();
    f = root.openNextFile();
  }
  root.close();
  return count;
}

// ── Helpers de textbuf ────────────────────────────────────────
// Wrapping de textbuf_raw_insert para escrever strings inteiras.

static void tbuf_write(uint8_t id, const char* str) {
  // Limpa o buffer antes — shell:scroll_and_write já fez o scroll,
  // aqui só escrevemos no slot 0 que foi limpo pelo scroll.
  // Para shell:write_line (init), o chamador escolhe o id.
  RequestParam params[2];
  params[0].key = "id";
  params[0].int_value = id;
  params[0].str_value = nullptr;
  RequestPayload clr = { params, 1 };
  capability_request_native("textbuf:clear", &clr);

  // Insere char por char
  int len = strlen(str);
  for (int i = 0; i < len && i < 21; i++) {
    textbuf_raw_insert(id, str[i]);
  }
}

// Rola o histórico: buf[3]←buf[2]←buf[1]←buf[0]
// e escreve str no buf[0].
static void scroll_and_write(const char* str) {
  // Copia: 0→1, 1→2, 2→3 (3 é o mais antigo, descartado)
  // Usamos textbuf_get_buffer que retorna o ponteiro direto.
  const char* b0 = textbuf_get_buffer(0);
  const char* b1 = textbuf_get_buffer(1);
  const char* b2 = textbuf_get_buffer(2);

  char tmp1[22], tmp2[22], tmp3[22];
  strncpy(tmp1, b0, 21);
  tmp1[21] = '\0';
  strncpy(tmp2, b1, 21);
  tmp2[21] = '\0';
  strncpy(tmp3, b2, 21);
  tmp3[21] = '\0';

  tbuf_write(3, tmp3);
  tbuf_write(2, tmp2);
  tbuf_write(1, tmp1);
  tbuf_write(0, str);
}

// ── Handlers ──────────────────────────────────────────────────

static const char* get_str(const RequestPayload* p, const char* key) {
  const RequestParam* r = payload_get(p, key);
  return (r && r->str_value) ? r->str_value : nullptr;
}
static int32_t get_int(const RequestPayload* p, const char* key, int32_t def) {
  const RequestParam* r = payload_get(p, key);
  return r ? r->int_value : def;
}

// shell:write_line  id(int) text(str)
static RequestResult h_write_line(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int id = get_int(p, "id", 0);
  const char* t = get_str(p, "text");
  if (!t || id < 0 || id > 3) return REQ_IGNORED;
  tbuf_write((uint8_t)id, t);
  return REQ_ACCEPTED;
}

// shell:scroll_and_write  text(str)
static RequestResult h_scroll_write(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  const char* t = get_str(p, "text");
  if (!t) return REQ_IGNORED;
  scroll_and_write(t);
  return REQ_ACCEPTED;
}

// shell:scroll_and_write_num  label(str) v(int)
static RequestResult h_scroll_write_num(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  const char* label = get_str(p, "label");
  int32_t val = get_int(p, "v", 0);
  char buf[22];
  if (label) {
    snprintf(buf, sizeof(buf), "%s%ld", label, (long)val);
  } else {
    snprintf(buf, sizeof(buf), "%ld", (long)val);
  }
  scroll_and_write(buf);
  return REQ_ACCEPTED;
}

// shell:set_input  c0..c19(int) len(int)
// O FetScript passa todos os 20 slots de char de uma vez.
// Parâmetros: "c0".."c19" + "len"
// Total: 21 pares chave-valor — dentro do limite de 7 pares do SYS_REQ?
// NÃO — o SYS_REQ tem limite de 7 params (i < 7 no loop).
// Solução: dividir em dois calls: shell:set_input_a (c0-c9) e shell:set_input_b (c10-c19+len)
// Veja o .fets ajustado para usar duas chamadas.

static char s_inp_chars[20];  // buffer temporário

static RequestResult h_set_input_a(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  s_inp_chars[0] = (char)get_int(p, "c0", 0);
  s_inp_chars[1] = (char)get_int(p, "c1", 0);
  s_inp_chars[2] = (char)get_int(p, "c2", 0);
  s_inp_chars[3] = (char)get_int(p, "c3", 0);
  s_inp_chars[4] = (char)get_int(p, "c4", 0);
  s_inp_chars[5] = (char)get_int(p, "c5", 0);
  s_inp_chars[6] = (char)get_int(p, "c6", 0);
  return REQ_ACCEPTED;
}

static RequestResult h_set_input_b(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  s_inp_chars[7] = (char)get_int(p, "c7", 0);
  s_inp_chars[8] = (char)get_int(p, "c8", 0);
  s_inp_chars[9] = (char)get_int(p, "c9", 0);
  s_inp_chars[10] = (char)get_int(p, "c10", 0);
  s_inp_chars[11] = (char)get_int(p, "c11", 0);
  s_inp_chars[12] = (char)get_int(p, "c12", 0);
  s_inp_chars[13] = (char)get_int(p, "c13", 0);
  return REQ_ACCEPTED;
}

static RequestResult h_set_input_c(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  s_inp_chars[14] = (char)get_int(p, "c14", 0);
  s_inp_chars[15] = (char)get_int(p, "c15", 0);
  s_inp_chars[16] = (char)get_int(p, "c16", 0);
  s_inp_chars[17] = (char)get_int(p, "c17", 0);
  s_inp_chars[18] = (char)get_int(p, "c18", 0);
  s_inp_chars[19] = (char)get_int(p, "c19", 0);
  uint8_t len = (uint8_t)get_int(p, "len", 0);
  if (len > 20) len = 20;
  s_input_len = len;
  // Reconstrói s_input_buf
  memcpy(s_input_buf, s_inp_chars, len);
  s_input_buf[len] = '\0';
  return REQ_ACCEPTED;
}

// shell:draw_input  x(int) y(int)
// Desenha s_input_buf na posição dada. Chamado de dentro do draw().
static RequestResult h_draw_input(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int x = get_int(p, "x", 8);
  int y = get_int(p, "y", 55);
  if (s_input_len > 0) {
    hal_text(D, (int16_t)x, (int16_t)y, s_input_buf, 1);
  }
  return REQ_ACCEPTED;
}

// shell:fs_count  → int
static RequestResult h_fs_count(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  p->params[0].int_value = fs_count();
  return REQ_ACCEPTED;
}

// shell:fs_name_to_buf  idx(int)
// Escreve o nome do arquivo no textbuf id=0 para ser rolado+exibido pelo script.
static RequestResult h_fs_name_to_buf(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int idx = get_int(p, "idx", 0);
  char name[32];
  size_t sz = 0;
  if (fs_get_entry(idx, name, sizeof(name), &sz)) {
    // Trunca se necessário para caber em 21 chars
    char display[22];
    snprintf(display, sizeof(display), "%s", name);
    tbuf_write(0, display);
  } else {
    tbuf_write(0, "");
  }
  return REQ_ACCEPTED;
}

// shell:fs_size  idx(int)  → int
static RequestResult h_fs_size(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int idx = get_int(p, "idx", 0);
  char name[32];
  size_t sz = 0;
  fs_get_entry(idx, name, sizeof(name), &sz);
  p->params[0].int_value = (int32_t)sz;
  return REQ_ACCEPTED;
}

// shell:fs_format
static RequestResult h_fs_format(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)p;
  (void)c;
  Serial.println("[Shell] Formatando LittleFS...");
  LittleFS.format();
  LittleFS.begin(true);
  Serial.println("[Shell] LittleFS formatado.");
  return REQ_ACCEPTED;
}

// shell:mem_free  → int
static RequestResult h_mem_free(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  p->params[0].int_value = (int32_t)ESP.getFreeHeap();
  return REQ_ACCEPTED;
}

// shell:mem_min  → int
static RequestResult h_mem_min(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  p->params[0].int_value = (int32_t)ESP.getMinFreeHeap();
  return REQ_ACCEPTED;
}

// shell:task_count  → int
static RequestResult h_task_count(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  p->params[0].int_value = scheduler_task_count();
  return REQ_ACCEPTED;
}

// shell:task_name_to_buf  idx(int)
static RequestResult h_task_name_to_buf(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int idx = get_int(p, "idx", 0);
  const Task* t = scheduler_get_task(idx);
  if (t && t->active && t->name) {
    char buf[22];
    snprintf(buf, sizeof(buf), "%s", t->name);
    tbuf_write(0, buf);
  } else {
    tbuf_write(0, "");
  }
  return REQ_ACCEPTED;
}

// shell:task_interval  idx(int)  → int
static RequestResult h_task_interval(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int idx = get_int(p, "idx", 0);
  const Task* t = scheduler_get_task(idx);
  p->params[0].int_value = (t && t->active) ? (int32_t)t->interval : -1;
  return REQ_ACCEPTED;
}

// shell:kill
// Aborta o app FVM atual (se houver) e volta ao launcher.
static RequestResult h_kill(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)p;
  (void)c;
  if (current_app && current_app->update_ctx) {
    FvmAppContext* fctx = (FvmAppContext*)current_app->update_ctx;
    if (fctx->proc) {
      fctx->proc->halted = true;
      Serial.printf("[Shell] kill: abortado '%s'\n", fctx->proc->name);
    }
  }
  // Nota: não chama system_set_app aqui — deixa o próprio fvm_update
  // detectar halted=true e lidar. O script pode chamar system:exit depois.
  return REQ_ACCEPTED;
}

// shell:ls_entry  idx(int)
// Combina nome+tamanho numa única linha formatada e rola o histórico.
// Evita a condição de corrida de escrever no buf[0] duas vezes.
static RequestResult h_ls_entry(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int idx = get_int(p, "idx", 0);
  char name[32];
  size_t sz = 0;
  if (fs_get_entry(idx, name, sizeof(name), &sz)) {
    char line[22];
    // Formato: "name  NNNb" truncado em 21 chars
    // Calcula espaço disponível para o nome
    char szbuf[8];
    snprintf(szbuf, sizeof(szbuf), "%lub", (unsigned long)sz);
    int sz_len = strlen(szbuf);
    int name_max = 21 - sz_len - 1;  // 1 espaço entre nome e tamanho
    if (name_max < 1) name_max = 1;
    snprintf(line, sizeof(line), "%-*.*s %s", name_max, name_max, name, szbuf);
    scroll_and_write(line);
  }
  return REQ_ACCEPTED;
}

// shell:ps_entry  idx(int)
// Combina nome da task + interval numa única linha e rola o histórico.
static RequestResult h_ps_entry(Device* dev, const RequestPayload* p, CallerContext* c) {
  (void)dev;
  (void)c;
  int idx = get_int(p, "idx", 0);
  const Task* t = scheduler_get_task(idx);
  if (t && t->active && t->name) {
    char line[22];
    char ivbuf[8];
    snprintf(ivbuf, sizeof(ivbuf), "%lums", (unsigned long)t->interval);
    int iv_len = strlen(ivbuf);
    int name_max = 21 - iv_len - 1;
    if (name_max < 1) name_max = 1;
    snprintf(line, sizeof(line), "%-*.*s %s", name_max, name_max, t->name, ivbuf);
    scroll_and_write(line);
  }
  return REQ_ACCEPTED;
}

// ── Init ──────────────────────────────────────────────────────

void shell_device_init(Device* dev) {
  dev->state = nullptr;
  memset(s_input_buf, 0, sizeof(s_input_buf));
  memset(s_inp_chars, 0, sizeof(s_inp_chars));
  s_input_len = 0;

  system_register_capability("shell:write_line", h_write_line, dev);
  system_register_capability("shell:scroll_and_write", h_scroll_write, dev);
  system_register_capability("shell:scroll_and_write_num", h_scroll_write_num, dev);
  system_register_capability("shell:set_input_a", h_set_input_a, dev);
  system_register_capability("shell:set_input_b", h_set_input_b, dev);
  system_register_capability("shell:set_input_c", h_set_input_c, dev);
  system_register_capability("shell:draw_input", h_draw_input, dev);
  system_register_capability("shell:fs_count", h_fs_count, dev);
  system_register_capability("shell:fs_name_to_buf", h_fs_name_to_buf, dev);
  system_register_capability("shell:fs_size", h_fs_size, dev);
  system_register_capability("shell:fs_format", h_fs_format, dev);
  system_register_capability("shell:mem_free", h_mem_free, dev);
  system_register_capability("shell:mem_min", h_mem_min, dev);
  system_register_capability("shell:task_count", h_task_count, dev);
  system_register_capability("shell:task_name_to_buf", h_task_name_to_buf, dev);
  system_register_capability("shell:task_interval", h_task_interval, dev);
  system_register_capability("shell:kill", h_kill, dev);

  system_register_capability("shell:ls_entry", h_ls_entry, dev);
  system_register_capability("shell:ps_entry", h_ps_entry, dev);

  Serial.println("[Shell] device iniciado");
}