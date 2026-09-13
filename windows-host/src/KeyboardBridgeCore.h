#ifndef WORKBUDDY_KEYBOARD_BRIDGE_CORE_H
#define WORKBUDDY_KEYBOARD_BRIDGE_CORE_H

#include <windows.h>
#include <stdbool.h>

struct wb_keyboard_core {
    HHOOK keyboard_hook;
    HINSTANCE instance;
    bool hotkey_registered;
    bool win_key_down;
};

bool wb_keyboard_core_start(struct wb_keyboard_core *core, HINSTANCE instance);
bool wb_keyboard_core_handle_message(struct wb_keyboard_core *core, const MSG *message);
void wb_keyboard_core_stop(struct wb_keyboard_core *core);

#endif
