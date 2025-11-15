#ifndef TIMER0_H
#define TIMER0_H

#include <stdint.h>

void timer0_init(void);
uint32_t millis(void);
uint32_t micros_approx(void);

#endif
