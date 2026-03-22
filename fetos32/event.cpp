#include "event.h"

static Event event_queue[EVENT_QUEUE_SIZE];
static int head = 0;
static int tail = 0;

void event_init() {
  head = 0;
  tail = 0;
}

bool event_push(Event e) {
  int next = (head + 1) % EVENT_QUEUE_SIZE;
  if (next == tail) return false;

  event_queue[head] = e;
  head = next;
  return true;
}

bool event_pop(Event* e) {
  if (head == tail) return false;

  *e = event_queue[tail];
  tail = (tail + 1) % EVENT_QUEUE_SIZE;
  return true;
}