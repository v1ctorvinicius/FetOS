#pragma once
#include "device.h"

#define MAX_CAPABILITIES 64

typedef struct
{
  const char *key;
  const char *str_value;
  int32_t int_value;
} RequestParam;

typedef struct
{
  RequestParam *params;
  uint8_t count;
} RequestPayload;

const RequestParam *payload_get(const RequestPayload *payload, const char *key);

typedef enum
{
  REQ_ACCEPTED,
  REQ_IGNORED,
  REQ_NO_DEVICE,
  REQ_OOM
} RequestResult;

typedef enum
{
  CALLER_NATIVE,
  CALLER_FVM
} CallerType;

typedef struct
{
  CallerType type;
  void **state;
  void *fvm_proc;
} CallerContext;

typedef RequestResult (*CapabilityHandler)(
    Device *dev,
    const RequestPayload *payload,
    CallerContext *caller);

typedef struct
{
  const char *capability;
  CapabilityHandler handler;
  Device *device;
  bool active;
} CapabilityEntry;

void capability_init();
bool capability_register(const char *capability, CapabilityHandler handler, Device *dev);
RequestResult capability_request(const char *capability, const RequestPayload *payload, CallerContext *caller);

RequestResult capability_request_native(const char *capability, const RequestPayload *payload);