#include "persistence.h"
#include <Preferences.h>

static Preferences prefs;

void persistence_init() {
  prefs.begin("fetos_storage", false);
  prefs.end();
}

void persistence_write_int(const char* key, int value) {
  prefs.begin("fetos_storage", false);
  prefs.putInt(key, value);
  prefs.end();
}

int persistence_read_int(const char* key, int default_value) {
  prefs.begin("fetos_storage", true);
  int value = prefs.getInt(key, default_value);
  prefs.end();
  return value;
}

void persistence_write_string(const char* key, const char* value) {
  prefs.begin("fetos_storage", false);
  prefs.putString(key, value);
  prefs.end();
}

void persistence_read_string(const char* key, char* buffer, int max_len) {
  prefs.begin("fetos_storage", true);
  String s = prefs.getString(key, "");
  s.toCharArray(buffer, max_len);
  prefs.end();
}