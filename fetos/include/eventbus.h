#ifndef EVENTBUS_H
#define EVENTBUS_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint8_t type;
  uint8_t src;
  uint8_t dst;
  uint8_t data[8];
} Event;

void eventbus_init(void);
bool event_post(Event* ev);
bool event_poll(Event* ev_out);

#endif
