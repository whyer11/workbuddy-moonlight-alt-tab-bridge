#include <windows.h>
#include <stdio.h>

static volatile LONG tab_down_count;
static DWORD message_thread_id;

static LRESULT CALLBACK observe_keyboard(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *event = (const KBDLLHOOKSTRUCT *)data;
        if (event->vkCode == VK_TAB && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN))
            InterlockedIncrement(&tab_down_count);
    }
    return CallNextHookEx(NULL, code, message, data);
}

static void send_key(WORD key, DWORD flags) {
    INPUT input; ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD; input.ki.wVk = key; input.ki.dwFlags = flags;
    input.ki.dwExtraInfo = (ULONG_PTR)0x574254455354ULL;
    SendInput(1, &input, sizeof(input));
}

static DWORD WINAPI exercise_hotkey(void *context) {
    (void)context; Sleep(250);
    send_key(VK_MENU, 0); send_key(VK_F12, 0); Sleep(50);
    send_key(VK_F12, KEYEVENTF_KEYUP); send_key(VK_MENU, KEYEVENTF_KEYUP);
    Sleep(500); PostThreadMessageW(message_thread_id, WM_QUIT, 0, 0); return 0;
}

int main(void) {
    message_thread_id = GetCurrentThreadId();
    MSG message; PeekMessageW(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, observe_keyboard, GetModuleHandleW(NULL), 0);
    if (hook == NULL) return 2;
    HANDLE worker = CreateThread(NULL, 0, exercise_hotkey, NULL, 0, NULL);
    while (GetMessageW(&message, NULL, 0, 0) > 0) { }
    WaitForSingleObject(worker, 2000); CloseHandle(worker); UnhookWindowsHookEx(hook);
    LONG tabs = InterlockedCompareExchange(&tab_down_count, 0, 0);
    printf("ALT_TAB_BRIDGE tab_down=%ld\n", tabs);
    return tabs == 1 ? 0 : 1;
}
