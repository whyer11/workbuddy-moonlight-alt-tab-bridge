#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define _WIN32_IE 0x0600

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

#include "HostTrayIcon.h"

/* Requires an interactive Explorer session and the production host.rc resource.
 * Uses only its own icon: no host mutex, keyboard hooks, or Explorer restart.
 * A message-only tray receiver must fail the broadcast recovery check below.
 */
enum { TEST_ICON_ID = 1, TEST_WAIT_MS = 3000 };

static bool wait_for_icon(const NOTIFYICONIDENTIFIER *identifier, bool present) {
    ULONGLONG deadline = GetTickCount64() + TEST_WAIT_MS;
    do {
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        RECT rectangle;
        bool found = SUCCEEDED(Shell_NotifyIconGetRect(identifier, &rectangle));
        if (found == present) return true;
        MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
    } while (GetTickCount64() < deadline);
    return false;
}

int main(void) {
    struct wb_host_tray tray = {0};
    NOTIFYICONIDENTIFIER identifier = {0};
    NOTIFYICONDATAW data = {0};
    wchar_t message_name[128];
    int result = 1;

    if (!wb_host_tray_start(&tray, GetModuleHandleW(NULL),
            L"WorkBuddy tray integration test")) {
        fprintf(stderr, "TRAY_TEST_SETUP_FAILED error=%lu\n", GetLastError());
        goto cleanup;
    }

    identifier.cbSize = sizeof(identifier);
    identifier.hWnd = tray.window;
    identifier.uID = TEST_ICON_ID;
    if (!wait_for_icon(&identifier, true)) {
        fputs("TRAY_TEST_INITIAL_ICON_MISSING\n", stderr);
        goto cleanup;
    }

    /* Never broadcast the real TaskbarCreated message: other applications'
     * icons must remain untouched. Route this unique message through the same
     * production recovery handler, using the real broadcast delivery path.
     */
    swprintf(message_name, ARRAYSIZE(message_name),
            L"WorkBuddyTrayRecoveryTest-%lu-%llu",
            GetCurrentProcessId(), GetTickCount64());
    tray.taskbar_created_message = RegisterWindowMessageW(message_name);
    if (tray.taskbar_created_message == 0) {
        fprintf(stderr, "TRAY_TEST_REGISTER_MESSAGE_FAILED error=%lu\n", GetLastError());
        goto cleanup;
    }

    data.cbSize = sizeof(data);
    data.hWnd = tray.window;
    data.uID = TEST_ICON_ID;
    if (!Shell_NotifyIconW(NIM_DELETE, &data)) {
        fputs("TRAY_TEST_REMOVE_ICON_FAILED\n", stderr);
        goto cleanup;
    }
    if (!wait_for_icon(&identifier, false)) {
        fputs("TRAY_TEST_REMOVED_ICON_STILL_PRESENT\n", stderr);
        goto cleanup;
    }

    /* Leave icon_added stale, as it is after Explorer discards the icon. */
    if (!PostMessageW(HWND_BROADCAST, tray.taskbar_created_message, 0, 0)) {
        fprintf(stderr, "TRAY_TEST_BROADCAST_FAILED error=%lu\n", GetLastError());
        goto cleanup;
    }
    if (!wait_for_icon(&identifier, true) || !tray.icon_added) {
        fputs("TRAY_TEST_BROADCAST_RECOVERY_FAILED: removed icon was not restored\n",
                stderr);
        goto cleanup;
    }
    if (IsWindowVisible(tray.window)) {
        fputs("TRAY_TEST_RECEIVER_MUST_REMAIN_HIDDEN\n", stderr);
        goto cleanup;
    }
    result = 0;

cleanup:
    wb_host_tray_stop(&tray);
    if (identifier.hWnd != NULL
            && (!wait_for_icon(&identifier, false) || IsWindow(identifier.hWnd))) {
        fputs("TRAY_TEST_CLEANUP_FAILED\n", stderr);
        result = 1;
    }
    if (result == 0) {
        puts("HostTrayIntegrationTest: PASS (broadcast restores own icon, "
                "receiver stays hidden, "
                "cleanup removes icon and window)");
    }
    return result;
}
