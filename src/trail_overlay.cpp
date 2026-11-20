#include "trail_overlay.h"

#include "app_context.h"

#include <objidl.h>
#include <propidl.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using std::max;
using std::min;

#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

#pragma comment(lib, "gdiplus.lib")

namespace
{
constexpr wchar_t kTrailClass[] = L"ZaYuCursorHalo.Trail.P1";
constexpr UINT kFrameIntervalMs = 33;
constexpr ULONGLONG kTrailLifetimeMs = 320;
constexpr float kTrailLengthDip = 260.0f;
constexpr float kTrailBaseWidthDip = 5.5f;
constexpr float kTrailPaddingDip = 18.0f;

float SystemScale()
{
    HDC dc = GetDC(nullptr);
    if (!dc)
        return 1.0f;
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return (dpi >= 72 && dpi <= 480) ? static_cast<float>(dpi) / 96.0f : 1.0f;
}

float Distance(POINT a, POINT b)
{
    const float dx = static_cast<float>(a.x - b.x);
    const float dy = static_cast<float>(a.y - b.y);
    return std::sqrt(dx * dx + dy * dy);
}
} // namespace

TrailOverlay::~TrailOverlay()
{
    Stop();
}

bool TrailOverlay::Start(AppContext* context)
{
    if (!context)
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

    thread_ = std::thread(&TrailOverlay::Run, this);
    const DWORD wait = WaitForSingleObject(readyEvent_, 3000);
    const bool ok = wait == WAIT_OBJECT_0 && installed_.load();
    if (!ok)
        Stop();
    return ok;
}

void TrailOverlay::Stop()
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

void TrailOverlay::Run()
{
    Gdiplus::GdiplusStartupInput input;
    gdiplusStarted_ =
        Gdiplus::GdiplusStartup(&gdiplusToken_, &input, nullptr) == Gdiplus::Ok;

    if (!CreateTrailWindow())
    {
        Cleanup();
        if (readyEvent_)
            SetEvent(readyEvent_);
        running_.store(false);
        return;
    }

    installed_.store(true);
    if (readyEvent_)
        SetEvent(readyEvent_);

    MSG message = {};
    while (running_.load())
    {
        const DWORD wait = MsgWaitForMultipleObjectsEx(
            1,
            &stopEvent_,
            kFrameIntervalMs,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);
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

        Update();
    }

    Hide();
    Cleanup();
    running_.store(false);
}

bool TrailOverlay::CreateTrailWindow()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &TrailOverlay::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kTrailClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    classRegistered_ = true;

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        kTrailClass,
        L"ZaYuCursorHalo Trail",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);
    return hwnd_ != nullptr;
}

void TrailOverlay::Update()
{
    if (!context_ || !running_.load())
        return;

    if (!context_->enabled.load() ||
        context_->trayPanelOpen.load() || context_->benchmark.load())
    {
        Hide();
        samples_.clear();
        hasLastCursor_ = false;
        return;
    }

    POINT cursor = {};
    if (!GetCursorPos(&cursor))
        return;
    const ULONGLONG now = GetTickCount64();

    if (!hasLastCursor_ || cursor.x != lastCursor_.x || cursor.y != lastCursor_.y)
    {
        if (samples_.empty() || Distance(cursor, samples_.back().point) >= 0.75f)
            samples_.push_back({cursor, now});
        lastCursor_ = cursor;
        hasLastCursor_ = true;
    }

    Prune(now);
    if (samples_.size() < 2)
    {
        Hide();
        return;
    }

    float pathLength = 0.0f;
    for (size_t i = 1; i < samples_.size(); ++i)
        pathLength += Distance(samples_[i - 1].point, samples_[i].point);
    if (pathLength < 1.5f)
    {
        Hide();
        return;
    }

    Render();
}

void TrailOverlay::Prune(ULONGLONG now)
{
    while (!samples_.empty() && now - samples_.front().time > kTrailLifetimeMs)
        samples_.pop_front();

    const float maxLength = kTrailLengthDip * SystemScale();
    while (samples_.size() > 2)
    {
        float length = 0.0f;
        for (size_t i = 1; i < samples_.size(); ++i)
            length += Distance(samples_[i - 1].point, samples_[i].point);
        if (length <= maxLength)
            break;
        samples_.pop_front();
    }
    while (samples_.size() > 64)
        samples_.pop_front();
}

