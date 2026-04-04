#pragma once
#include "display_hal.h"

// LCD 16x2 paralelo (6 pinos: RS, EN, D4-D7)
// Usa a biblioteca LiquidCrystal padrão do Arduino
// Requer: #include <LiquidCrystal.h> na lib (já inclusa no ESP32 Arduino core)
DisplayDriver* driver_lcd1602_create(int rs, int en,
                                      int d4, int d5, int d6, int d7);