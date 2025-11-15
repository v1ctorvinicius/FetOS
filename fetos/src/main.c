#include <avr/io.h>
#include <avr/interrupt.h>
#include "fetlink.h"
#include "fetos.h"
#include "timer0.h"

int main(void) {
    DDRB |= (1 << PB5); // LED onboard

    timer0_init();
    sei();

    fetlink_init();
    fetos_init();

    uint32_t last_blink = 0;

    while (1) {
        uint32_t now = millis();

        // LED pisca no ritmo correto
        if (now - last_blink >= 500) {
            PORTB ^= (1 << PB5);
            last_blink = now;
        }

        fetos_tick();   // scheduler + ticks sem bloqueio
    }
}
