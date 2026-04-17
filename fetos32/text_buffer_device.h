//text_buffer_device.h
#pragma once
#include "device.h"
#include "capability.h"

// ── Text Buffer Device ────────────────────────────────────────
// Capabilities registradas com wildcard "textbuf.*:op"
// permitindo múltiplos buffers por processo (textbuf.doc, textbuf.clip...)
//
// Capabilities disponíveis:
//
//   textbuf.<nome>:alloc   size(int)
//     Aloca um buffer de texto de 'size' bytes para este processo.
//     Deve ser chamado antes de qualquer outra op.
//     Retorna: 0 (REQ_ACCEPTED) ou REQ_OOM
//
//   textbuf.<nome>:free
//     Libera o buffer e zera o estado.
//
//   textbuf.<nome>:insert   pos(int) char(int)
//     Insere o byte 'char' na posição 'pos' (0-based).
//     Faz memmove para abrir espaço.
//     Retorna: posição após o caractere inserido, ou -1 se cheio.
//
//   textbuf.<nome>:delete   pos(int)
//     Remove o byte na posição 'pos'. Faz memmove para fechar.
//     Retorna: novo tamanho do buffer.
//
//   textbuf.<nome>:get_char   pos(int)
//     Retorna o byte na posição 'pos', ou -1 se fora dos limites.
//
//   textbuf.<nome>:size
//     Retorna o número de bytes atualmente no buffer.
//
//   textbuf.<nome>:capacity
//     Retorna a capacidade total alocada.
//
//   textbuf.<nome>:seek_line   line(int)
//     Retorna o offset (posição em bytes) do início da linha 'line' (0-based).
//     Varre o buffer contando '\n'. O(n) mas chamado pelo app no scroll.
//     Retorna: offset, ou -1 se a linha não existir.
//
//   textbuf.<nome>:line_count
//     Retorna o número total de linhas no buffer.
//
//   textbuf.<nome>:clear
//     Esvazia o buffer (size = 0) sem desalocar.

void textbuf_device_init(Device* dev);
const char* textbuf_get_buffer(uint8_t id);
void textbuf_raw_insert(uint8_t id, char c);