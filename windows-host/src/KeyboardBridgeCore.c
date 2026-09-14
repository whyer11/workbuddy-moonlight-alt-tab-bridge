#include "KeyboardBridgeCore.h"

#include <string.h>

enum { HOTKEY_ALT_F12 = 0x5742 };
#define WORKBUDDY_SYNTH_TAG ((ULONG_PTR)0x5742574BU)

static struct wb_keyboard_core *active_core;

static bool send_tab(void) {
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_TAB;
    inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = VK_TAB;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

static bool send_win_key(bool down) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD; input.ki.wVk = VK_LWIN;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = WORKBUDDY_SYNTH_TAG;
    return SendInput(1, &input, sizeof(input)) == 1;
}

static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM message, LPARAM data) {
    struct wb_keyboard_core *core = active_core;
    if (core != NULL && code == HC_ACTION && !core->paused) {
        const KBDLLHOOKSTRUCT *event = (const KBDLLHOOKSTRUCT *)data;
        if (event->vkCode == VK_SCROLL && event->dwExtraInfo != WORKBUDDY_SYNTH_TAG) {
            bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
            if (down) {
                if (!core->win_key_down && send_win_key(true)) core->win_key_down = true;
                return 1;
            }
            if (up) {
                if (core->win_key_down && send_win_key(false)) core->win_key_down = false;
                return 1;
            }
        }
    }
    return CallNextHookEx(core == NULL ? NULL : core->keyboard_hook, code, message, data);
}

bool wb_keyboard_core_start(struct wb_keyboard_core *core, HINSTANCE instance,
                            HWND message_window) {
    if (core == NULL || active_core != NULL || message_window == NULL) return false;
    ZeroMemory(core, sizeof(*core));
    core->instance = instance;
    core->message_window = message_window;
    if (!RegisterHotKey(message_window, HOTKEY_ALT_F12, MOD_ALT | MOD_NOREPEAT, VK_F12)) {
        return false;
    }
    core->hotkey_registered = true;
    active_core = core;
    core->keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook_proc, instance, 0);
    if (core->keyboard_hook == NULL) {
        active_core = NULL;
        UnregisterHotKey(message_window, HOTKEY_ALT_F12);
        core->hotkey_registered = false;
        core->message_window = NULL;
        return false;
    }
    return true;
}

bool wb_keyboard_core_set_paused(struct wb_keyboard_core *core, bool paused) {
    if (core == NULL || !core->hotkey_registered) return false;
    if (core->paused == paused) return true;
    core->paused = paused;
    if (paused && core->win_key_down) {
        send_win_key(false);
        core->win_key_down = false;
    }
    return true;
}

bool wb_keyboard_core_is_paused(const struct wb_keyboard_core *core) {
    return core != NULL && core->paused;
}

bool wb_keyboard_core_handle_message(struct wb_keyboard_core *core, const MSG *message) {
    if (core == NULL || message == NULL || core->paused) return false;
    if (message->message == WM_HOTKEY && message->wParam == HOTKEY_ALT_F12) return send_tab();
    return false;
}

void wb_keyboard_core_stop(struct wb_keyboard_core *core) {
    if (core == NULL) return;
    if (core->win_key_down) { send_win_key(false); core->win_key_down = false; }
    if (core->keyboard_hook != NULL) UnhookWindowsHookEx(core->keyboard_hook);
    if (core->hotkey_registered && core->message_window != NULL) {
        UnregisterHotKey(core->message_window, HOTKEY_ALT_F12);
    }
    if (active_core == core) active_core = NULL;
    ZeroMemory(core, sizeof(*core));
}
