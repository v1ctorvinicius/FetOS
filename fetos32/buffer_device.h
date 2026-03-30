#pragma once
#include "device.h"
#include "capability.h"

#define BUFFER_DEFAULT_CAPACITY 32

typedef struct
{
  int32_t values[BUFFER_DEFAULT_CAPACITY];
  uint8_t size;
  uint8_t capacity;
} BufferState;

void buffer_device_init(Device *dev);

RequestResult buffer_handle_push(Device *dev, const RequestPayload *payload, CallerContext *caller);
RequestResult buffer_handle_avg(Device *dev, const RequestPayload *payload, CallerContext *caller);
RequestResult buffer_handle_clear(Device *dev, const RequestPayload *payload, CallerContext *caller);
RequestResult buffer_handle_size(Device *dev, const RequestPayload *payload, CallerContext *caller);
RequestResult buffer_handle_last(Device *dev, const RequestPayload *payload, CallerContext *caller);