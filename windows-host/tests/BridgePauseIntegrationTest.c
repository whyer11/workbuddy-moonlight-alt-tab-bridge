#include <windows.h>
#include <stdio.h>

#include "KeyboardBridgeCore.h"

static volatile LONG win_down_count;
static DWORD message_thread_id;

static LRESULT CALLBACK observe_keyboard(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *event = (const KBDLLHOOKSTRUCT *)data;
        if (event->vkCode == VK_LWIN
                && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)) {
            InterlockedIncrement(&win_down_count);
        }
    }
    return CallNextHookEx(NULL, code, message, data);
}

static BOOL send_scroll_lock(DWORD flags) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SCROLL;
    input.ki.dwFlags = flags;
    input.ki.dwExtraInfo = (ULONG_PTR)0x574254455354ULL;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

static void send_scroll_lock_cycle(void) {
    send_scroll_lock(0);
    Sleep(30);
    send_scroll_lock(KEYEVENTF_KEYUP);
    Sleep(150);
}

static LRESULT CALLBACK hidden_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

static DWORD WINAPI exercise_pause(void *context) {
    HWND window = (HWND)context;
    Sleep(250);

    send_scroll_lock_cycle();
    Sleep(200);

    PostMessageW(window, WM_APP + 100, 1, 0);
    Sleep(100);
    send_scroll_lock_cycle();
    Sleep(200);

    PostMessageW(window, WM_APP + 100, 0, 0);
    Sleep(100);
    send_scroll_lock_cycle();
    Sleep(200);

    PostThreadMessageW(message_thread_id, WM_QUIT, 0, 0);
    return 0;
}

int main(void) {
    message_thread_id = GetCurrentThreadId();
    MSG message;
    PeekMessageW(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    WNDCLASSEXW window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = hidden_window_proc;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = L"WorkBuddyPauseTestWindow";
    if (RegisterClassExW(&window_class) == 0
            && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        fprintf(stderr, "WINDOW_CLASS_FAILED error=%lu\n", GetLastError());
        return 2;
    }

    HWND window = CreateWindowExW(0, L"WorkBuddyPauseTestWindow", L"pause-test",
            0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    if (window == NULL) {
        fprintf(stderr, "WINDOW_CREATE_FAILED error=%lu\n", GetLastError());
        return 2;
    }

    HHOOK observer = SetWindowsHookExW(
            WH_KEYBOARD_LL, observe_keyboard, GetModuleHandleW(NULL), 0);
    if (observer == NULL) {
        fprintf(stderr, "OBSERVER_HOOK_FAILED error=%lu\n", GetLastError());
        DestroyWindow(window);
        return 2;
    }

    struct wb_keyboard_core core;
    if (!wb_keyboard_core_start(&core, GetModuleHandleW(NULL), window)) {
        fprintf(stderr, "KEYBOARD_CORE_START_FAILED error=%lu\n", GetLastError());
        UnhookWindowsHookEx(observer);
        DestroyWindow(window);
        return 3;
    }

    HANDLE worker = CreateThread(NULL, 0, exercise_pause, window, 0, NULL);
    if (worker == NULL) {
        wb_keyboard_core_stop(&core);
        UnhookWindowsHookEx(observer);
        DestroyWindow(window);
        return 3;
    }

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (message.message == WM_APP + 100 && message.hwnd == window) {
            bool paused = message.wParam == 1;
            if (!wb_keyboard_core_set_paused(&core, paused)) {
                fprintf(stderr, "SET_PAUSED_FAILED paused=%d\n", paused ? 1 : 0);
                break;
            }
            continue;
        }
        if (message.hwnd == window && wb_keyboard_core_handle_message(&core, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    WaitForSingleObject(worker, 3000);
    CloseHandle(worker);
    wb_keyboard_core_stop(&core);
    UnhookWindowsHookEx(observer);
    DestroyWindow(window);

    LONG down = InterlockedCompareExchange(&win_down_count, 0, 0);
    printf("BRIDGE_PAUSE win_down=%ld\n", down);
    /* running cycle produces 1 Win down; paused cycle must produce 0; resumed cycle 1 */
    return down == 2 ? 0 : 1;
}
