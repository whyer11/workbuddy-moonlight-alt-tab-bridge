#ifndef WORKBUDDY_HOST_TRAY_ICON_H
#define WORKBUDDY_HOST_TRAY_ICON_H

#include <windows.h>
#include <stdbool.h>

#ifndef IDI_HOST
#define IDI_HOST 101
#endif

enum wb_tray_command {
    WB_TRAY_CMD_NONE = 0,
    WB_TRAY_CMD_TOGGLE_PAUSE = 1001,
    WB_TRAY_CMD_OPEN_LOG_DIR = 1002,
    WB_TRAY_CMD_EXIT = 1003
};

struct wb_host_tray {
    HWND window;
    HICON icon;
    UINT taskbar_created_message;
    bool icon_added;
    bool bridge_paused;
};

bool wb_host_tray_start(struct wb_host_tray *tray, HINSTANCE instance,
                        const wchar_t *tooltip);
HWND wb_host_tray_window(const struct wb_host_tray *tray);
void wb_host_tray_set_paused(struct wb_host_tray *tray, bool paused);
bool wb_host_tray_handle_message(struct wb_host_tray *tray, const MSG *message,
                                 enum wb_tray_command *out_command);
void wb_host_tray_stop(struct wb_host_tray *tray);
void wb_host_tray_open_log_directory(void);

#endif
