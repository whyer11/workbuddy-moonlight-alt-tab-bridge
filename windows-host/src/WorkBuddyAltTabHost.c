#include "GeneratedBridgeSchema.h"
#include "WorkBuddyStatusProtocol.h"
#include "WorkBuddyStatusServer.h"
#include "KeyboardBridgeCore.h"
#include "HostApplication.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

static wchar_t log_path[MAX_PATH];
enum { STATUS_RETRY_TIMER = 0x5743 };

static void initialize_log_path(void) {
    wchar_t local_app_data[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) { wcscpy(log_path, L"WorkBuddyAltTabHost.log"); return; }
    wchar_t directory[MAX_PATH];
    _snwprintf(directory, MAX_PATH, L"%ls\\WorkBuddyAltTab", local_app_data);
    directory[MAX_PATH - 1] = L'\0'; CreateDirectoryW(directory, NULL);
    _snwprintf(log_path, MAX_PATH, L"%ls\\host.log", directory);
    log_path[MAX_PATH - 1] = L'\0';
}

static void write_log(const wchar_t *format, ...) {
    FILE *file = _wfopen(log_path, L"a, ccs=UTF-8");
    if (file == NULL) return;
    SYSTEMTIME time; GetLocalTime(&time);
    fwprintf(file, L"%04u-%02u-%02u %02u:%02u:%02u ", time.wYear, time.wMonth,
            time.wDay, time.wHour, time.wMinute, time.wSecond);
    va_list arguments; va_start(arguments, format); vfwprintf(file, format, arguments);
    va_end(arguments); fputws(L"\n", file); fclose(file);
}

static int self_test(void) {
    char response[WB_SCHEMA_MAX_DATAGRAM_BYTES];
    int length = wb_status_format_response(response, sizeof(response),
            "00112233445566778899aabbccddeeff");
    if (length <= 0 || strstr(response, "protocol=2 core=ready") == NULL
            || strcmp(WB_SCHEMA_WINDOWS_RULES, "alt_tab,windows_key") != 0) return 1;
    puts("SELF_TEST_OK version=" WB_SCHEMA_PRODUCT_VERSION " protocol=2");
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    PWSTR command_line, int show_command) {
    (void)previous_instance; (void)show_command;
    if (wcsstr(command_line, L"--self-test") != NULL) return self_test();
    initialize_log_path();
    bool probe_only = wcsstr(command_line, L"--probe") != NULL;
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\WorkBuddyAltTabHost");
    if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex != NULL) CloseHandle(mutex);
        return probe_only ? 4 : 0;
    }

    struct wb_host_application application;
    if (!wb_host_application_start(&application, instance, WB_STATUS_PORT)) {
        write_log(L"KEYBOARD_CORE_FAILED error=%lu", GetLastError());
        CloseHandle(mutex); return 2;
    }

    bool status_ready = application.status_ready;
    UINT_PTR status_retry_timer = 0;
    if (!status_ready) {
        write_log(L"STATUS_DEGRADED port=%u error=%lu", WB_STATUS_PORT, WSAGetLastError());
        status_retry_timer = SetTimer(NULL, STATUS_RETRY_TIMER, 5000, NULL);
    }

    if (probe_only) {
        write_log(L"PROBE_OK core=ready status=%ls", status_ready ? L"ready" : L"degraded");
        wb_host_application_stop(&application); CloseHandle(mutex); return 0;
    }

    write_log(L"HOST_READY version=%hs protocol=%d core=ready status=%ls",
            WB_SCHEMA_PRODUCT_VERSION, WB_SCHEMA_PROTOCOL_VERSION,
            status_ready ? L"ready" : L"degraded");
    MSG message; int result;
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        if (message.message == WM_TIMER && status_retry_timer != 0
                && message.wParam == status_retry_timer
                && wb_host_application_retry_status(&application)) {
            KillTimer(NULL, status_retry_timer);
            status_retry_timer = 0;
            write_log(L"STATUS_RECOVERED port=%u", WB_STATUS_PORT);
            continue;
        }
        if (wb_host_application_handle_message(&application, &message))
            write_log(L"HOTKEY_TRIGGERED send=TAB_OK");
    }
    if (status_retry_timer != 0) KillTimer(NULL, status_retry_timer);
    wb_host_application_stop(&application); CloseHandle(mutex);
    write_log(L"HOST_STOPPED result=%d", result);
    return result < 0 ? 3 : 0;
}

int main(void) {
    return self_test();
}
