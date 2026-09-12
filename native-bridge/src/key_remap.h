#ifndef WORKBUDDY_KEY_REMAP_H
#define WORKBUDDY_KEY_REMAP_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WB_KEY_TAB = 15,
    WB_KEY_LEFTALT = 56,
    WB_KEY_SCROLLLOCK = 70,
    WB_KEY_RIGHTALT = 100,
    WB_KEY_LEFTMETA = 125,
    WB_KEY_F12 = 88,
};

struct key_remap_state {
    bool left_alt_down;
    bool right_alt_down;
    bool tab_is_remapped;
};

uint16_t key_remap_apply(struct key_remap_state *state, uint16_t code, int32_t value);

#endif
