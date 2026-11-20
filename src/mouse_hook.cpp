#include "mouse_hook.h"

#include "app_context.h"

MouseHook* MouseHook::s_active = nullptr;

MouseHook::~MouseHook()
{
    Stop();
}

bool MouseHook::Start(AppContext* context)
{
    if (!context || !context->mouseWake)
        return false;

    Stop();

    context_ = context;
    running_.store(true);
    installed_.store(false);
    readyEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent_ || !stopEvent_)
    {
        Stop();
        return false;
    }

    thread_ = std::thread(&MouseHook::Run, this);
    const DWORD wait = WaitForSingleObject(readyEvent_, 3000);
    const bool ok = (wait == WAIT_OBJECT_0) && installed_.load();
    if (!ok)
        Stop();
    return ok;
}

void MouseHook::Stop()
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
    context_ = nullptr;
}

void MouseHook::Run()
{
    s_active = this;
    const HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, HookProc, GetModuleHandleW(nullptr), 0);
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

LRESULT CALLBACK MouseHook::HookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && s_active && s_active->context_ && s_active->context_->mouseWake)
    {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN)
        {
            const HWND target = s_active->context_->clickFeedbackWindow.load();
            const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
            if (target && info)
            {
                PostMessageW(
                    target,
                    WM_APP + 70,
                    wParam,
                    MAKELPARAM(info->pt.x, info->pt.y));
            }
        }

        switch (wParam)
        {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            SetEvent(s_active->context_->mouseWake);
            break;
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
