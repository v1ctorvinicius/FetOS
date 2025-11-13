#include "eventbus.h"

void eventbus_init(void) {}

bool event_post(Event* ev) {
  (void)ev;
  return true;
}

bool event_poll(Event* ev_out) {
  (void)ev_out;
  return false;
}
