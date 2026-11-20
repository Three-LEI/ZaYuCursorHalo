#include "keyboard_hook.h"

KeyboardHook* KeyboardHook::s_active = nullptr;

KeyboardHook::~KeyboardHook()
{
    Stop();
}

bool KeyboardHook::Start(HWND target, UINT message)
{
    if (!target || message == 0)
        return false;

    Stop();

    target_ = target;
    message_ = message;
    running_.store(true);
    installed_.store(false);
    readyEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent_ || !stopEvent_)
    {
        Stop();
        return false;
    }

    thread_ = std::thread(&KeyboardHook::Run, this);
    const DWORD wait = WaitForSingleObject(readyEvent_, 3000);
    const bool ok = (wait == WAIT_OBJECT_0) && installed_.load();
    if (!ok)
        Stop();
    return ok;
}

void KeyboardHook::Stop()
{
    running_.store(false);
    if (stopEvent_)
        SetEvent(stopEvent_);
    if (thread_.joinable())
        thread_.join();
    if (readyEvent_)
    {
        CloseHandle(readyEvent_);
        readyEvent_ = nullptr;
    }
    if (stopEvent_)
    {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    target_ = nullptr;
    message_ = 0;
    installed_.store(false);
}

void KeyboardHook::Run()
{
    s_active = this;
    const HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc, GetModuleHandleW(nullptr), 0);
    installed_.store(hook != nullptr);
    if (readyEvent_)
        SetEvent(readyEvent_);

    if (!hook)
    {
        s_active = nullptr;
        running_.store(false);
        return;
    }

    MSG message = {};
    while (running_.load())
    {
        const DWORD wait = MsgWaitForMultipleObjectsEx(
            1, &stopEvent_, 1000, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0)
            break;

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running_.store(false);
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    UnhookWindowsHookEx(hook);
    s_active = nullptr;
    running_.store(false);
}

LRESULT CALLBACK KeyboardHook::HookProc(int code, WPARAM wParam, LPARAM lParam)
{
    KeyboardHook* self = s_active;
    if (code == HC_ACTION && self && self->target_)
    {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN ||
            wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            const KBDLLHOOKSTRUCT* data = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
            if (data)
            {
                const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
                PostMessageW(self->target_, self->message_, data->vkCode, down ? 1 : 0);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
