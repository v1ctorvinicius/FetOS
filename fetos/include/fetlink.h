#ifndef FETLINK_H
#define FETLINK_H

#include <stdint.h>
#include <stdbool.h>

// Tipos de pacotes
#define FETLINK_TYPE_EVT          0x01
#define FETLINK_TYPE_APP_CALL     0x02
#define FETLINK_TYPE_APP_RET      0x03
#define FETLINK_TYPE_FS_READ      0x04
#define FETLINK_TYPE_FS_WRITE     0x05
#define FETLINK_TYPE_SYNC         0x06
#define FETLINK_TYPE_PING         0x07
#define FETLINK_TYPE_CAP_DISCOVER 0x08
#define FETLINK_TYPE_APP_DEPLOY   0x09
#define FETLINK_TYPE_UI_DRAW      0x10

// Estrutura principal do pacote
typedef struct {
  uint8_t version;
  uint8_t type;
  uint8_t ttl;
  uint8_t seq;
  char src_realm[4];
  char src_hash[4];
  char dst_realm[8];
  char dst_hash[16];
  uint8_t flags;
  uint8_t len;
  char payload[64];
  uint16_t crc;
} FetPacket;

void fetlink_init(void);
bool fetlink_send(FetPacket* pkt);
void fetlink_tick(void);

#endif
