#pragma once
// shell_device.h
// Capabilities de suporte ao FetShell.
// Registradas como device id 11.
//
// Capabilities expostas ao FetScript:
//
//   shell:write_line      id(int) text(str)
//     Escreve diretamente numa linha do histórico (textbuf id 0-3).
//     Usado na inicialização para mensagem de boas-vindas.
//
//   shell:scroll_and_write   text(str)
//     Rola o histórico (buf[3]←buf[2]←buf[1]←buf[0]) e
//     escreve 'text' no buf[0] (linha mais recente).
//
//   shell:scroll_and_write_num   label(str) v(int)
//     Igual ao anterior mas concatena label + valor numérico.
//     Ex: "free:" + 42000 → "free:42000"
//
//   shell:set_input  c0..c19(int) len(int)
//     Recebe os 20 slots de char do input FetScript e reconstrói
//     o buffer interno de input (s_input_buf).
//
//   shell:draw_input   x(int) y(int)
//     Desenha o conteúdo de s_input_buf no display na posição dada.
//     Não faz flush — chamado dentro de draw().
//
//   shell:fs_count   → int
//     Retorna quantos arquivos existem no LittleFS.
//
//   shell:fs_name_to_buf   idx(int)
//     Copia o nome do arquivo [idx] para o textbuf id 0 (linha mais recente).
//     O script então chama output() logo após para rolar + exibir.
//
//   shell:fs_size   idx(int)  → int
//     Retorna o tamanho em bytes do arquivo [idx].
//
//   shell:fs_format
//     Formata o LittleFS (LittleFS.format()).
//
//   shell:mem_free  → int
//     ESP.getFreeHeap()
//
//   shell:mem_min   → int
//     ESP.getMinFreeHeap()
//
//   shell:task_count  → int
//     scheduler_task_count()
//
//   shell:task_name_to_buf   idx(int)
//     Copia o nome da task [idx] para o textbuf id 0.
//
//   shell:task_interval   idx(int)  → int
//     Retorna o interval_ms da task [idx].
//
//   shell:kill
//     Aborta o processo FVM atual (proc->halted = true) e vai pro launcher.
//     Seguro chamar do próprio app em foco apenas se ele quiser sair —
//     normalmente chamado de um app DIFERENTE (o shell manda matar outro).
//     Por ora: seta halted=true no current_app se for FVM.

#include "device.h"
#include "capability.h"

void shell_device_init(Device* dev);