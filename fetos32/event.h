#pragma once
#include <Arduino.h>

#define EVENT_QUEUE_SIZE 16
#define EVENT_TEXT_SIZE 64

typedef enum {
  EVENT_NONE = 0,
  EVENT_BUTTON_PRESS,
  EVENT_TEXT,
  EVENT_KEY
} EventType;

typedef struct {
  EventType type;
  uint8_t device_id;
  bool has_payload;

  union {
    int value;
    char text[EVENT_TEXT_SIZE];
    char key_char;
  } payload;

} Event;

void event_init();
bool event_push(Event e);
bool event_pop(Event* e);
void event_clear();