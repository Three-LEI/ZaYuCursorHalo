#pragma once

#include <windows.h>

class FramePacer
{
public:
    FramePacer();
    ~FramePacer();

    FramePacer(const FramePacer&) = delete;
    FramePacer& operator=(const FramePacer&) = delete;

    bool Initialize(HWND hwnd, bool requestDwm);
    void Shutdown();
    void Reset();

    void WaitNextFrame();

    double refreshHz() const { return refreshHz_; }
    bool compositionEnabled() const { return compositionEnabled_; }
    bool usingDwm() const { return useDwm_; }
    const wchar_t* modeName() const { return useDwm_ ? L"dwm" : L"timer"; }
    double lastDwmWaitMs() const { return lastDwmWaitMs_; }

private:
    bool QueryRefreshRate(HWND hwnd);
    bool CreateTimer();
    void WaitUntil(double targetSeconds);

    double refreshHz_ = 60.0;
    double periodSeconds_ = 1.0 / 60.0;
    bool compositionEnabled_ = false;
    bool useDwm_ = false;
    double lastWaitSeconds_ = 0.0;
    double lastDwmWaitMs_ = 0.0;
    HANDLE timer_ = nullptr;
    bool highResolutionTimer_ = false;
};
