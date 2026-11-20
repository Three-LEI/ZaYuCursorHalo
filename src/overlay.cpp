#include "overlay.h"

#include "app_context.h"
#include "util.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr wchar_t kOverlayClass[] = L"CursorHalo.Overlay.P0";
constexpr double kIdleThresholdSeconds = 0.35;

using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

UINT SystemDpi()
{
    HDC dc = GetDC(nullptr);
    if (!dc)
        return 96;
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    if (dpi >= 72 && dpi <= 480)
        return static_cast<UINT>(dpi);
    return 96;
}
} // namespace

Overlay::~Overlay()
{
    Stop();
}

bool Overlay::Start(HINSTANCE instance, AppContext* context)
{
    if (!context)
        return false;
    Stop();

    instance_ = instance;
    context_ = context;
    readyOk_.store(false);
    readyEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent_)
    {
        context_ = nullptr;
        return false;
    }

    thread_ = std::thread(&Overlay::Run, this);
    const DWORD wait = WaitForSingleObject(readyEvent_, 5000);
    if (wait != WAIT_OBJECT_0 || !readyOk_.load())
    {
        Stop();
        return false;
    }
    return true;
}

void Overlay::Stop()
{
    if (context_)
    {
        context_->running.store(false);
        if (context_->stateWake)
            SetEvent(context_->stateWake);
    }
    if (thread_.joinable())
        thread_.join();
    if (readyEvent_)
    {
        CloseHandle(readyEvent_);
        readyEvent_ = nullptr;
    }
    context_ = nullptr;
}

void Overlay::Run()
{
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(comHr);
    if (comHr == RPC_E_CHANGED_MODE)
        comInitialized = false;

    const bool resourcesOk =
        RegisterWindowClass() &&
        CreateOverlayWindow() &&
        renderer_.Initialize() &&
        pacer_.Initialize(hwnd_, context_->pacerMode == PacerMode::Dwm);

    if (resourcesOk)
    {
        context_->stats.SetRunInfo(
            pacer_.refreshHz(),
            pacer_.modeName(),
            pacer_.compositionEnabled(),
            context_->radiusDip.load(),
            context_->benchmark.load());
        readyOk_.store(true);
    }
    else
    {
        readyOk_.store(false);
    }

    if (readyEvent_)
        SetEvent(readyEvent_);

    if (resourcesOk)
    {
        lastMoveSeconds_ = QpcNowSeconds();
        while (context_->running.load())
        {
            PumpMessages();
            if (!context_->running.load() || !hwnd_)
                break;
            if (!IsWindow(hwnd_))
                break;

            if (refreshDirty_)
            {
                pacer_.Initialize(hwnd_, context_->pacerMode == PacerMode::Dwm);
                context_->stats.SetRunInfo(
                    pacer_.refreshHz(),
                    pacer_.modeName(),
                    pacer_.compositionEnabled(),
                    context_->radiusDip.load(),
                    context_->benchmark.load());
                pacer_.Reset();
                refreshDirty_ = false;
            }

            if (!context_->enabled.load())
            {
                HideOverlay();
                idle_ = false;
                hasLastPos_ = false;
                hasPresented_ = false;
                WaitForSingleObject(context_->stateWake, 250);
                continue;
            }

            if (idle_ && !context_->benchmark.load())
            {
                // The low-level hook can be bypassed while an elevated window
                // (for example Task Manager) owns the input. Poll the cursor at
                // a low rate while idle so the halo always resumes even when no
                // mouseWake event arrives.
                HANDLE events[2] = {context_->mouseWake, context_->stateWake};
                const DWORD wait = WaitForMultipleObjects(2, events, FALSE, 30);
                if (wait == WAIT_OBJECT_0 || wait == WAIT_OBJECT_0 + 1)
                {
                    idle_ = false;
                    lastMoveSeconds_ = QpcNowSeconds();
                    pacer_.Reset();
                }
                else if (wait == WAIT_TIMEOUT)
                {
                    POINT cursor = {};
                    if (GetCursorPos(&cursor) &&
                        (!hasLastPos_ || cursor.x != lastPos_.x || cursor.y != lastPos_.y))
                    {
                        idle_ = false;
                        lastMoveSeconds_ = QpcNowSeconds();
                        pacer_.Reset();
                    }
                }
                continue;
            }

            POINT cursor = {};
            if (!GetCursorPos(&cursor))
            {
                Sleep(10);
                continue;
            }

            const UINT dpi = DpiForPoint(cursor);
            const float scale = static_cast<float>(dpi) / 96.0f;

            if (visible_)
                EnsureTopmost();

            const bool scaleChanged = std::fabs(scale - lastScale_) > 0.001f;
            const bool moved = !hasLastPos_ || cursor.x != lastPos_.x || cursor.y != lastPos_.y;
            const bool styleChanged = context_->styleVersion.load() != renderedStyleVersion_;
            const bool needUpdate = moved || scaleChanged || styleChanged || !visible_;
            if (needUpdate)
            {
                if (context_->benchmark.load() && !benchmarkMotionStarted_ && hasLastPos_ && moved)
                {
                    context_->stats.Reset();
                    hasPresented_ = false;
                    benchmarkMotionStarted_ = true;
                }

                if (UpdateOverlay(cursor, scale))
                {
                    const double now = QpcNowSeconds();
                    if (!visible_)
                        ShowOverlay();
                    hasLastPos_ = true;
                    lastPos_ = cursor;
                    lastScale_ = scale;
                    if (moved || !visible_)
                        lastMoveSeconds_ = now;
                }
            }

            const double now = QpcNowSeconds();
            if (!context_->benchmark.load() && (now - lastMoveSeconds_) >= kIdleThresholdSeconds)
            {
                // The auto-reset mouse event is consumed by the idle wait. A
                // movement that happens just before the transition wakes us
                // immediately instead of being lost.
                idle_ = true;
                hasPresented_ = false;
                continue;
            }

            pacer_.WaitNextFrame();
        }
    }

    HideOverlay();
    Cleanup();

    if (comInitialized)
        CoUninitialize();
}

