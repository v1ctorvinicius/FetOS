#pragma once
#include <Arduino.h>

void persistence_init();
void persistence_write_int(const char* key, int value);
int persistence_read_int(const char* key, int default_value);
void persistence_write_string(const char* key, const char* value);
void persistence_read_string(const char* key, char* buffer, int max_len);