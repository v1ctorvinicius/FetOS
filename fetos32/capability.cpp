#include "capability.h"
#include <string.h>

static CapabilityEntry registry[MAX_CAPABILITIES];
static int registry_count = 0;

void capability_init()
{
  for (int i = 0; i < MAX_CAPABILITIES; i++)
  {
    registry[i].active = false;
  }
  registry_count = 0;
}

bool capability_register(const char *capability, CapabilityHandler handler, Device *dev)
{
  if (registry_count >= MAX_CAPABILITIES)
    return false;

  registry[registry_count].capability = capability;
  registry[registry_count].handler = handler;
  registry[registry_count].device = dev;
  registry[registry_count].active = true;
  registry_count++;

  return true;
}

static bool capability_matches(const char *registered, const char *requested)
{

  if (strcmp(registered, requested) == 0)
    return true;

  const char *star = strstr(registered, ".*:");
  if (!star)
    return false;

  size_t prefix_len = star - registered;
  if (strncmp(registered, requested, prefix_len) != 0)
    return false;

  const char *reg_suffix = star + 2; // pula o '*'
  const char *req_dot = requested + prefix_len;
  const char *req_colon = strchr(req_dot, ':');
  if (!req_colon)
    return false;

  return strcmp(reg_suffix, req_colon) == 0;
}

RequestResult capability_request(const char *capability, const RequestPayload *payload, CallerContext *caller)
{
  for (int i = 0; i < registry_count; i++)
  {
    if (!registry[i].active)
      continue;
    if (!capability_matches(registry[i].capability, capability))
      continue;

    return registry[i].handler(registry[i].device, payload, caller);
  }

  return REQ_NO_DEVICE;
}

RequestResult capability_request_native(const char *capability, const RequestPayload *payload)
{
  static CallerContext native_caller = {CALLER_NATIVE, nullptr};
  return capability_request(capability, payload, &native_caller);
}

const RequestParam *payload_get(const RequestPayload *payload, const char *key)
{
  if (!payload || !payload->params)
    return nullptr;

  for (int i = 0; i < payload->count; i++)
  {
    if (strcmp(payload->params[i].key, key) == 0)
    {
      return &payload->params[i];
    }
  }

  return nullptr;
}