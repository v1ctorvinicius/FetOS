#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer0.h"

volatile uint32_t fetos_millis = 0;

void timer0_init(void) {
    // CTC mode
    TCCR0A = (1 << WGM01);
    // Prescaler 64
    TCCR0B = (1 << CS01) | (1 << CS00);
    // Comparação para 1 kHz
    OCR0A = 249;
    // Habilita interrupção
    TIMSK0 = (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect) {
    fetos_millis++;
}

uint32_t millis(void) {
    uint32_t m;
    cli();
    m = fetos_millis;
    sei();
    return m;
}

uint32_t micros_approx(void) {
    // micros aproximado = millis * 1000 + (TCNT0 * (64 / 16 MHz))
    // 64 / 16 MHz = 4 µs por tick do Timer0
    return millis() * 1000UL + (TCNT0 * 4);
}
