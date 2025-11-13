#ifndef HAL_H
#define HAL_H
#include <stdint.h>
#include <stdbool.h>

void hal_init(void);
void hal_tick(void);

void hal_led_set(uint8_t id, bool on);
void hal_oled_draw(uint8_t* buffer);
uint16_t hal_adc_read(uint8_t ch);

#endif
