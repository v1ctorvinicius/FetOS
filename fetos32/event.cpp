#include "event.h"

static Event event_queue[EVENT_QUEUE_SIZE];
int head = 0;
int tail = 0;

void event_init() {
  head = 0;
  tail = 0;
}

bool event_push(Event e) {
  Serial.println("[!] EVENT PUSH");
  Serial.print(F("DEVICE ID: "));
  Serial.print(e.device_id);
  Serial.print(F(" | Type: "));
  Serial.print(e.type);
  Serial.print(F(" | Payload: "));
  Serial.println(e.has_payload ? "true" : "false");
  Serial.print(F(" | Value: "));
  Serial.print(e.payload.value);
  Serial.println("");

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

void event_clear() {
  head = 0;
  tail = 0;
}