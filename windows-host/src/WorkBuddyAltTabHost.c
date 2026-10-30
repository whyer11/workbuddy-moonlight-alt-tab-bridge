#include "GeneratedBridgeSchema.h"
#include "WorkBuddyStatusProtocol.h"
#include "WorkBuddyStatusServer.h"
#include "KeyboardBridgeCore.h"
#include "HostApplication.h"
#include "HostTrayIcon.h"
#include "HostAutostart.h"

#include <shellapi.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

static wchar_t log_path[MAX_PATH];
enum { STATUS_RETRY_TIMER = 0x5743 };
enum { AUTOSTART_DISABLED_EXIT = 10, AUTOSTART_ERROR_EXIT = 11 };

static bool initialize_log_path(void) {
    DWORD length = GetModuleFileNameW(NULL, log_path, ARRAYSIZE(log_path));
    if (length == 0 || length >= ARRAYSIZE(log_path)) {
        if (length >= ARRAYSIZE(log_path)) SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return false;
    }
    wchar_t *separator = wcsrchr(log_path, L'\\');
    if (separator == NULL) {
        SetLastError(ERROR_INVALID_NAME);
        return false;
    }
    size_t directory_length = (size_t)(separator + 1 - log_path);
    static const wchar_t filename[] = L"host.log";
    if (directory_length + ARRAYSIZE(filename) > ARRAYSIZE(log_path)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return false;
    }
    wcscpy(separator + 1, filename);
    return true;
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

static bool has_argument(const wchar_t *argument) {
    int count = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == NULL) return false;
    bool found = false;
    for (int index = 1; index < count; ++index) {
        if (wcscmp(arguments[index], argument) == 0) {
            found = true;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

static HRESULT executable_path(wchar_t *path, DWORD capacity) {
    DWORD length = GetModuleFileNameW(NULL, path, capacity);
    if (length == 0) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= capacity) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    return S_OK;
}

static HRESULT query_autostart(const wchar_t *path, bool *enabled) {
    HRESULT result = wb_host_autostart_query(WB_AUTOSTART_TASK_NAME, path, enabled);
    if (FAILED(result)) {
        write_log(L"AUTOSTART_QUERY_FAILED hresult=0x%08lx", (unsigned long)result);
    }
    return result;
}

static void refresh_autostart(struct wb_host_tray *tray, const wchar_t *path) {
    bool enabled = false;
    HRESULT result = query_autostart(path, &enabled);
    wb_host_tray_set_autostart(tray, SUCCEEDED(result) && enabled, SUCCEEDED(result));
}

static void toggle_autostart(struct wb_host_tray *tray, const wchar_t *path) {
    bool enabled = false;
    HRESULT result = query_autostart(path, &enabled);
    if (SUCCEEDED(result)) {
        result = wb_host_autostart_set(WB_AUTOSTART_TASK_NAME, path, !enabled);
        write_log(L"AUTOSTART_SET enabled=%d hresult=0x%08lx",
                !enabled, (unsigned long)result);
    }
    refresh_autostart(tray, path);
    if (SUCCEEDED(result) && tray->autostart_available
            && tray->autostart_enabled == enabled) {
        result = E_FAIL;
        write_log(L"AUTOSTART_VERIFY_FAILED expected=%d actual=%d hresult=0x%08lx",
                !enabled, tray->autostart_enabled, (unsigned long)result);
    }
    if (FAILED(result) || !tray->autostart_available
            || tray->autostart_enabled == enabled) {
        MessageBoxW(tray->window, L"无法修改开机自启动，请查看日志后重试。",
                L"WorkBuddy Alt+Tab 桥接", MB_OK | MB_ICONERROR);
    }
}

static int request_host_exit(void) {
    /* Search both the current top-level receiver and older message-only hosts. */
    HWND window = FindWindowExW(NULL, NULL,
            L"WorkBuddyAltTabHostTray", L"WorkBuddyAltTabHostTray");
    if (window == NULL) return 0;
    if (!PostMessageW(window, WM_COMMAND, WB_TRAY_CMD_EXIT, 0)) {
        write_log(L"HOST_QUIT_FAILED error=%lu", GetLastError());
        return 12;
    }
    write_log(L"HOST_QUIT_REQUESTED");
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    PWSTR command_line, int show_command) {
    (void)previous_instance; (void)show_command; (void)command_line;
    if (has_argument(L"--self-test")) return self_test();
    if (!initialize_log_path()) {
        OutputDebugStringW(L"WorkBuddy: 无法确定程序目录下的 host.log 路径。\n");
        return AUTOSTART_ERROR_EXIT;
    }
    if (has_argument(L"--quit")) return request_host_exit();
    wchar_t path[32768];
    HRESULT path_result = executable_path(path, ARRAYSIZE(path));
    if (FAILED(path_result)) {
        write_log(L"HOST_PATH_FAILED hresult=0x%08lx", (unsigned long)path_result);
        return AUTOSTART_ERROR_EXIT;
    }
    bool enable_autostart = has_argument(L"--enable-autostart");
    bool disable_autostart = has_argument(L"--disable-autostart");
    bool autostart_status = has_argument(L"--autostart-status");
    if (enable_autostart || disable_autostart || autostart_status) {
        if ((enable_autostart && disable_autostart)
                || (autostart_status && (enable_autostart || disable_autostart))) {
            write_log(L"AUTOSTART_ARGUMENT_CONFLICT");
            return AUTOSTART_ERROR_EXIT;
        }
        if (autostart_status) {
            bool enabled = false;
            HRESULT result = query_autostart(path, &enabled);
            if (FAILED(result)) return AUTOSTART_ERROR_EXIT;
            return enabled ? 0 : AUTOSTART_DISABLED_EXIT;
        }
        HRESULT result = wb_host_autostart_set(WB_AUTOSTART_TASK_NAME,
                path, enable_autostart);
        write_log(L"AUTOSTART_SET enabled=%d hresult=0x%08lx",
                enable_autostart, (unsigned long)result);
        return SUCCEEDED(result) ? 0 : AUTOSTART_ERROR_EXIT;
    }
    bool probe_only = has_argument(L"--probe");
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\WorkBuddyAltTabHost");
    if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex != NULL) CloseHandle(mutex);
        return probe_only ? 4 : 0;
    }

    struct wb_host_tray tray;
    if (!wb_host_tray_start(&tray, instance, L"WorkBuddy Alt+Tab 桥接")) {
        write_log(L"TRAY_START_FAILED error=%lu", GetLastError());
        CloseHandle(mutex);
        return 5;
    }

    struct wb_host_application application;
    if (!wb_host_application_start(&application, instance,
            wb_host_tray_window(&tray), WB_STATUS_PORT)) {
        write_log(L"KEYBOARD_CORE_FAILED error=%lu", GetLastError());
        wb_host_tray_stop(&tray);
        CloseHandle(mutex); return 2;
    }

    bool status_ready = application.status_ready;
    UINT_PTR status_retry_timer = 0;
    if (!status_ready) {
        write_log(L"STATUS_DEGRADED port=%u error=%lu", WB_STATUS_PORT, WSAGetLastError());
        status_retry_timer = SetTimer(wb_host_tray_window(&tray),
                STATUS_RETRY_TIMER, 5000, NULL);
    }

    if (probe_only) {
        write_log(L"PROBE_OK core=ready status=%ls", status_ready ? L"ready" : L"degraded");
        wb_host_application_stop(&application);
        wb_host_tray_stop(&tray);
        CloseHandle(mutex); return 0;
    }

    write_log(L"HOST_READY version=%hs protocol=%d core=ready status=%ls tray=ready",
            WB_SCHEMA_PRODUCT_VERSION, WB_SCHEMA_PROTOCOL_VERSION,
            status_ready ? L"ready" : L"degraded");

    MSG message; int result;
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        enum wb_tray_command tray_command = WB_TRAY_CMD_NONE;
        if (wb_host_tray_handle_message(&tray, &message, &tray_command)
                && tray_command != WB_TRAY_CMD_NONE) {
            if (tray_command == WB_TRAY_CMD_EXIT) {
                write_log(L"TRAY_EXIT");
                PostQuitMessage(0);
                continue;
            }
            if (tray_command == WB_TRAY_CMD_OPEN_LOG_DIR) {
                wb_host_tray_open_log_directory();
                write_log(L"TRAY_OPEN_LOG");
                continue;
            }
            if (tray_command == WB_TRAY_CMD_SHOW_MENU) {
                refresh_autostart(&tray, path);
                wb_host_tray_show_menu(&tray);
                continue;
            }
            if (tray_command == WB_TRAY_CMD_TOGGLE_AUTOSTART) {
                toggle_autostart(&tray, path);
                continue;
            }
            if (tray_command == WB_TRAY_CMD_TOGGLE_PAUSE) {
                bool paused = !wb_keyboard_core_is_paused(&application.keyboard);
                if (wb_host_application_set_paused(&application, paused)) {
                    wb_host_tray_set_paused(&tray, paused);
                    write_log(paused ? L"TRAY_PAUSED" : L"TRAY_RESUMED");
                }
                continue;
            }
        }

        if (message.message == WM_TIMER && status_retry_timer != 0
                && message.wParam == status_retry_timer
                && wb_host_application_retry_status(&application)) {
            KillTimer(wb_host_tray_window(&tray), status_retry_timer);
            status_retry_timer = 0;
            write_log(L"STATUS_RECOVERED port=%u", WB_STATUS_PORT);
            continue;
        }
        if (wb_host_application_handle_message(&application, &message)) {
            write_log(L"HOTKEY_TRIGGERED send=TAB_OK");
        } else {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (status_retry_timer != 0) KillTimer(wb_host_tray_window(&tray), status_retry_timer);
    wb_host_application_stop(&application);
    wb_host_tray_stop(&tray);
    CloseHandle(mutex);
    write_log(L"HOST_STOPPED result=%d", result);
    return result < 0 ? 3 : 0;
}

int main(void) {
    return self_test();
}
