#include <windows.h>

#include <stdio.h>

static volatile LONG win_down_count;
static volatile LONG win_up_count;
static DWORD message_thread_id;

static LRESULT CALLBACK observe_keyboard(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *event = (const KBDLLHOOKSTRUCT *)data;
        if (event->vkCode == VK_LWIN) {
            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
                InterlockedIncrement(&win_down_count);
            } else if (message == WM_KEYUP || message == WM_SYSKEYUP) {
                InterlockedIncrement(&win_up_count);
            }
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
    return SendInput(1, &input, sizeof(input)) == 1;
}

static DWORD WINAPI exercise_sentinel(void *context) {
    (void)context;
    Sleep(250);

    for (int cycle = 0; cycle < 2; ++cycle) {
        send_scroll_lock(0);
        Sleep(30);
        send_scroll_lock(0);
        Sleep(30);
        send_scroll_lock(KEYEVENTF_KEYUP);
        Sleep(300);
    }

    Sleep(500);
    PostThreadMessageW(message_thread_id, WM_QUIT, 0, 0);
    return 0;
}

int main(void) {
    message_thread_id = GetCurrentThreadId();
    MSG message;
    PeekMessageW(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    HHOOK hook = SetWindowsHookExW(
            WH_KEYBOARD_LL, observe_keyboard, GetModuleHandleW(NULL), 0);
    if (hook == NULL) {
        fprintf(stderr, "OBSERVER_HOOK_FAILED error=%lu\n", GetLastError());
        return 2;
    }

    HANDLE worker = CreateThread(NULL, 0, exercise_sentinel, NULL, 0, NULL);
    if (worker == NULL) {
        fprintf(stderr, "WORKER_START_FAILED error=%lu\n", GetLastError());
        UnhookWindowsHookEx(hook);
        return 3;
    }

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    WaitForSingleObject(worker, 2000);
    CloseHandle(worker);
    UnhookWindowsHookEx(hook);

    LONG down = InterlockedCompareExchange(&win_down_count, 0, 0);
    LONG up = InterlockedCompareExchange(&win_up_count, 0, 0);
    printf("WIN_KEY_BRIDGE down=%ld up=%ld\n", down, up);
    if (down != 2 || up != 2) {
        fprintf(stderr, "EXPECTED_TWO_WIN_KEY_CYCLES\n");
        return 1;
    }
    return 0;
}
