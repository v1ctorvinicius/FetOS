// fetlink_ble.h
// Adaptador BLE para o FetLink — Nordic UART Service (NUS)
// UUIDs compatíveis com qualquer cliente BLE que suporte NUS
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "button.h"


void fetlink_ble_init();

// chamado internamente pelo RxCallbacks — não usar diretamente
void fetlink_ble_on_rx(const uint8_t* data, uint16_t len);