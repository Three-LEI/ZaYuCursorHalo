#pragma once

#include "settings.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

class TimerWidgetHost
{
public:
    virtual ~TimerWidgetHost() = default;
    virtual void OnTimerPositionChanged(int x, int y) = 0;
    virtual void OnTimerSettingsChanged(TimerMode mode, int countdownMinutes) = 0;
};

class TimerWidget
{
public:
    TimerWidget();
    ~TimerWidget();

    TimerWidget(const TimerWidget&) = delete;
    TimerWidget& operator=(const TimerWidget&) = delete;

    bool Create(HINSTANCE instance);
    void Destroy();
    bool IsCreated() const { return hwnd_ != nullptr; }
    void SetHost(TimerWidgetHost* host) { host_ = host; }

    void ApplySettings(
        bool enabled,
        bool showHours,
        bool resetOnStop,
        TimerMode mode,
        int countdownMinutes,
        unsigned accentRgb,
        bool darkMode,
        int x,
        int y,
        bool hasPosition);
    void Start();
    void Stop();
    void Reset();
    void Refresh();
    void MarkSegment();

    bool IsEnabled() const { return enabled_; }
    bool IsRunning() const { return running_; }
    ULONGLONG ElapsedMs() const;

private:
    struct TimerRender;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK CountdownDialogProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    bool LoadFont();
    void EnsureFont(int pixelHeight);
    void RebuildLayout();
    void Paint();
    void Present();
    void OnTick();
    void UpdateHover(int x, int y);
    void BeginDrag();
    void UpdateDrag();
    void EndDrag(bool commit);
    void ClampToWorkArea(const POINT* cursor);
    void DefaultPosition();
    std::wstring FormatElapsed() const;
    std::wstring FormatSegment() const;
    ULONGLONG DisplayMs() const;
    void ShowModeMenu();
    void ShowCustomCountdownDialog();
    void SetMode(TimerMode mode, int countdownMinutes);
    void EnsureSegmentFont(int pixelHeight);
    bool IsReadable(unsigned color, unsigned background) const;
    static UINT SystemDpi();
    static UINT DpiForMonitor(HMONITOR monitor);

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    TimerWidgetHost* host_ = nullptr;
    std::unique_ptr<TimerRender> render_;
    bool classRegistered_ = false;
    bool gdiplusStarted_ = false;
    ULONG_PTR gdiplusToken_ = 0;
    bool enabled_ = true;
    bool showHours_ = true;
    bool resetOnStop_ = false;
    bool darkMode_ = false;
    bool running_ = false;
    bool visible_ = false;
    bool positionSet_ = false;
    bool dragging_ = false;
    bool hoverPlay_ = false;
    bool hoverStop_ = false;
    bool hoverMark_ = false;
    bool hoverGrip_ = false;
    bool trackingLeave_ = false;
    TimerMode mode_ = TimerMode::CountUp;
    int countdownMinutes_ = 5;
    int pendingCustomMinutes_ = 0;
    bool countdownFinished_ = false;
    std::vector<ULONGLONG> marks_;
    ULONGLONG lastSegmentMs_ = 0;
    ULONGLONG accumulatedMs_ = 0;
    ULONGLONG startedAt_ = 0;
    ULONGLONG lastDisplayedSecond_ = static_cast<ULONGLONG>(-1);
    unsigned accentRgb_ = 0x5B8DFF;
    int x_ = 0;
    int y_ = 0;
    int width_ = 0;
    int height_ = 0;
    int scaleDpi_ = 96;
    float scale_ = 1.0f;
    int digitWidth_ = 0;
    int colonWidth_ = 0;
    RECT capsuleRect_ = {};
    RECT gripRect_ = {};
    RECT playRect_ = {};
    RECT stopRect_ = {};
    RECT markRect_ = {};
    RECT timeRect_ = {};
    RECT segmentRect_ = {};
    POINT dragCursor_ = {};
    POINT dragWindow_ = {};
};
