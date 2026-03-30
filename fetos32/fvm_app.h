#pragma once
#include "app.h"
#include "fvm_process.h"

typedef struct
{
  FvmProcess *proc;
  App app;
  uint8_t *fs_buffer;
} FvmAppContext;

App *fvm_app_create(const char *name, const uint8_t *bytecode, uint16_t len);

void fvm_app_destroy(App *app);

FvmProcess *fvm_app_get_process(App *app);

App *fvm_app_load_fs(const char *filename);