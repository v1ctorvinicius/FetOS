//ui_device.h
#pragma once
#include "device.h"
#include "capability.h"

// ── UI Device ─────────────────────────────────────────────────
// Capabilities de alto nível para uso em FetScript/FVM.
// Todas operam via display_hal no DISPLAY_DEFAULT_ID.
//
// ui:clear
// ui:flush
// ui:text        x(int) y(int) text(str) size(int=1)
// ui:text_center y(int) text(str) size(int=1)
// ui:text_invert x(int) y(int) text(str) size(int=1)
// ui:rect        x(int) y(int) w(int) h(int) fill(int=1)
// ui:header      text(str)           — linha 0..9, separador em y=9
// ui:footer      text(str)           — linha 55..63, separador em y=54
// ui:list        items(str,\n-sep) selected(int) start_y(int) visible(int)
// ui:cursor      x(int) y(int)       — cursor piscante (bloco 3x8)
// ui:invert_row  y(int) h(int=8)     — inverte retângulo linha inteira

void ui_device_init(Device* dev);