#include "iom.h"
#include <stddef.h>

static IOMEntry devs[MAX_DEVICES];

void iom_init(void) {
  for (int i = 0; i < MAX_DEVICES; i++) {
      devs[i].owner = NULL;
      devs[i].shared = false;
      devs[i].usage_count = 0;
  }
}

bool iom_request(FetApp* app, uint8_t dev) {
  (void)app; (void)dev;
  return true;
}

void iom_release(FetApp* app, uint8_t dev) {
  (void)app; (void)dev;
}

bool iom_owned(FetApp* app, uint8_t dev) {
  (void)app; (void)dev;
  return true;
}
