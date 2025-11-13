#ifndef IOM_H
#define IOM_H
#include <stdbool.h>
#include <stdint.h>
#include "fetos.h"

typedef struct {
  uint8_t dev_id;
  FetApp* owner;
  bool shared;
  uint8_t usage_count;
} IOMEntry;

void iom_init(void);
bool iom_request(FetApp* app, uint8_t dev);
void iom_release(FetApp* app, uint8_t dev);
bool iom_owned(FetApp* app, uint8_t dev);

#endif
