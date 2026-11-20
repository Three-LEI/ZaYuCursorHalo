#pragma once

#include <windows.h>

#include <atomic>
#include <thread>

struct AppContext;

class MouseHook
{
public:
    MouseHook() = default;
    ~MouseHook();

    MouseHook(const MouseHook&) = delete;
    MouseHook& operator=(const MouseHook&) = delete;

    bool Start(AppContext* context);
    void Stop();

private:
    void Run();
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);

    AppContext* context_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> installed_{false};
    HANDLE readyEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;

    static MouseHook* s_active;
};
