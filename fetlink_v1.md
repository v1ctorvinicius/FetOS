# FetLink v1 — Implementação base (C + Python)

> Complemento prático da spec JSON v1.0 – stubs e simulador para integração MCU ↔ PC.

---

## 🧩 1. Biblioteca C (`fetlink.h` / `fetlink.c`)

### 1.1 Arquitetura

```
FetOS ↔ UART ↔ fetlink_rx_line() → parse JSON → dispatch(cmd)
                                   ↳ fetlink_send(obj)
```

### 1.2 `fetlink.h`

```c
#ifndef FETLINK_H
#define FETLINK_H
#include <stdint.h>
#include <stdbool.h>

#define FETLINK_RX_MAX 384
#define FETLINK_TX_MAX 384

typedef struct {
    char cmd[12];
    char src[8];
    uint16_t id;
    bool ack;
    char payload[FETLINK_RX_MAX];
} FetMsg;

void fetlink_init(void (*tx_func)(const char*));
void fetlink_rx_char(char c);
void fetlink_send_text(uint8_t x, uint8_t y, const char* text);
void fetlink_send_ping(uint16_t id);
void fetlink_tick(void); // opcional para timeout/keepalive

#endif
```

### 1.3 `fetlink.c`

```c
#include "fetlink.h"
#include <stdio.h>
#include <string.h>

static void (*tx)(const char*) = 0;
static char rx_buf[FETLINK_RX_MAX];
static uint16_t rx_len = 0;

void fetlink_init(void (*tx_func)(const char*)) {
    tx = tx_func;
    rx_len = 0;
}

void fetlink_rx_char(char c) {
    if (c == '\n') {
        rx_buf[rx_len] = 0;
        // Aqui chamar parser simples (usar cJSON ou parse manual leve)
        if (strstr(rx_buf, "PING")) {
            char resp[64];
            sprintf(resp, "{\"cmd\":\"PONG\"}\n");
            tx(resp);
        }
        rx_len = 0;
    } else if (rx_len < FETLINK_RX_MAX - 1) {
        rx_buf[rx_len++] = c;
    }
}

void fetlink_send_text(uint8_t x, uint8_t y, const char* text) {
    if (!tx) return;
    char msg[FETLINK_TX_MAX];
    snprintf(msg, sizeof(msg),
        "{\"cmd\":\"TEXT\",\"x\":%d,\"y\":%d,\"text\":\"%s\"}\n", x, y, text);
    tx(msg);
}

void fetlink_send_ping(uint16_t id) {
    if (!tx) return;
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"cmd\":\"PING\",\"id\":%u}\n", id);
    tx(msg);
}

void fetlink_tick(void) {
    // TODO: watchdog, timeout de handshake, etc.
}
```

---

## 🧠 2. Driver Python (`fetlink.py`)

```python
import json, serial, time, threading

class FetLink:
    def __init__(self, port="/dev/ttyACM0", baud=115200, cb=None):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.cb = cb
        self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self.rx_thread.start()

    def _rx_loop(self):
        buf = b""
        while True:
            c = self.ser.read()
            if not c:
                continue
            if c == b"\n":
                try:
                    obj = json.loads(buf.decode())
                    if self.cb:
                        self.cb(obj)
                except Exception as e:
                    print("[RX ERR]", e, buf)
                buf = b""
            else:
                buf += c

    def send(self, obj: dict):
        line = json.dumps(obj, separators=(',', ':')) + "\n"
        self.ser.write(line.encode())

    def ping(self):
        self.send({"cmd": "PING", "id": int(time.time()*1000) & 0xFFFF})

    def text(self, x, y, text):
        self.send({"cmd": "TEXT", "x": x, "y": y, "text": text})

    def clear(self):
        self.send({"cmd": "CLEAR"})

    def draw_pixel(self, x, y, v=1):
        self.send({"cmd": "DRAW", "op": "PIXEL", "x": x, "y": y, "v": v})
```

---

## 📺 3. TV Simulator (`tv_sim.py`)

```python
from rich.console import Console
from rich.panel import Panel
from fetlink import FetLink

class FetTV:
    def __init__(self, w=16, h=2):
        self.console = Console()
        self.w, self.h = w, h
        self.buf = [[" " for _ in range(w)] for _ in range(h)]

    def update(self, msg):
        if msg.get("cmd") == "TEXT":
            x, y = msg["x"], msg["y"]
            for i, ch in enumerate(msg["text"]):
                if 0 <= x+i < self.w and 0 <= y < self.h:
                    self.buf[y][x+i] = ch
        elif msg.get("cmd") == "CLEAR":
            self.buf = [[" " for _ in range(self.w)] for _ in range(self.h)]
        self.draw()

    def draw(self):
        lines = ["".join(row) for row in self.buf]
        self.console.clear()
        self.console.print(Panel("\n".join(lines), title="FetHub TV"))

if __name__ == "__main__":
    tv = FetTV(16, 2)
    link = FetLink("/dev/ttyACM0", 115200, cb=tv.update)
    print("[TV Simulator] Running...")
    while True:
        time.sleep(1)
```

---

## 🧱 4. Integração

* **FetOS:** chama `fetlink_init(uart_tx)` e alimenta `fetlink_rx_char()` no ISR de RX.
* **FetHub:** roda `python tv_sim.py`, recebe comandos e mostra LCD em tempo real.

### Futuro

* Adicionar suporte OLED (128x64, mapa 8 páginas).
* Converter `DRAW` em render gráfico no console (rich + unicode blocks).
* Sincronizar `SYSINFO` e `EVT` em painel lateral.

---

✅ **Com isso, FetLink está pronto para integração real MCU ↔ Hub com simulação LCD.**
