
#include "fvm_app.h"
#include "system.h"
#include <stdlib.h>
#include <string.h>
#include <LittleFS.h>
#include "oled.h"

static void fvm_on_enter_stub()
{
}

static void fvm_on_exit_stub()
{
}

static void fvm_on_event_stub(Event *e)
{
  if (!e || !e->has_payload)
    return;

  if (e->payload.value == GESTURE_DOUBLE_TAP)
  {
    if (current_app && current_app->update_ctx)
    {
      FvmAppContext *fctx = (FvmAppContext *)current_app->update_ctx;
      if (fctx->proc)
        fctx->proc->halted = true;
    }
    system_set_app(&launcher_app);
  }
  else if (e->payload.value == GESTURE_TAP)
  {
    if (current_app && current_app->update_ctx)
    {
      FvmAppContext *fctx = (FvmAppContext *)current_app->update_ctx;
      if (fctx->proc && (fctx->proc->halted || fctx->proc->error))
      {

        fvm_process_reset(fctx->proc);
        fctx->proc->error = false;
        fctx->proc->halted = false;
        fctx->proc->error_code = 0;

        Serial.println("[FVM] Ressureição Completa!");
      }
    }
  }
}

static void fvm_render_stub()
{
  Device *oled_dev = system_get_device_by_id(2);
  if (!oled_dev || !oled_dev->state)
    return;
  OledState *oled = (OledState *)oled_dev->state;

  if (!current_app || !current_app->update_ctx)
    return;

  FvmAppContext *fctx = (FvmAppContext *)current_app->update_ctx;
  FvmProcess *proc = fctx->proc;
  if (!proc)
    return;

  if (proc->error || proc->halted)
  {
    hal_clear(DISPLAY_DEFAULT_ID);
    hal_rect(DISPLAY_DEFAULT_ID, 0, 0, 128, 64, false);

    ui_center_text(oled, 10, proc->name, 1);
    if (proc->error)
    {
      ui_center_text(oled, 25, "!! APP CRASHED !!", 1);
      char err_msg[20];
      snprintf(err_msg, sizeof(err_msg), "Erro Code: %d", proc->error_code);
      ui_center_text(oled, 40, err_msg, 1);
    }
    else
    {
      ui_center_text(oled, 30, "PROCESSO FINALIZADO", 1);
    }
    ui_center_text(oled, 52, "1x: Reset | 2x: Sair", 1);
  }
}

static void fvm_update(void *ctx)
{
  FvmAppContext *fctx = (FvmAppContext *)ctx;
  if (!fctx || !fctx->proc)
    return;

  fvm_run(fctx->proc);

  if (fctx->proc->error)
  {

    Serial.print("[FVM] error in '");
    Serial.print(fctx->proc->name);
    Serial.print("': code=");
    Serial.println(fctx->proc->error_code);
  }
  else if (fctx->proc->halted)
  {

    App *app_ptr = &fctx->app;

    fvm_process_reset(fctx->proc);

    system_kill_app(app_ptr);
  }
}

App *fvm_app_setup_wrapper(FvmProcess *proc, uint8_t *fs_buffer)
{
  if (!proc)
    return nullptr;

  FvmAppContext *fctx = (FvmAppContext *)malloc(sizeof(FvmAppContext));
  if (!fctx)
    return nullptr;

  fctx->proc = proc;
  fctx->fs_buffer = fs_buffer;

  App *app = &fctx->app;
  memset(app, 0, sizeof(App));

  app->name = proc->name;
  app->on_enter = fvm_on_enter_stub;
  app->on_exit = fvm_on_exit_stub;
  app->on_event = fvm_on_event_stub;
  app->render = fvm_render_stub;
  app->update = fvm_update;
  app->update_interval_ms = 10;
  app->update_ctx = fctx;
  app->_task_id = -1;

  return app;
}

App *fvm_app_load_fs(const char *filename)
{
  File file = LittleFS.open(filename, "r");
  if (!file)
  {
    Serial.printf("[FVM] Erro: Arquivo %s nao abriu\n", filename);
    return nullptr;
  }

  size_t size = file.size();

  size_t free_ram = ESP.getFreeHeap();
  size_t max_block = ESP.getMaxAllocHeap();
  Serial.printf("[FVM] Tentando carregar %s (%d bytes). RAM Livre: %d | Maior Bloco: %d\n",
                filename, size, free_ram, max_block);

  if (size > max_block)
  {
    Serial.println("[FVM] PITOMBA! Nao ha bloco de RAM contiguo para este app.");
    file.close();
    return nullptr;
  }

  uint8_t *buffer = (uint8_t *)malloc(size);
  if (!buffer)
  {
    Serial.println("[FVM] Erro crítico: malloc falhou mesmo com espaço aparente.");
    file.close();
    return nullptr;
  }

  file.read(buffer, size);
  file.close();

  FvmProcess *proc = fvm_load_from_memory(filename, buffer, size);
  if (!proc)
  {
    Serial.println("[FVM] Erro: Falha ao parsear bytecode na memoria.");
    free(buffer);
    return nullptr;
  }

  return fvm_app_setup_wrapper(proc, buffer);
}

App *fvm_app_create(const char *name, const uint8_t *bytecode, uint16_t len)
{
  FvmProcess *proc = fvm_process_create(name, bytecode, len);
  if (!proc)
    return nullptr;

  return fvm_app_setup_wrapper(proc, nullptr);
}

void fvm_app_destroy(App *app)
{
  if (!app)
    return;

  FvmAppContext *fctx = (FvmAppContext *)app->update_ctx;
  if (!fctx)
    return;

  if (fctx->proc)
  {
    fvm_process_destroy(fctx->proc);
    fctx->proc = nullptr;
  }

  if (fctx->fs_buffer)
  {
    free(fctx->fs_buffer);
  }

  free(fctx);
}

FvmProcess *fvm_app_get_process(App *app)
{
  if (!app)
    return nullptr;
  FvmAppContext *fctx = (FvmAppContext *)app->update_ctx;
  if (!fctx)
    return nullptr;
  return fctx->proc;
}