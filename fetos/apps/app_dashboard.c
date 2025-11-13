#include "fetlink.h"
#include "eventbus.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static uint32_t counter = 0;

void dashboard_bg(void) { counter++; }

void dashboard_core(void) {
    if (counter % 50 == 0) {
        Event ev = { .type = 1, .src = 0, .dst = 255 };
        event_post(&ev);
    }
}

void dashboard_fg(void) {
    FetPacket pkt = {0};
    pkt.type = FETLINK_TYPE_UI_DRAW;
    strcpy(pkt.dst_realm, "HUB");
    strcpy(pkt.dst_hash, "OLED_SIM");
    snprintf(pkt.payload, sizeof(pkt.payload), "{\"cmd\":\"TEXT\",\"x\":0,\"y\":0,\"text\":\"Hello FetHub!\"}");
    fetlink_send(&pkt);
}
