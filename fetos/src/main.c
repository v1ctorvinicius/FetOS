#include <avr/io.h>
#include <util/delay.h>
#include "fetlink.h"
#include "fetos.h"

int main(void) {
    DDRB |= (1 << PB5); // LED builtin

    fetlink_init();
    fetos_init();

    while (1) {
        // LED piscando = heartbeat
        PORTB ^= (1 << PB5);
        _delay_ms(500);

        // tick principal
        fetos_tick();
    }
}
