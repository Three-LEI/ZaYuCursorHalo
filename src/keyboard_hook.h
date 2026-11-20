#pragma once

#include <windows.h>

#include <atomic>
#include <thread>

class KeyboardHook
{
public:
    KeyboardHook() = default;
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    bool Start(HWND target, UINT message);
    void Stop();
    bool IsRunning() const { return installed_.load(); }

private:
    void Run();
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);

    HWND target_ = nullptr;
    UINT message_ = 0;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> installed_{false};
    HANDLE readyEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;

    static KeyboardHook* s_active;
};