void TrailOverlay::Render()
{
    const float scale = SystemScale();
    const int padding = static_cast<int>(std::lround(kTrailPaddingDip * scale));
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    RECT bounds = {INT_MAX, INT_MAX, INT_MIN, INT_MIN};
    for (const Sample& sample : samples_)
    {
        bounds.left = static_cast<LONG>(std::min<int>(static_cast<int>(bounds.left), sample.point.x));
        bounds.top = static_cast<LONG>(std::min<int>(static_cast<int>(bounds.top), sample.point.y));
        bounds.right = static_cast<LONG>(std::max<int>(static_cast<int>(bounds.right), sample.point.x));
        bounds.bottom = static_cast<LONG>(std::max<int>(static_cast<int>(bounds.bottom), sample.point.y));
    }
    bounds.left -= padding;
    bounds.top -= padding;
    bounds.right += padding;
    bounds.bottom += padding;
    bounds.left = static_cast<LONG>(std::max<int>(static_cast<int>(bounds.left), vx));
    bounds.top = static_cast<LONG>(std::max<int>(static_cast<int>(bounds.top), vy));
    bounds.right = static_cast<LONG>(std::min<int>(static_cast<int>(bounds.right), vx + vw));
    bounds.bottom = static_cast<LONG>(std::min<int>(static_cast<int>(bounds.bottom), vy + vh));
    if (bounds.right - bounds.left < 8 || bounds.bottom - bounds.top < 8)
    {
        Hide();
        return;
    }

    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    RebuildSurface(width, height);
    if (!bitmap_ || !dc_)
        return;

    {
    Gdiplus::Graphics graphics(bitmap_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    const unsigned rgb = context_->accentRgb.load();
    const BYTE r = static_cast<BYTE>((rgb >> 16) & 0xFF);
    const BYTE g = static_cast<BYTE>((rgb >> 8) & 0xFF);
    const BYTE b = static_cast<BYTE>(rgb & 0xFF);
    const float baseWidth = kTrailBaseWidthDip * scale;

    auto local = [&](POINT p)
    {
        return Gdiplus::PointF(
            static_cast<Gdiplus::REAL>(p.x - bounds.left),
            static_cast<Gdiplus::REAL>(p.y - bounds.top));
    };

    std::vector<Gdiplus::PointF> rawPoints;
    rawPoints.reserve(samples_.size());
    for (const Sample& sample : samples_)
        rawPoints.push_back(local(sample.point));

    if (rawPoints.size() >= 2)
    {
        std::vector<Gdiplus::PointF> pathPoints;
        if (rawPoints.size() == 2)
        {
            pathPoints = rawPoints;
        }
        else
        {
            constexpr int kSubdivisions = 4;
            pathPoints.reserve((rawPoints.size() - 1) * kSubdivisions + 1);
            for (size_t i = 0; i + 1 < rawPoints.size(); ++i)
            {
                const Gdiplus::PointF& p0 = rawPoints[i == 0 ? i : i - 1];
                const Gdiplus::PointF& p1 = rawPoints[i];
                const Gdiplus::PointF& p2 = rawPoints[i + 1];
                const Gdiplus::PointF& p3 =
                    rawPoints[(i + 2 < rawPoints.size()) ? i + 2 : i + 1];
                for (int step = 0; step < kSubdivisions; ++step)
                {
                    const float t =
                        static_cast<float>(step) / static_cast<float>(kSubdivisions);
                    const float t2 = t * t;
                    const float t3 = t2 * t;
                    auto spline = [t, t2, t3](float a, float b, float c, float d)
                    {
                        return 0.5f * ((2.0f * b) +
                            (-a + c) * t +
                            (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 +
                            (-a + 3.0f * b - 3.0f * c + d) * t3);
                    };
                    pathPoints.emplace_back(
                        spline(p0.X, p1.X, p2.X, p3.X),
                        spline(p0.Y, p1.Y, p2.Y, p3.Y));
                }
            }
            pathPoints.push_back(rawPoints.back());
        }

        const Gdiplus::PointF tail = pathPoints.front();
        const Gdiplus::PointF head = pathPoints.back();
        const size_t count = pathPoints.size();

        auto buildRibbon = [&](float extraWidth, std::vector<Gdiplus::PointF>& left,
                               std::vector<Gdiplus::PointF>& right)
        {
            left.resize(count);
            right.resize(count);
            for (size_t i = 0; i < count; ++i)
            {
                const float t = count > 1
                    ? static_cast<float>(i) / static_cast<float>(count - 1)
                    : 1.0f;
                const float taper = std::pow(t, 0.85f);
                const float halfWidth =
                    0.5f * (baseWidth * (0.05f + 0.95f * taper) + extraWidth * taper);

                Gdiplus::PointF tangent;
                if (i == 0)
                    tangent = Gdiplus::PointF(pathPoints[1].X - pathPoints[0].X,
                        pathPoints[1].Y - pathPoints[0].Y);
                else if (i + 1 == count)
                    tangent = Gdiplus::PointF(pathPoints[i].X - pathPoints[i - 1].X,
                        pathPoints[i].Y - pathPoints[i - 1].Y);
                else
                    tangent = Gdiplus::PointF(pathPoints[i + 1].X - pathPoints[i - 1].X,
                        pathPoints[i + 1].Y - pathPoints[i - 1].Y);

                const float length = std::sqrt(
                    tangent.X * tangent.X + tangent.Y * tangent.Y);
                if (length < 0.001f)
                {
                    left[i] = pathPoints[i];
                    right[i] = pathPoints[i];
                    continue;
                }
                const float nx = -tangent.Y / length;
                const float ny = tangent.X / length;
                left[i] = Gdiplus::PointF(
                    pathPoints[i].X + nx * halfWidth,
                    pathPoints[i].Y + ny * halfWidth);
                right[i] = Gdiplus::PointF(
                    pathPoints[i].X - nx * halfWidth,
                    pathPoints[i].Y - ny * halfWidth);
            }
        };

        auto drawRibbon = [&](float extraWidth, const Gdiplus::Color& tailColor,
                              const Gdiplus::Color& headColor)
        {
            std::vector<Gdiplus::PointF> left;
            std::vector<Gdiplus::PointF> right;
            buildRibbon(extraWidth, left, right);
            std::vector<Gdiplus::PointF> polygon;
            polygon.reserve(count * 2);
            polygon.insert(polygon.end(), left.begin(), left.end());
            polygon.insert(polygon.end(), right.rbegin(), right.rend());

            Gdiplus::GraphicsPath ribbon;
            ribbon.AddLines(polygon.data(), static_cast<INT>(polygon.size()));
            ribbon.CloseFigure();

            Gdiplus::LinearGradientBrush brush(tail, head, tailColor, headColor);
            graphics.FillPath(&brush, &ribbon);

            // A small rounded head avoids a flat cut at the cursor.
            const float headHalf = 0.5f * (baseWidth + extraWidth);
            Gdiplus::SolidBrush headBrush(headColor);
            graphics.FillEllipse(
                &headBrush,
                head.X - headHalf,
                head.Y - headHalf,
                headHalf * 2.0f,
                headHalf * 2.0f);
        };

        drawRibbon(
            9.0f * scale,
            Gdiplus::Color(0, r, g, b),
            Gdiplus::Color(54, r, g, b));
        drawRibbon(
            0.0f,
            Gdiplus::Color(0, r, g, b),
            Gdiplus::Color(230, r, g, b));
    }

    graphics.Flush();
    }

    HDC screen = GetDC(nullptr);
    if (!screen)
        return;
    POINT destination = {bounds.left, bounds.top};
    SIZE size = {width, height};
    POINT source = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    const BOOL updated = UpdateLayeredWindow(
        hwnd_,
        screen,
        &destination,
        &size,
        dc_,
        &source,
        0,
        &blend,
        ULW_ALPHA);
    ReleaseDC(nullptr, screen);
    (void)updated;
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        destination.x,
        destination.y,
        width,
        height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!visible_)
    {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
    }
}

void TrailOverlay::RebuildSurface(int width, int height)
{
    if (width == surfaceWidth_ && height == surfaceHeight_ && bitmap_ && dc_)
        return;

    delete bitmap_;
    bitmap_ = nullptr;
    if (dc_)
    {
        if (oldBitmap_)
            SelectObject(dc_, oldBitmap_);
        DeleteDC(dc_);
        dc_ = nullptr;
        oldBitmap_ = nullptr;
    }
    if (dib_)
    {
        DeleteObject(dib_);
        dib_ = nullptr;
    }

    HDC screen = GetDC(nullptr);
    if (!screen)
        return;
    dc_ = CreateCompatibleDC(screen);
    if (!dc_)
    {
        ReleaseDC(nullptr, screen);
        return;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    dib_ = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!dib_)
    {
        DeleteDC(dc_);
        dc_ = nullptr;
        return;
    }

    oldBitmap_ = SelectObject(dc_, dib_);
    bitmap_ = new Gdiplus::Bitmap(
        width,
        height,
        width * 4,
        PixelFormat32bppPARGB,
        static_cast<BYTE*>(bits));
    if (!bitmap_ || bitmap_->GetLastStatus() != Gdiplus::Ok)
    {
        delete bitmap_;
        bitmap_ = nullptr;
        if (oldBitmap_)
            SelectObject(dc_, oldBitmap_);
        DeleteDC(dc_);
        dc_ = nullptr;
        DeleteObject(dib_);
        dib_ = nullptr;
        return;
    }
    surfaceWidth_ = width;
    surfaceHeight_ = height;
}

void TrailOverlay::Hide()
{
    if (hwnd_ && visible_)
        ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
}

void TrailOverlay::Cleanup()
{
    delete bitmap_;
    bitmap_ = nullptr;
    if (dc_)
    {
        if (oldBitmap_)
            SelectObject(dc_, oldBitmap_);
        DeleteDC(dc_);
        dc_ = nullptr;
        oldBitmap_ = nullptr;
    }
    if (dib_)
    {
        DeleteObject(dib_);
        dib_ = nullptr;
    }
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (classRegistered_)
    {
        UnregisterClassW(kTrailClass, GetModuleHandleW(nullptr));
        classRegistered_ = false;
    }
    if (gdiplusStarted_)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
        gdiplusStarted_ = false;
    }
}

LRESULT CALLBACK TrailOverlay::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint = {};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
