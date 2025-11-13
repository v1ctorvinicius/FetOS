#include "fetos.h"
#include "hal.h"
#include "eventbus.h"
#include "iom.h"
#include "scheduler.h"
#include "fetvm.h"
#include "fetlink.h"

void fetos_init(void) {
  hal_init();
  eventbus_init();
  iom_init();
  scheduler_init();
  fetvm_init();
  fetlink_tick();
}
