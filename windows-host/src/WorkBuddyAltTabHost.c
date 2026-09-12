#include <windows.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

enum {
    HOTKEY_ALT_F12 = 0x5742,
};

static wchar_t log_path[MAX_PATH];

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

    if (probe_only) {
        write_log(L"PROBE_OK hotkey=ALT+F12");
        UnregisterHotKey(NULL, HOTKEY_ALT_F12);
        CloseHandle(mutex);
        return 0;
    }

    write_log(L"HOST_READY hotkey=ALT+F12 action=ALT+TAB");
    MSG message;
    int result;
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        if (message.message == WM_HOTKEY && message.wParam == HOTKEY_ALT_F12) {
            bool sent = send_tab();
            write_log(sent ? L"HOTKEY_TRIGGERED send=TAB_OK" : L"HOTKEY_TRIGGERED send=TAB_FAILED");
        }
    }

    UnregisterHotKey(NULL, HOTKEY_ALT_F12);
    CloseHandle(mutex);
    write_log(L"HOST_STOPPED result=%d", result);
    return result < 0 ? 3 : 0;
}
