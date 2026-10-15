#ifndef WORKBUDDY_HOST_AUTOSTART_H
#define WORKBUDDY_HOST_AUTOSTART_H

#include <stdbool.h>
#include <windows.h>

#define WB_AUTOSTART_TASK_NAME L"WorkBuddyAltTabHost"

/* These functions manage only task_name in the Task Scheduler root folder. */
HRESULT wb_host_autostart_query(const wchar_t *task_name,
        const wchar_t *executable_path, bool *enabled);
HRESULT wb_host_autostart_set(const wchar_t *task_name,
        const wchar_t *executable_path, bool enabled);

#endif