bool Overlay::RegisterWindowClass()
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = 0;
    windowClass.lpfnWndProc = &Overlay::WndProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = nullptr;
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kOverlayClass;

    if (RegisterClassExW(&windowClass))
    {
        classRegistered_ = true;
        return true;
    }
    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
        return true;
    return false;
}

bool Overlay::CreateOverlayWindow()
{
    const DWORD exStyle =
        WS_EX_LAYERED |
        WS_EX_TRANSPARENT |
        WS_EX_TOOLWINDOW |
        WS_EX_TOPMOST |
        WS_EX_NOACTIVATE;

    hwnd_ = CreateWindowExW(
        exStyle,
        kOverlayClass,
        L"CursorHalo overlay",
        WS_POPUP,
        0,
        0,
        64,
        64,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_)
        return false;

    // The window is created topmost. The render loop re-asserts topmost only
    // when a later WS_EX_TOPMOST window (for example WPS's New panel or the
    // taskbar after activation) gets above it.
    return true;
}

void Overlay::PumpMessages()
{
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
            return;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool Overlay::UpdateOverlay(POINT cursor, float scale)
{
    const int radiusDip = context_->radiusDip.load();
    const unsigned accentRgb = context_->accentRgb.load();
    const int opacityPercent = context_->opacityPercent.load();
    const unsigned styleVersion = context_->styleVersion.load();
    const int haloStyle = context_->haloStyle.load();

    const bool contentChanged =
        !renderer_.HasSurface() ||
        std::fabs(scale - renderedScale_) > 0.001f ||
        renderedRadiusDip_ != radiusDip ||
        renderedStyleVersion_ != styleVersion;

    double drawMs = 0.0;
    double presentMs = 0.0;
    bool ok = false;

    if (contentChanged)
    {
        const float marginDip = 18.0f;
        int surfaceSize = static_cast<int>(
            std::ceil((static_cast<float>(radiusDip) + marginDip) * 2.0f * scale)) + 2;
        surfaceSize = (std::max)(surfaceSize, 32);
        if ((surfaceSize & 1) != 0)
            ++surfaceSize;

        if (!renderer_.EnsureSurface(static_cast<UINT>(surfaceSize), static_cast<UINT>(surfaceSize)))
            return false;

        const RenderResult result = renderer_.Render(
            hwnd_,
            cursor,
            scale,
            static_cast<float>(radiusDip),
            accentRgb,
            opacityPercent,
            haloStyle);
        if (!result.presentOk)
            return false;

        drawMs = result.drawMs;
        presentMs = result.presentMs;
        renderedScale_ = scale;
        renderedRadiusDip_ = radiusDip;
        renderedStyleVersion_ = styleVersion;
        if (!dumpedFrame_ && !context_->dumpFramePath.empty())
        {
            renderer_.SavePng(context_->dumpFramePath);
            dumpedFrame_ = true;
        }
        ok = true;
    }
    else
    {
        ok = renderer_.Move(hwnd_, cursor, presentMs);
    }

    if (!ok)
        return false;

    const double now = QpcNowSeconds();
    const double intervalMs = hasPresented_ ? (now - lastPresentSeconds_) * 1000.0 : 0.0;
    context_->stats.Record(intervalMs, drawMs, presentMs, pacer_.lastDwmWaitMs());
    lastPresentSeconds_ = now;
    hasPresented_ = true;
    return true;
}

void Overlay::ShowOverlay()
{
    if (!hwnd_ || visible_)
        return;
    EnsureTopmost();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;
    EnsureTopmost();
}

void Overlay::HideOverlay()
{
    if (hwnd_ && visible_)
        ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
}

void Overlay::EnsureTopmost()
{
    if (!hwnd_)
        return;

    // WPS 的“新建”面板等应用弹层本身就是 WS_EX_TOPMOST。普通 topmost
    // 窗口创建得早时会落在它们下面，因此只在检测到上方还有窗口时重排，
    // 避免每帧无条件 SetWindowPos。
    if (GetWindow(hwnd_, GW_HWNDPREV) == nullptr)
        return;
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}



void Overlay::Cleanup()
{
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    renderer_.Shutdown();
    pacer_.Shutdown();
    if (classRegistered_)
    {
        UnregisterClassW(kOverlayClass, instance_);
        classRegistered_ = false;
    }
    visible_ = false;
}

UINT Overlay::DpiForPoint(POINT point)
{
    static HMODULE shcore = LoadLibraryW(L"shcore.dll");
    static GetDpiForMonitorFn getDpiForMonitor = shcore
        ? reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"))
        : nullptr;

    if (getDpiForMonitor)
    {
        HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 0;
        UINT dpiY = 0;
        if (SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX >= 72 && dpiX <= 480)
            return dpiX;
    }
    return SystemDpi();
}

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Overlay* self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_NCCREATE:
    {
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<Overlay*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_DISPLAYCHANGE:
    case WM_DWMCOMPOSITIONCHANGED:
        if (self)
            self->refreshDirty_ = true;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
