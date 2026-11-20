#include "frame_pacer.h"

#include "util.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>

FramePacer::FramePacer() = default;

FramePacer::~FramePacer()
{
    Shutdown();
}

bool FramePacer::Initialize(HWND hwnd, bool requestDwm)
{
    Shutdown();

    BOOL composition = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&composition)))
        compositionEnabled_ = composition != FALSE;
    else
        compositionEnabled_ = false;

    useDwm_ = requestDwm && compositionEnabled_;
    QueryRefreshRate(hwnd);
    CreateTimer();

    lastWaitSeconds_ = QpcNowSeconds();
    lastDwmWaitMs_ = 0.0;
    return true;
}

void FramePacer::Shutdown()
{
    if (timer_)
    {
        CloseHandle(timer_);
        timer_ = nullptr;
    }
    highResolutionTimer_ = false;
}

void FramePacer::Reset()
{
    lastWaitSeconds_ = QpcNowSeconds();
}

bool FramePacer::QueryRefreshRate(HWND hwnd)
{
    DWM_TIMING_INFO timing = {};
    timing.cbSize = sizeof(timing);

    HRESULT hr = DwmGetCompositionTimingInfo(hwnd, &timing);
    if (FAILED(hr))
        hr = DwmGetCompositionTimingInfo(nullptr, &timing);

    LARGE_INTEGER qpcFrequency = {};
    QueryPerformanceFrequency(&qpcFrequency);
    if (SUCCEEDED(hr) && timing.qpcRefreshPeriod > 0 && qpcFrequency.QuadPart > 0)
    {
        const double refresh = static_cast<double>(qpcFrequency.QuadPart) /
            static_cast<double>(timing.qpcRefreshPeriod);
        if (refresh >= 20.0 && refresh <= 500.0)
        {
            refreshHz_ = refresh;
            periodSeconds_ = 1.0 / refreshHz_;
            return true;
        }
    }

    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode))
    {
        if (mode.dmDisplayFrequency > 1)
        {
            refreshHz_ = static_cast<double>(mode.dmDisplayFrequency);
            periodSeconds_ = 1.0 / refreshHz_;
            return true;
        }
    }

    refreshHz_ = 60.0;
    periodSeconds_ = 1.0 / 60.0;
    return false;
}

bool FramePacer::CreateTimer()
{
    constexpr DWORD kHighResolutionFlag = 0x00000002; // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, kHighResolutionFlag, TIMER_ALL_ACCESS);
    highResolutionTimer_ = timer_ != nullptr;
    if (!timer_)
        timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    return timer_ != nullptr;
}

void FramePacer::WaitUntil(double targetSeconds)
{
    for (;;)
    {
        const double now = QpcNowSeconds();
        const double remaining = targetSeconds - now;
        if (remaining <= 0.0)
            return;

        // Leave at most ~1 ms to a short spin; waitable timers can have coarse
        // granularity on Windows 7, while the spin keeps the final landing tight.
        const double timerSlice = (std::max)(0.0, remaining - 0.0015);
        if (timer_ && timerSlice > 0.0)
        {
            LARGE_INTEGER due = {};
            due.QuadPart = -static_cast<LONGLONG>(timerSlice * 10000000.0);
            if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
            {
                DWORD timeoutMs = static_cast<DWORD>(std::ceil(timerSlice * 1000.0)) + 2;
                WaitForSingleObject(timer_, timeoutMs);
                continue;
            }
        }

        if (remaining > 0.003)
            Sleep(1);
        else
            SwitchToThread();
    }
}

void FramePacer::WaitNextFrame()
{
    const double before = QpcNowSeconds();
    if (useDwm_)
    {
        DwmFlush();
        const double after = QpcNowSeconds();
        lastDwmWaitMs_ = (after - before) * 1000.0;
    }
    else
    {
        lastDwmWaitMs_ = 0.0;
    }

    const double now = QpcNowSeconds();
    if ((now - lastWaitSeconds_) < periodSeconds_ * 0.55)
    {
        double target = lastWaitSeconds_ + periodSeconds_;
        if (target < now)
            target = now;
        WaitUntil(target);
    }

    lastWaitSeconds_ = QpcNowSeconds();
}
