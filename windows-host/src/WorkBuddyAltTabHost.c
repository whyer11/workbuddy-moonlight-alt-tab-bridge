#include "WorkBuddyStatusProtocol.h"
#include "WorkBuddyStatusServer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

enum {
    HOTKEY_ALT_F12 = 0x5742,
};

static wchar_t log_path[MAX_PATH];
static HHOOK keyboard_hook;
static bool win_key_down;

#define WORKBUDDY_SYNTH_TAG ((ULONG_PTR)0x5742574BU)

static void initialize_log_path(void) {
    wchar_t local_app_data[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        wcscpy(log_path, L"WorkBuddyAltTabHost.log");
        return;
    }
    wchar_t directory[MAX_PATH];
    _snwprintf(directory, MAX_PATH, L"%ls\\WorkBuddyAltTab", local_app_data);
    directory[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(directory, NULL);
    _snwprintf(log_path, MAX_PATH, L"%ls\\host.log", directory);
    log_path[MAX_PATH - 1] = L'\0';
}

static void write_log(const wchar_t *format, ...) {
    FILE *file = _wfopen(log_path, L"a, ccs=UTF-8");
    if (file == NULL) {
        return;
    }
    SYSTEMTIME time;
    GetLocalTime(&time);
    fwprintf(file, L"%04u-%02u-%02u %02u:%02u:%02u ",
            time.wYear, time.wMonth, time.wDay,
            time.wHour, time.wMinute, time.wSecond);
    va_list arguments;
    va_start(arguments, format);
    vfwprintf(file, format, arguments);
    va_end(arguments);
    fputws(L"\n", file);
    fclose(file);
}

static bool send_tab(void) {
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_TAB;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_TAB;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

static bool send_win_key(bool down) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_LWIN;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = WORKBUDDY_SYNTH_TAG;
    return SendInput(1, &input, sizeof(input)) == 1;
}

static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *event = (const KBDLLHOOKSTRUCT *)data;
        if (event->vkCode == VK_SCROLL && event->dwExtraInfo != WORKBUDDY_SYNTH_TAG) {
            bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            bool up = message == WM_KEYUP || message == WM_SYSKEYUP;

            if (down) {
                if (!win_key_down && send_win_key(true)) {
                    win_key_down = true;
                }
                return 1;
            }
            if (up) {
                if (win_key_down && send_win_key(false)) {
                    win_key_down = false;
                }
                return 1;
            }
        }
    }
    return CallNextHookEx(keyboard_hook, code, message, data);
}

static void release_win_key(void) {
    if (win_key_down && send_win_key(false)) {
        win_key_down = false;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    PWSTR command_line, int show_command) {
    (void)instance;
    (void)previous_instance;
    (void)show_command;
    initialize_log_path();

    bool probe_only = wcsstr(command_line, L"--probe") != NULL;
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\WorkBuddyAltTabHost");
    if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return probe_only ? 4 : 0;
    }

    if (!RegisterHotKey(NULL, HOTKEY_ALT_F12, MOD_ALT | MOD_NOREPEAT, VK_F12)) {
        DWORD error = GetLastError();
        write_log(L"REGISTER_FAILED error=%lu", error);
        CloseHandle(mutex);
        return 2;
    }

    keyboard_hook = SetWindowsHookExW(
            WH_KEYBOARD_LL, keyboard_hook_proc, instance, 0);
    if (keyboard_hook == NULL) {
        DWORD error = GetLastError();
        write_log(L"HOOK_FAILED error=%lu", error);
        UnregisterHotKey(NULL, HOTKEY_ALT_F12);
        CloseHandle(mutex);
        return 5;
    }

    struct wb_status_server status_server;
    if (!wb_status_server_start(&status_server, WB_STATUS_PORT)) {
        DWORD error = WSAGetLastError();
        write_log(L"STATUS_SERVER_FAILED port=%u error=%lu", WB_STATUS_PORT, error);
        UnhookWindowsHookEx(keyboard_hook);
        UnregisterHotKey(NULL, HOTKEY_ALT_F12);
        CloseHandle(mutex);
        return 6;
    }

    if (probe_only) {
        write_log(L"PROBE_OK hotkey=ALT+F12 sentinel=SCROLLLOCK action=LWIN status=UDP:%u",
                WB_STATUS_PORT);
        wb_status_server_stop(&status_server);
        UnhookWindowsHookEx(keyboard_hook);
        UnregisterHotKey(NULL, HOTKEY_ALT_F12);
        CloseHandle(mutex);
        return 0;
    }

    write_log(L"HOST_READY hotkey=ALT+F12 action=ALT+TAB sentinel=SCROLLLOCK action=LWIN status=UDP:%u",
            WB_STATUS_PORT);
    MSG message;
    int result;
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        if (message.message == WM_HOTKEY && message.wParam == HOTKEY_ALT_F12) {
            bool sent = send_tab();
            write_log(sent ? L"HOTKEY_TRIGGERED send=TAB_OK" : L"HOTKEY_TRIGGERED send=TAB_FAILED");
        }
    }

    wb_status_server_stop(&status_server);
    release_win_key();
    UnhookWindowsHookEx(keyboard_hook);
    UnregisterHotKey(NULL, HOTKEY_ALT_F12);
    CloseHandle(mutex);
    write_log(L"HOST_STOPPED result=%d", result);
    return result < 0 ? 3 : 0;
}
