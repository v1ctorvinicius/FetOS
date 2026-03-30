#include "buffer_device.h"
#include "system.h"
#include <stdlib.h>
#include <string.h>

static BufferState *get_or_init(CallerContext *caller, uint8_t capacity)
{
  if (!caller || !caller->state)
    return nullptr;
  if (caller->type != CALLER_FVM)
    return nullptr;

  if (!(*caller->state))
  {
    BufferState *buf = (BufferState *)malloc(sizeof(BufferState));
    if (!buf)
      return nullptr;

    buf->size = 0;
    buf->capacity = capacity <= BUFFER_DEFAULT_CAPACITY ? capacity : BUFFER_DEFAULT_CAPACITY;
    memset(buf->values, 0, sizeof(buf->values));

    *caller->state = buf;
  }

  return (BufferState *)*caller->state;
}

void buffer_device_init(Device *dev)
{
  dev->state = nullptr;

  system_register_capability("buffer.*:push", buffer_handle_push, dev);
  system_register_capability("buffer.*:avg", buffer_handle_avg, dev);
  system_register_capability("buffer.*:clear", buffer_handle_clear, dev);
  system_register_capability("buffer.*:size", buffer_handle_size, dev);
  system_register_capability("buffer.*:last", buffer_handle_last, dev);
}

RequestResult buffer_handle_push(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  const RequestParam *cap_p = payload_get(payload, "capacity");
  uint8_t capacity = cap_p ? (uint8_t)cap_p->int_value : BUFFER_DEFAULT_CAPACITY;

  BufferState *buf = get_or_init(caller, capacity);
  if (!buf)
    return REQ_OOM;

  const RequestParam *val_p = payload_get(payload, "value");
  if (!val_p)
    return REQ_IGNORED;

  if (buf->size < buf->capacity)
  {
    buf->values[buf->size++] = val_p->int_value;
  }
  else
  {

    memmove(buf->values, buf->values + 1, (buf->capacity - 1) * sizeof(int32_t));
    buf->values[buf->capacity - 1] = val_p->int_value;
  }

  return REQ_ACCEPTED;
}

RequestResult buffer_handle_avg(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  BufferState *buf = get_or_init(caller, BUFFER_DEFAULT_CAPACITY);
  if (!buf)
    return REQ_OOM;
  if (buf->size == 0)
    return REQ_IGNORED;

  int64_t sum = 0;
  for (int i = 0; i < buf->size; i++)
    sum += buf->values[i];

  const RequestParam *result_p = payload_get(payload, "result");
  if (result_p)
  {

    ((RequestParam *)result_p)->int_value = (int32_t)(sum / buf->size);
  }

  return REQ_ACCEPTED;
}

RequestResult buffer_handle_clear(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  BufferState *buf = get_or_init(caller, BUFFER_DEFAULT_CAPACITY);
  if (!buf)
    return REQ_OOM;

  buf->size = 0;
  return REQ_ACCEPTED;
}

RequestResult buffer_handle_size(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  BufferState *buf = get_or_init(caller, BUFFER_DEFAULT_CAPACITY);
  if (!buf)
    return REQ_OOM;

  const RequestParam *result_p = payload_get(payload, "result");
  if (result_p)
  {
    ((RequestParam *)result_p)->int_value = buf->size;
  }

  return REQ_ACCEPTED;
}

RequestResult buffer_handle_last(Device *dev, const RequestPayload *payload, CallerContext *caller)
{
  BufferState *buf = get_or_init(caller, BUFFER_DEFAULT_CAPACITY);
  if (!buf)
    return REQ_OOM;
  if (buf->size == 0)
    return REQ_IGNORED;

  const RequestParam *result_p = payload_get(payload, "result");
  if (result_p)
  {
    ((RequestParam *)result_p)->int_value = buf->values[buf->size - 1];
  }

  return REQ_ACCEPTED;
}