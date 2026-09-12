#include "key_remap.h"

uint16_t key_remap_apply(struct key_remap_state *state, uint16_t code, int32_t value) {
    if (code == WB_KEY_LEFTALT) {
        state->left_alt_down = value != 0;
        return code;
    }
    if (code == WB_KEY_RIGHTALT) {
        state->right_alt_down = value != 0;
        return code;
    }
    if (code != WB_KEY_TAB) {
        return code;
    }

    if (value == 1 && (state->left_alt_down || state->right_alt_down)) {
        state->tab_is_remapped = true;
    }

    if (state->tab_is_remapped) {
        if (value == 0) {
            state->tab_is_remapped = false;
        }
        return WB_KEY_F12;
    }

    return WB_KEY_TAB;
}
