#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define _WIN32_IE 0x0600

#include "HostTrayIcon.h"

#include <shellapi.h>
#include <string.h>
#include <wchar.h>

enum {
    WB_TRAY_ICON_ID = 1,
    WB_TRAY_CALLBACK = WM_APP + 1,
    WB_TRAY_TOOLTIP_CCH = 128
};

static const wchar_t WB_TRAY_WINDOW_CLASS[] = L"WorkBuddyAltTabHostTray";

static void update_tooltip(struct wb_host_tray *tray, const wchar_t *base) {
    if (tray == NULL || !tray->icon_added || base == NULL) {
        return;
    }
    NOTIFYICONDATAW data;
    ZeroMemory(&data, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = tray->window;
    data.uID = WB_TRAY_ICON_ID;
    data.uFlags = NIF_TIP;
    _snwprintf(data.szTip, WB_TRAY_TOOLTIP_CCH, L"%ls - %ls", base,
            tray->bridge_paused ? L"已暂停" : L"运行中");
    data.szTip[WB_TRAY_TOOLTIP_CCH - 1] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

static bool add_icon(struct wb_host_tray *tray, const wchar_t *tooltip) {
    NOTIFYICONDATAW data;
    ZeroMemory(&data, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = tray->window;
    data.uID = WB_TRAY_ICON_ID;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = WB_TRAY_CALLBACK;
    data.hIcon = tray->icon;
    if (tooltip != NULL) {
        _snwprintf(data.szTip, WB_TRAY_TOOLTIP_CCH, L"%ls - %ls", tooltip,
                tray->bridge_paused ? L"已暂停" : L"运行中");
        data.szTip[WB_TRAY_TOOLTIP_CCH - 1] = L'\0';
    } else {
        wcscpy(data.szTip, L"WorkBuddy Alt+Tab 桥接");
    }
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        return false;
    }
    tray->icon_added = true;
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

static void remove_icon(struct wb_host_tray *tray) {
    if (!tray->icon_added) {
        return;
    }
    NOTIFYICONDATAW data;
    ZeroMemory(&data, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = tray->window;
    data.uID = WB_TRAY_ICON_ID;
    Shell_NotifyIconW(NIM_DELETE, &data);
    tray->icon_added = false;
}

static void show_menu(struct wb_host_tray *tray) {
    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }

    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0,
            tray->bridge_paused ? L"桥接已暂停" : L"桥接运行中");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, WB_TRAY_CMD_TOGGLE_PAUSE,
            tray->bridge_paused ? L"恢复桥接" : L"暂停桥接");
    AppendMenuW(menu, MF_STRING, WB_TRAY_CMD_OPEN_LOG_DIR, L"打开日志目录");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, WB_TRAY_CMD_EXIT, L"退出");

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(tray->window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, cursor.x, cursor.y,
            0, tray->window, NULL);
    PostMessageW(tray->window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static LRESULT CALLBACK tray_window_proc(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam) {
    struct wb_host_tray *tray =
            (struct wb_host_tray *)GetWindowLongPtrW(window, GWLP_USERDATA);

    if (message == WM_NCCREATE) {
        const CREATESTRUCTW *create = (const CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return DefWindowProcW(window, message, wparam, lparam);
    }

    if (tray != NULL && tray->taskbar_created_message != 0
            && message == tray->taskbar_created_message) {
        tray->icon_added = false;
        add_icon(tray, L"WorkBuddy Alt+Tab 桥接");
        return 0;
    }

    if (message == WB_TRAY_CALLBACK) {
        UINT event = LOWORD(lparam);
        if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP
                || event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK
                || event == NIN_SELECT || event == NIN_KEYSELECT) {
            show_menu(tray);
        }
        return 0;
    }

    if (message == WM_COMMAND) {
        return DefWindowProcW(window, message, wparam, lparam);
    }

    if (message == WM_DESTROY) {
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

bool wb_host_tray_start(struct wb_host_tray *tray, HINSTANCE instance,
                        const wchar_t *tooltip) {
    if (tray == NULL || instance == NULL) {
        return false;
    }
    memset(tray, 0, sizeof(*tray));

    WNDCLASSEXW window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = tray_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = WB_TRAY_WINDOW_CLASS;
    if (RegisterClassExW(&window_class) == 0
            && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    tray->taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    tray->window = CreateWindowExW(0, WB_TRAY_WINDOW_CLASS,
            L"WorkBuddyAltTabHostTray", 0, 0, 0, 0, 0,
            HWND_MESSAGE, NULL, instance, tray);
    if (tray->window == NULL) {
        return false;
    }

    tray->icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_HOST),
            IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (tray->icon == NULL) {
        UINT dpi = 96;
        HWND desktop = GetDesktopWindow();
        if (desktop != NULL) {
            dpi = GetDpiForWindow(desktop);
            if (dpi == 0) dpi = 96;
        }
        int sm_cx = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
        if (sm_cx <= 0) sm_cx = GetSystemMetrics(SM_CXSMICON);
        tray->icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_HOST),
                IMAGE_ICON, sm_cx, sm_cx, LR_SHARED);
    }
    if (tray->icon == NULL) {
        DestroyWindow(tray->window);
        tray->window = NULL;
        return false;
    }

    if (!add_icon(tray, tooltip)) {
        DestroyWindow(tray->window);
        tray->window = NULL;
        return false;
    }
    return true;
}

HWND wb_host_tray_window(const struct wb_host_tray *tray) {
    return tray == NULL ? NULL : tray->window;
}

void wb_host_tray_set_paused(struct wb_host_tray *tray, bool paused) {
    if (tray == NULL) {
        return;
    }
    tray->bridge_paused = paused;
    update_tooltip(tray, L"WorkBuddy Alt+Tab 桥接");
}

bool wb_host_tray_handle_message(struct wb_host_tray *tray, const MSG *message,
                                 enum wb_tray_command *out_command) {
    if (out_command != NULL) {
        *out_command = WB_TRAY_CMD_NONE;
    }
    if (tray == NULL || message == NULL || message->hwnd != tray->window) {
        return false;
    }
    if (message->message != WM_COMMAND) {
        return false;
    }
    int command = LOWORD(message->wParam);
    if (command == WB_TRAY_CMD_TOGGLE_PAUSE
            || command == WB_TRAY_CMD_OPEN_LOG_DIR
            || command == WB_TRAY_CMD_EXIT) {
        if (out_command != NULL) {
            *out_command = (enum wb_tray_command)command;
        }
        return true;
    }
    return false;
}

void wb_host_tray_stop(struct wb_host_tray *tray) {
    if (tray == NULL) {
        return;
    }
    remove_icon(tray);
    if (tray->window != NULL) {
        DestroyWindow(tray->window);
        tray->window = NULL;
    }
    tray->icon = NULL;
}

void wb_host_tray_open_log_directory(void) {
    wchar_t local_app_data[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return;
    }
    wchar_t directory[MAX_PATH];
    _snwprintf(directory, MAX_PATH, L"%ls\\WorkBuddyAltTab", local_app_data);
    directory[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(directory, NULL);
    ShellExecuteW(NULL, L"open", directory, NULL, NULL, SW_SHOWNORMAL);
}
