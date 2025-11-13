#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "fetlink.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

typedef struct {
    unsigned long f_cpu;
    unsigned long baud;
    uint8_t u2x;
} UartConfig;

static const UartConfig uart_presets[] = {
    // ---- Clones LGT8F e Unos genéricos ----
    {16000000UL, 115200, 1}, // LGT8F clone com CKDIV8 ativo (tua placa)
    {32000000UL, 115200, 1}, // LGT8F full clock 32MHz
    {16000000UL, 9600,   0}, // Uno / Nano genuíno
    {8000000UL,  9600,   1}, // Pro Mini low-power
    {16000000UL, 57600,  1}, // fallback baud
};

static void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

static void uart_print(const char *s) {
    while (*s) uart_putc(*s++);
}

static void uart_init(const UartConfig* cfg) {
    unsigned int ubrr = (cfg->f_cpu / ((cfg->u2x ? 8UL : 16UL) * cfg->baud)) - 1;
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)(ubrr & 0xFF);
    UCSR0A = cfg->u2x ? (1 << U2X0) : 0;
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void fetlink_send_test(void) {
    uart_print("\r\n[FetLink] TEST\r\n");
}

// ===================================================
//  Auto-detecção de configuração UART
// ===================================================
static const UartConfig* fetlink_autobaud(void) {
    _delay_ms(1500); // tempo pro CDC/CH340 estabilizar
    for (uint8_t i = 0; i < sizeof(uart_presets)/sizeof(UartConfig); i++) {
        const UartConfig* cfg = &uart_presets[i];
        uart_init(cfg);
        fetlink_send_test();
        _delay_ms(50);
        // (futuramente: validar eco via RX)
        return cfg; // por enquanto pega o primeiro válido
    }
    return &uart_presets[0];
}

// ===================================================
//  API pública
// ===================================================
void fetlink_init(void) {
    const UartConfig* cfg = fetlink_autobaud();

    uart_print("[FetLink] READY\r\n");
    uart_print("[INFO] ");
    char info[64];
    snprintf(info, sizeof(info),
             "CPU=%luHz, BAUD=%lu, U2X=%d\r\n",
             cfg->f_cpu, cfg->baud, cfg->u2x);
    uart_print(info);
}

void fetlink_tick(void) {
    static uint16_t tick_counter = 0;
    if (++tick_counter >= 4) {
        tick_counter = 0;
        uart_print("\x02{\"src\":\"fetos:uno\",\"cmd\":\"TEXT\",\"payload\":{\"x\":0,\"y\":0,\"text\":\"TICK\"}}\x03\n");
    }
}
