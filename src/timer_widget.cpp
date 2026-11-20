#include "timer_widget.h"

#include "resource.h"

#include <windowsx.h>
#include <objidl.h>
#include <propidl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
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
constexpr wchar_t kTimerClass[] = L"ZaYuCursorHalo.TimerWidget.P2";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerIntervalMs = 100;

unsigned RgbRed(unsigned rgb) { return (rgb >> 16) & 0xFF; }
unsigned RgbGreen(unsigned rgb) { return (rgb >> 8) & 0xFF; }
unsigned RgbBlue(unsigned rgb) { return rgb & 0xFF; }

float Luminance(unsigned rgb)
{
    return 0.299f * (static_cast<float>(RgbRed(rgb)) / 255.0f) +
        0.587f * (static_cast<float>(RgbGreen(rgb)) / 255.0f) +
        0.114f * (static_cast<float>(RgbBlue(rgb)) / 255.0f);
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius)
{
    const float diameter = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(
        rect.GetRight() - diameter,
        rect.GetBottom() - diameter,
        diameter,
        diameter,
        0.0f,
        90.0f);
    path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

Gdiplus::Color ColorOf(unsigned rgb, BYTE alpha = 255)
{
    return Gdiplus::Color(
        alpha,
        static_cast<BYTE>(RgbRed(rgb)),
        static_cast<BYTE>(RgbGreen(rgb)),
        static_cast<BYTE>(RgbBlue(rgb)));
}
} // namespace

struct TimerWidget::TimerRender
{
    std::vector<BYTE> fontData;
    Gdiplus::PrivateFontCollection fonts;
    Gdiplus::FontFamily* families = nullptr;
    int familyCount = 0;
    Gdiplus::FontFamily* family = nullptr;
    Gdiplus::Font* font = nullptr;
    Gdiplus::Font* segmentFont = nullptr;
    int segmentFontPx = 0;
    HANDLE gdiFontResource = nullptr;
    HFONT measureFont = nullptr;
    int measureFontPx = 0;
    int fontPx = 0;
    Gdiplus::Bitmap* bitmap = nullptr;
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    BYTE* bits = nullptr;
    int surfaceWidth = 0;
    int surfaceHeight = 0;

    ~TimerRender()
    {
        if (bitmap)
        {
            delete bitmap;
            bitmap = nullptr;
        }
        if (dc)
        {
            if (oldBitmap)
                SelectObject(dc, oldBitmap);
            DeleteDC(dc);
            dc = nullptr;
        }
        if (dib)
        {
            DeleteObject(dib);
            dib = nullptr;
        }
        bits = nullptr;
        if (font)
        {
            delete font;
            font = nullptr;
        }
        if (segmentFont)
        {
            delete segmentFont;
            segmentFont = nullptr;
        }
        if (families)
        {
            delete[] families;
            families = nullptr;
            family = nullptr;
            familyCount = 0;
        }
        if (measureFont)
        {
            DeleteObject(measureFont);
            measureFont = nullptr;
        }
        if (gdiFontResource)
        {
            RemoveFontMemResourceEx(gdiFontResource);
            gdiFontResource = nullptr;
        }
    }

    void ReleaseSurface()
    {
        if (bitmap)
        {
            delete bitmap;
            bitmap = nullptr;
        }
        if (dc)
        {
            if (oldBitmap)
                SelectObject(dc, oldBitmap);
            DeleteDC(dc);
            dc = nullptr;
            oldBitmap = nullptr;
        }
        if (dib)
        {
            DeleteObject(dib);
            dib = nullptr;
        }
        bits = nullptr;
        surfaceWidth = 0;
        surfaceHeight = 0;
    }
};

TimerWidget::TimerWidget() = default;

TimerWidget::~TimerWidget()
{
    Destroy();
}

bool TimerWidget::Create(HINSTANCE instance)
{
    if (hwnd_)
        return true;
    instance_ = instance;

    if (!gdiplusStarted_)
    {
        Gdiplus::GdiplusStartupInput input;
        gdiplusStarted_ =
            Gdiplus::GdiplusStartup(&gdiplusToken_, &input, nullptr) == Gdiplus::Ok;
    }
    if (!render_)
        render_ = std::make_unique<TimerRender>();
    LoadFont();

    if (!classRegistered_)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &TimerWidget::WndProc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kTimerClass;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        classRegistered_ = true;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kTimerClass,
        L"",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance_,
        this);
    if (!hwnd_)
        return false;

    ShowWindow(hwnd_, SW_HIDE);
    if (running_)
    {
        lastDisplayedSecond_ = static_cast<ULONGLONG>(-1);
        SetTimer(hwnd_, kTimerId, kTimerIntervalMs, nullptr);
    }
    return true;
}

void TimerWidget::Destroy()
{
    if (hwnd_)
    {
        KillTimer(hwnd_, kTimerId);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    visible_ = false;
    running_ = false;
    if (render_)
        render_.reset();
    if (gdiplusStarted_)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusStarted_ = false;
        gdiplusToken_ = 0;
    }
    if (classRegistered_)
    {
        UnregisterClassW(kTimerClass, instance_);
        classRegistered_ = false;
    }
}

bool TimerWidget::LoadFont()
{
    if (!render_ || render_->family)
        return true;

    HRSRC resource = FindResourceW(instance_, MAKEINTRESOURCEW(IDR_TIMER_FONT), RT_RCDATA);
    if (resource)
    {
        HGLOBAL loaded = LoadResource(instance_, resource);
        const DWORD size = SizeofResource(instance_, resource);
        const void* data = loaded ? LockResource(loaded) : nullptr;
        if (data && size)
        {
            render_->fontData.assign(
                static_cast<const BYTE*>(data),
                static_cast<const BYTE*>(data) + size);
            DWORD gdiFontCount = 0;
            render_->gdiFontResource = AddFontMemResourceEx(
                render_->fontData.data(),
                size,
                nullptr,
                &gdiFontCount);
            if (render_->fonts.AddMemoryFont(render_->fontData.data(), static_cast<INT>(size)) ==
                Gdiplus::Ok)
            {
                const INT count = render_->fonts.GetFamilyCount();
                if (count > 0)
                {
                    render_->families = new Gdiplus::FontFamily[count];
                    INT found = 0;
                    if (render_->fonts.GetFamilies(count, render_->families, &found) ==
                            Gdiplus::Ok &&
                        found > 0)
                    {
                        render_->familyCount = found;
                        render_->family = &render_->families[0];
                    }
                    else
                    {
                        delete[] render_->families;
                        render_->families = nullptr;
                    }
                }
            }
        }
    }
    return render_->family != nullptr;
}

void TimerWidget::EnsureFont(int pixelHeight)
{
    if (!render_)
        return;
    pixelHeight = (std::max)(12, pixelHeight);
    if (render_->font && render_->fontPx == pixelHeight)
        return;
    if (render_->font)
    {
        delete render_->font;
        render_->font = nullptr;
    }
    render_->fontPx = pixelHeight;
    if (render_->family)
        render_->font = new Gdiplus::Font(
            render_->family,
            static_cast<Gdiplus::REAL>(pixelHeight),
            Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
    if (!render_->font || render_->font->GetLastStatus() != Gdiplus::Ok)
    {
        delete render_->font;
        render_->font = new Gdiplus::Font(
            L"Consolas",
            static_cast<Gdiplus::REAL>(pixelHeight),
            Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
    }
}

void TimerWidget::EnsureSegmentFont(int pixelHeight)
{
    if (!render_ || pixelHeight <= 0)
        return;
    if (render_->segmentFont && render_->segmentFontPx == pixelHeight)
        return;
    delete render_->segmentFont;
    render_->segmentFont = new Gdiplus::Font(
        L"Segoe UI",
        static_cast<Gdiplus::REAL>(pixelHeight),
        Gdiplus::FontStyleRegular,
        Gdiplus::UnitPixel);
    if (!render_->segmentFont || render_->segmentFont->GetLastStatus() != Gdiplus::Ok)
    {
        delete render_->segmentFont;
        render_->segmentFont = new Gdiplus::Font(
            L"Tahoma",
            static_cast<Gdiplus::REAL>(pixelHeight),
            Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
    }
    render_->segmentFontPx = pixelHeight;
}

void TimerWidget::RebuildLayout()
{
    if (!hwnd_ || !render_)
        return;

    const int oldWidth = width_;
    const int oldHeight = height_;
    const POINT anchor = positionSet_
        ? POINT{x_ + (std::max)(width_, 1) / 2, y_ + (std::max)(height_, 1) / 2}
        : POINT{0, 0};
    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    scaleDpi_ = DpiForMonitor(monitor);
    scale_ = static_cast<float>(scaleDpi_) / 96.0f;

    const int capsuleHeight = (std::max)(24, static_cast<int>(std::lround(36.0f * scale_)));
    const int margin = (std::max)(2, static_cast<int>(std::lround(2.0f * scale_)));
    const int padding = (std::max)(4, static_cast<int>(std::lround(7.0f * scale_)));
    const int gripWidth = (std::max)(8, static_cast<int>(std::lround(10.0f * scale_)));
    const int gripGap = (std::max)(3, static_cast<int>(std::lround(4.0f * scale_)));
    const int buttonSize = (std::max)(18, (std::min)(
        static_cast<int>(std::lround(26.0f * scale_)),
        capsuleHeight - static_cast<int>(std::lround(8.0f * scale_))));
    const int buttonGap = (std::max)(3, static_cast<int>(std::lround(4.0f * scale_)));
    const int textGap = (std::max)(4, static_cast<int>(std::lround(7.0f * scale_)));
    const int fontPx = (std::max)(13, static_cast<int>(std::lround(capsuleHeight * 0.46f)));
    EnsureFont(fontPx);

    if (!render_->measureFont || render_->measureFontPx != fontPx)
    {
        if (render_->measureFont)
            DeleteObject(render_->measureFont);
        render_->measureFont = CreateFontW(
            -fontPx,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"DSEG7 Modern");
        render_->measureFontPx = fontPx;
    }

    HDC screen = GetDC(nullptr);
    HGDIOBJ oldFont = render_->measureFont
        ? SelectObject(screen, render_->measureFont)
        : nullptr;
    int widths[10] = {};
    digitWidth_ = 0;
    if (render_->measureFont &&
        GetCharWidth32W(screen, L'0', L'9', widths))
    {
        for (int width : widths)
            digitWidth_ = (std::max)(digitWidth_, width);
    }
    SIZE colonSize = {};
    if (render_->measureFont)
        GetTextExtentPoint32W(screen, L":", 1, &colonSize);
    if (oldFont)
        SelectObject(screen, oldFont);
    ReleaseDC(nullptr, screen);
    if (digitWidth_ <= 0)
        digitWidth_ = (std::max)(1, static_cast<int>(std::lround(fontPx * 0.45f)));
    colonWidth_ = (std::max)(3, static_cast<int>(colonSize.cx));
    if (colonSize.cx <= 0)
        colonWidth_ = (std::max)(3, static_cast<int>(std::lround(fontPx * 0.18f)));

    const int textWidth = digitWidth_ * (showHours_ ? 6 : 4) +
        colonWidth_ * (showHours_ ? 2 : 1);
    const int segmentGap = marks_.empty() ? 0 : (std::max)(4, static_cast<int>(std::lround(7.0f * scale_)));
    const int segmentWidth = marks_.empty() ? 0 : (std::max)(44, static_cast<int>(std::lround(74.0f * scale_)));
    const int contentWidth = padding * 2 + gripWidth + gripGap +
        buttonSize * 3 + buttonGap * 2 + textGap + textWidth +
        segmentGap + segmentWidth;

    width_ = contentWidth + margin * 2;
    height_ = capsuleHeight + margin * 2;
    if (positionSet_ && oldWidth > 0 && oldHeight > 0 &&
        (width_ != oldWidth || height_ != oldHeight))
    {
        // Keep the visual center stable when the capsule changes size (font
        // metrics, hour field, segment text, DPI). Otherwise the saved
        // top-left anchor makes the timer appear to jump right/up.
        x_ -= (width_ - oldWidth) / 2;
        y_ -= (height_ - oldHeight) / 2;
    }

    capsuleRect_ = {margin, margin, margin + contentWidth, margin + capsuleHeight};
    int left = margin + padding;
    gripRect_ = {left, margin + (capsuleHeight - buttonSize) / 2, left + gripWidth, margin + (capsuleHeight + buttonSize) / 2};
    left += gripWidth + gripGap;
    playRect_ = {left, margin + (capsuleHeight - buttonSize) / 2, left + buttonSize, margin + (capsuleHeight + buttonSize) / 2};
    left += buttonSize + buttonGap;
    stopRect_ = {left, playRect_.top, left + buttonSize, playRect_.bottom};
    left += buttonSize + buttonGap;
    markRect_ = {left, playRect_.top, left + buttonSize, playRect_.bottom};
    left += buttonSize + textGap;
    timeRect_ = {left, margin, left + textWidth, margin + capsuleHeight};
    left = timeRect_.right + segmentGap;
    segmentRect_ = {left, margin, left + segmentWidth, margin + capsuleHeight};

    const int segmentFontPx = (std::max)(9, static_cast<int>(std::lround(fontPx * 0.36f)));
    EnsureSegmentFont(segmentFontPx);

    if (render_->surfaceWidth != width_ || render_->surfaceHeight != height_ || !render_->dib)
    {
        render_->ReleaseSurface();
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width_;
        info.bmiHeader.biHeight = -height_;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        HDC screenDc = GetDC(nullptr);
        render_->dib = CreateDIBSection(
            screenDc,
            &info,
            DIB_RGB_COLORS,
            reinterpret_cast<void**>(&render_->bits),
            nullptr,
            0);
        render_->dc = CreateCompatibleDC(screenDc);
        ReleaseDC(nullptr, screenDc);
        if (render_->dc && render_->dib)
        {
            render_->oldBitmap = SelectObject(render_->dc, render_->dib);
            render_->bitmap = new Gdiplus::Bitmap(
                width_,
                height_,
                width_ * 4,
                PixelFormat32bppPARGB,
                render_->bits);
            render_->surfaceWidth = width_;
            render_->surfaceHeight = height_;
        }
        else
        {
            render_->ReleaseSurface();
        }
    }

    if (!positionSet_)
        DefaultPosition();
    else
        ClampToWorkArea(nullptr);

    if (render_->dc && render_->dib)
        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            x_,
            y_,
            width_,
            height_,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TimerWidget::DefaultPosition()
{
    HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        x_ = 24;
        y_ = 24;
        return;
    }
    const int margin = static_cast<int>(std::lround(24.0f * scale_));
    x_ = info.rcWork.right - width_ - margin;
    y_ = info.rcWork.bottom - height_ - margin;
    if (x_ < info.rcWork.left)
        x_ = info.rcWork.left;
    if (y_ < info.rcWork.top)
        y_ = info.rcWork.top;
}

void TimerWidget::ClampToWorkArea(const POINT* cursor)
{
    POINT anchor = {};
    if (cursor)
        anchor = *cursor;
    else
        anchor = POINT{x_ + width_ / 2, y_ + height_ / 2};
    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return;
    const int minX = static_cast<int>(info.rcWork.left);
    const int minY = static_cast<int>(info.rcWork.top);
    const int maxX = (std::max)(minX, static_cast<int>(info.rcWork.right) - width_);
    const int maxY = (std::max)(minY, static_cast<int>(info.rcWork.bottom) - height_);
    x_ = (std::max)(minX, (std::min)(x_, maxX));
    y_ = (std::max)(minY, (std::min)(y_, maxY));
}

void TimerWidget::ApplySettings(
    bool enabled,
    bool showHours,
    bool resetOnStop,
    TimerMode mode,
    int countdownMinutes,
    unsigned accentRgb,
    bool darkMode,
    int x,
    int y,
    bool hasPosition)
{
    const int clampedCountdown = ClampCountdownMinutes(countdownMinutes);
    const bool modeChanged = mode_ != mode || countdownMinutes_ != clampedCountdown;
    if (modeChanged && running_)
    {
        Stop();
        accumulatedMs_ = 0;
    }
    enabled_ = enabled;
    showHours_ = showHours;
    resetOnStop_ = resetOnStop;
    mode_ = mode;
    countdownMinutes_ = clampedCountdown;
    countdownFinished_ = false;
    accentRgb_ = accentRgb;
    darkMode_ = darkMode;
    if (hasPosition)
    {
        x_ = x;
        y_ = y;
        positionSet_ = true;
    }

    if (!enabled_)
    {
        if (running_)
            Stop();
        if (hwnd_)
            ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
        return;
    }
    if (!hwnd_)
        return;

    RebuildLayout();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;
    Paint();
}

void TimerWidget::Start()
{
    if (!enabled_ || running_ || !hwnd_)
        return;
    if (!visible_)
    {
        RebuildLayout();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
    }
    if (mode_ == TimerMode::CountDown &&
        (countdownFinished_ ||
         accumulatedMs_ >= static_cast<ULONGLONG>(countdownMinutes_) * 60000ULL))
    {
        accumulatedMs_ = 0;
        countdownFinished_ = false;
        marks_.clear();
        lastSegmentMs_ = 0;
        RebuildLayout();
    }
    startedAt_ = GetTickCount64();
    running_ = true;
    lastDisplayedSecond_ = static_cast<ULONGLONG>(-1);
    SetTimer(hwnd_, kTimerId, kTimerIntervalMs, nullptr);
    Paint();
}

void TimerWidget::Stop()
{
    if (running_)
    {
        accumulatedMs_ += GetTickCount64() - startedAt_;
        running_ = false;
    }
    if (hwnd_)
        KillTimer(hwnd_, kTimerId);
    if (mode_ == TimerMode::CountDown &&
        accumulatedMs_ >= static_cast<ULONGLONG>(countdownMinutes_) * 60000ULL)
    {
        countdownFinished_ = true;
        accumulatedMs_ = static_cast<ULONGLONG>(countdownMinutes_) * 60000ULL;
    }
    if (resetOnStop_)
        accumulatedMs_ = 0;
    if (hwnd_ && visible_)
        Paint();
}

void TimerWidget::Reset()
{
    if (hwnd_)
        KillTimer(hwnd_, kTimerId);
    running_ = false;
    accumulatedMs_ = 0;
    countdownFinished_ = false;
    marks_.clear();
    lastSegmentMs_ = 0;
    lastDisplayedSecond_ = static_cast<ULONGLONG>(-1);
    if (hwnd_)
        RebuildLayout();
    if (hwnd_ && visible_)
        Paint();
}

void TimerWidget::MarkSegment()
{
    if (mode_ != TimerMode::CountUp || !running_)
        return;
    const ULONGLONG now = ElapsedMs();
    const ULONGLONG previous = marks_.empty() ? 0 : marks_.back();
    lastSegmentMs_ = now - previous;
    marks_.push_back(now);
    RebuildLayout();
    Paint();
}

void TimerWidget::SetMode(TimerMode mode, int countdownMinutes)
{
    const int minutes = ClampCountdownMinutes(countdownMinutes);
    if (mode_ == mode && countdownMinutes_ == minutes)
        return;
    if (running_)
        Stop();
    mode_ = mode;
    countdownMinutes_ = minutes;
    accumulatedMs_ = 0;
    countdownFinished_ = false;
    marks_.clear();
    lastSegmentMs_ = 0;
    lastDisplayedSecond_ = static_cast<ULONGLONG>(-1);
    RebuildLayout();
    if (hwnd_ && visible_)
        Paint();
    if (host_)
        host_->OnTimerSettingsChanged(mode_, countdownMinutes_);
}

void TimerWidget::ShowModeMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, 1, L"正计时");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const int presets[] = {5, 10, 15, 25, 45, 60};
    for (int i = 0; i < static_cast<int>(sizeof(presets) / sizeof(presets[0])); ++i)
    {
        wchar_t label[48] = {};
        swprintf_s(label, L"倒计时 %d 分钟", presets[i]);
        AppendMenuW(menu, MF_STRING, 2 + i, label);
    }
    AppendMenuW(menu, MF_STRING, 12, L"自定义…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 10, L"标记分段");
    AppendMenuW(menu, MF_STRING, 11, L"重置计时");

    CheckMenuItem(menu, 1, MF_BYCOMMAND | (mode_ == TimerMode::CountUp ? MF_CHECKED : MF_UNCHECKED));
    bool presetChecked = false;
    for (int i = 0; i < static_cast<int>(sizeof(presets) / sizeof(presets[0])); ++i)
    {
        const bool checked = mode_ == TimerMode::CountDown && countdownMinutes_ == presets[i];
        presetChecked = presetChecked || checked;
        CheckMenuItem(menu, 2 + i, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    }
    CheckMenuItem(
        menu,
        12,
        MF_BYCOMMAND |
            (mode_ == TimerMode::CountDown && !presetChecked ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(
        menu,
        10,
        MF_BYCOMMAND | (mode_ == TimerMode::CountUp && running_ ? MF_ENABLED : MF_GRAYED));

    POINT cursor = {};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd_);
    const UINT command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        cursor.x,
        cursor.y,
        0,
        hwnd_,
        nullptr);
    DestroyMenu(menu);

    if (command == 1)
    {
        SetMode(TimerMode::CountUp, 5);
    }
    else if (command >= 2 && command < 2 + static_cast<UINT>(sizeof(presets) / sizeof(presets[0])))
    {
        SetMode(TimerMode::CountDown, presets[command - 2]);
    }
    else if (command == 12)
    {
        ShowCustomCountdownDialog();
    }
    else if (command == 10)
    {
        MarkSegment();
    }
    else if (command == 11)
    {
        Reset();
    }
}

void TimerWidget::Refresh()
{
    if (!enabled_ || !hwnd_)
        return;
    RebuildLayout();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;
    Paint();
}

ULONGLONG TimerWidget::ElapsedMs() const
{
    if (running_)
        return accumulatedMs_ + (GetTickCount64() - startedAt_);
    return accumulatedMs_;
}

ULONGLONG TimerWidget::DisplayMs() const
{
    const ULONGLONG elapsed = ElapsedMs();
    if (mode_ == TimerMode::CountUp)
        return elapsed;
    const ULONGLONG total = static_cast<ULONGLONG>(countdownMinutes_) * 60000ULL;
    return elapsed >= total ? 0 : total - elapsed;
}

std::wstring TimerWidget::FormatElapsed() const
{
    ULONGLONG seconds = DisplayMs() / 1000;
    const ULONGLONG hours = seconds / 3600;
    seconds %= 3600;
    const ULONGLONG minutes = seconds / 60;
    seconds %= 60;
    const auto pad2 = [](int value) {
        std::wstring text = std::to_wstring(value);
        if (text.size() < 2)
            text.insert(text.begin(), L'0');
        return text;
    };
    if (showHours_)
    {
        return pad2(static_cast<int>((std::min)(hours, 99ULL))) + L":" +
            pad2(static_cast<int>(minutes)) + L":" +
            pad2(static_cast<int>(seconds));
    }
    return pad2(static_cast<int>((std::min)(hours * 60 + minutes, 99ULL))) + L":" +
        pad2(static_cast<int>(seconds));
}

std::wstring TimerWidget::FormatSegment() const
{
    if (marks_.empty())
        return std::wstring();
    ULONGLONG seconds = lastSegmentMs_ / 1000;
    const ULONGLONG hours = seconds / 3600;
    seconds %= 3600;
    const ULONGLONG minutes = seconds / 60;
    seconds %= 60;
    wchar_t buffer[48] = {};
    if (hours > 0)
    {
        swprintf_s(
            buffer,
            L"S%zu %02llu:%02llu:%02llu",
            marks_.size(),
            hours,
            minutes,
            seconds);
    }
    else
    {
        swprintf_s(
            buffer,
            L"S%zu %02llu:%02llu",
            marks_.size(),
            minutes,
            seconds);
    }
    return buffer;
}

bool TimerWidget::IsReadable(unsigned color, unsigned background) const
{
    return std::fabs(Luminance(color) - Luminance(background)) > 0.28f;
}

void TimerWidget::Paint()
{
    if (!render_ || !render_->bitmap || !render_->font || width_ <= 0 || height_ <= 0)
        return;

    const bool light = !darkMode_;
    const unsigned backgroundRgb = light ? 0xF2F3F5 : 0x202226;
    const unsigned borderRgb = light ? 0xD9DCE2 : 0x3A3E46;
    const unsigned textRgb = light ? 0x1B1C1F : 0xF4F6FA;
    const unsigned mutedRgb = light ? 0x8A8F98 : 0x8B919A;
    const unsigned hoverRgb = light ? 0xE2E5EA : 0x2D3139;
    const float stroke = (std::max)(1.0f, scale_);

    Gdiplus::Graphics graphics(render_->bitmap);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    const Gdiplus::RectF capsule(
        static_cast<Gdiplus::REAL>(capsuleRect_.left),
        static_cast<Gdiplus::REAL>(capsuleRect_.top),
        static_cast<Gdiplus::REAL>(capsuleRect_.right - capsuleRect_.left),
        static_cast<Gdiplus::REAL>(capsuleRect_.bottom - capsuleRect_.top));
    Gdiplus::GraphicsPath capsulePath;
    AddRoundedRect(capsulePath, capsule, capsule.Height / 2.0f);
    Gdiplus::SolidBrush background(ColorOf(backgroundRgb));
    graphics.FillPath(&background, &capsulePath);
    Gdiplus::Pen border(ColorOf(borderRgb), stroke);
    graphics.DrawPath(&border, &capsulePath);

    if (hoverGrip_)
    {
        Gdiplus::SolidBrush hover(ColorOf(hoverRgb));
        Gdiplus::GraphicsPath hoverPath;
        AddRoundedRect(
            hoverPath,
            Gdiplus::RectF(
                static_cast<Gdiplus::REAL>(gripRect_.left - 2.0f * scale_),
                static_cast<Gdiplus::REAL>(gripRect_.top - 2.0f * scale_),
                static_cast<Gdiplus::REAL>(gripRect_.right - gripRect_.left + 4.0f * scale_),
                static_cast<Gdiplus::REAL>(gripRect_.bottom - gripRect_.top + 4.0f * scale_)),
            6.0f * scale_);
        graphics.FillPath(&hover, &hoverPath);
    }

    Gdiplus::SolidBrush muted(ColorOf(mutedRgb));
    const float gripCenterX = (gripRect_.left + gripRect_.right) / 2.0f;
    const float gripCenterY = (gripRect_.top + gripRect_.bottom) / 2.0f;
    for (int row = -1; row <= 1; ++row)
    {
        for (int column = 0; column < 2; ++column)
        {
            const float dotX = gripCenterX + (column == 0 ? -2.4f : 2.4f) * scale_;
            const float dotY = gripCenterY + static_cast<float>(row) * 3.4f * scale_;
            graphics.FillEllipse(
                &muted,
                Gdiplus::RectF(
                    dotX - 1.15f * scale_,
                    dotY - 1.15f * scale_,
                    2.3f * scale_,
                    2.3f * scale_));
        }
    }

    auto drawButton = [&](const RECT& rect, bool play, bool hover, bool active) {
        const Gdiplus::RectF button(
            static_cast<Gdiplus::REAL>(rect.left),
            static_cast<Gdiplus::REAL>(rect.top),
            static_cast<Gdiplus::REAL>(rect.right - rect.left),
            static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
        if (hover)
        {
            Gdiplus::SolidBrush hoverBrush(ColorOf(hoverRgb));
            Gdiplus::GraphicsPath hoverPath;
            AddRoundedRect(
                hoverPath,
                Gdiplus::RectF(button.X + 1.0f, button.Y + 1.0f, button.Width - 2.0f, button.Height - 2.0f),
                (button.Height - 2.0f) / 2.0f);
            graphics.FillPath(&hoverBrush, &hoverPath);
        }
        const unsigned iconRgb = active ? textRgb : mutedRgb;
        Gdiplus::SolidBrush icon(ColorOf(iconRgb));
        if (play)
        {
            const Gdiplus::REAL left = button.X + button.Width * 0.29f;
            const Gdiplus::REAL right = button.GetRight() - button.Width * 0.24f;
            const Gdiplus::REAL top = button.Y + button.Height * 0.24f;
            const Gdiplus::REAL bottom = button.GetBottom() - button.Height * 0.24f;
            Gdiplus::PointF points[3] = {
                Gdiplus::PointF(left, top),
                Gdiplus::PointF(left, bottom),
                Gdiplus::PointF(right, (top + bottom) / 2.0f),
            };
            graphics.FillPolygon(&icon, points, 3);
        }
        else
        {
            const Gdiplus::REAL pad = button.Width * 0.22f;
            Gdiplus::GraphicsPath path;
            AddRoundedRect(
                path,
                Gdiplus::RectF(
                    button.X + pad,
                    button.Y + pad,
                    button.Width - pad * 2.0f,
                    button.Height - pad * 2.0f),
                (button.Width - pad * 2.0f) * 0.13f);
            graphics.FillPath(&icon, &path);
        }
    };

    drawButton(playRect_, true, hoverPlay_, !running_);
    drawButton(stopRect_, false, hoverStop_, running_);

    {
        const bool enabled = mode_ == TimerMode::CountUp && running_;
        if (hoverMark_ && enabled)
        {
            Gdiplus::SolidBrush hoverBrush(ColorOf(hoverRgb));
            Gdiplus::GraphicsPath hoverPath;
            AddRoundedRect(
                hoverPath,
                Gdiplus::RectF(
                    static_cast<Gdiplus::REAL>(markRect_.left + 1),
                    static_cast<Gdiplus::REAL>(markRect_.top + 1),
                    static_cast<Gdiplus::REAL>(markRect_.right - markRect_.left - 2),
                    static_cast<Gdiplus::REAL>(markRect_.bottom - markRect_.top - 2)),
                (std::max)(1.0f, static_cast<Gdiplus::REAL>(markRect_.bottom - markRect_.top - 2) / 2.0f));
            graphics.FillPath(&hoverBrush, &hoverPath);
        }
        Gdiplus::SolidBrush icon(ColorOf(enabled ? textRgb : mutedRgb));
        const float poleX = static_cast<float>(markRect_.left) + (markRect_.right - markRect_.left) * 0.38f;
        const float top = static_cast<float>(markRect_.top) + (markRect_.bottom - markRect_.top) * 0.25f;
        const float bottom = static_cast<float>(markRect_.bottom) - (markRect_.bottom - markRect_.top) * 0.25f;
        graphics.FillRectangle(&icon, poleX, top, (std::max)(1.0f, 1.6f * scale_), bottom - top);
        Gdiplus::PointF flag[3] = {
            Gdiplus::PointF(poleX, top),
            Gdiplus::PointF(poleX + (markRect_.right - markRect_.left) * 0.34f, top + 2.0f * scale_),
            Gdiplus::PointF(poleX, top + (bottom - top) * 0.48f),
        };
        graphics.FillPolygon(&icon, flag, 3);
    }

    const std::wstring elapsed = FormatElapsed();
    int gridWidth = 0;
    for (wchar_t glyph : elapsed)
        gridWidth += glyph == L':' ? colonWidth_ : digitWidth_;
    Gdiplus::StringFormat glyphFormat;
    glyphFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);
    glyphFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
    glyphFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    const unsigned timeRgb = running_ && IsReadable(accentRgb_, backgroundRgb)
        ? accentRgb_
        : textRgb;
    Gdiplus::SolidBrush timeBrush(ColorOf(timeRgb));
    // DSEG7 has a slightly different optical center from the old Digital-7
    // face; nudge the glyph grid left/down by a scaled pixel or two so the
    // digits sit centered in the capsule instead of looking high-right.
    const int timeNudgeX = (std::max)(1, static_cast<int>(std::lround(2.0f * scale_)));
    const int timeNudgeY = (std::max)(1, static_cast<int>(std::lround(1.0f * scale_)));
    int glyphLeft = timeRect_.left + (timeRect_.right - timeRect_.left - gridWidth) / 2 - timeNudgeX;
    for (wchar_t glyph : elapsed)
    {
        const int cellWidth = glyph == L':' ? colonWidth_ : digitWidth_;
        Gdiplus::RectF cell(
            static_cast<Gdiplus::REAL>(glyphLeft),
            static_cast<Gdiplus::REAL>(timeRect_.top + timeNudgeY),
            static_cast<Gdiplus::REAL>(cellWidth),
            static_cast<Gdiplus::REAL>(timeRect_.bottom - timeRect_.top));
        graphics.DrawString(&glyph, 1, render_->font, cell, &glyphFormat, &timeBrush);
        glyphLeft += cellWidth;
    }

    if (!marks_.empty() && render_->segmentFont)
    {
        const std::wstring segment = FormatSegment();
        Gdiplus::StringFormat segmentFormat;
        segmentFormat.SetFormatFlags(
            Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);
        segmentFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        segmentFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::SolidBrush segmentBrush(ColorOf(mutedRgb));
        Gdiplus::RectF segmentRect(
            static_cast<Gdiplus::REAL>(segmentRect_.left),
            static_cast<Gdiplus::REAL>(segmentRect_.top),
            static_cast<Gdiplus::REAL>(segmentRect_.right - segmentRect_.left),
            static_cast<Gdiplus::REAL>(segmentRect_.bottom - segmentRect_.top));
        graphics.DrawString(
            segment.c_str(),
            static_cast<INT>(segment.size()),
            render_->segmentFont,
            segmentRect,
            &segmentFormat,
            &segmentBrush);
    }

    Present();
}

void TimerWidget::Present()
{
    if (!hwnd_ || !render_ || !render_->dc || !render_->dib || !enabled_)
        return;

    HDC screen = GetDC(nullptr);
    if (!screen)
        return;
    POINT destination = {x_, y_};
    SIZE size = {width_, height_};
    POINT source = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(
        hwnd_,
        screen,
        &destination,
        &size,
        render_->dc,
        &source,
        0,
        &blend,
        ULW_ALPHA);
    ReleaseDC(nullptr, screen);
    if (!visible_)
    {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
    }
}

void TimerWidget::OnTick()
{
    if (!visible_ || !running_)
        return;
    if (mode_ == TimerMode::CountDown &&
        ElapsedMs() >= static_cast<ULONGLONG>(countdownMinutes_) * 60000ULL)
    {
        Stop();
        countdownFinished_ = true;
        Paint();
        return;
    }
    const ULONGLONG second = DisplayMs() / 1000;
    if (second != lastDisplayedSecond_)
    {
        lastDisplayedSecond_ = second;
        Paint();
    }
}

void TimerWidget::UpdateHover(int x, int y)
{
    const auto inside = [x, y](const RECT& rect) {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    };
    const bool play = inside(playRect_);
    const bool stop = inside(stopRect_);
    const bool mark = inside(markRect_);
    const bool grip = inside(gripRect_);
    if (play != hoverPlay_ || stop != hoverStop_ || mark != hoverMark_ || grip != hoverGrip_)
    {
        hoverPlay_ = play;
        hoverStop_ = stop;
        hoverMark_ = mark;
        hoverGrip_ = grip;
        Paint();
    }
    if (!trackingLeave_)
    {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd_;
        TrackMouseEvent(&track);
        trackingLeave_ = true;
    }
}

void TimerWidget::BeginDrag()
{
    if (dragging_)
        return;
    dragging_ = true;
    hoverPlay_ = false;
    hoverStop_ = false;
    hoverGrip_ = true;
    GetCursorPos(&dragCursor_);
    dragWindow_ = POINT{x_, y_};
    SetCapture(hwnd_);
    Paint();
}

void TimerWidget::UpdateDrag()
{
    if (!dragging_)
        return;
    POINT cursor = {};
    GetCursorPos(&cursor);
    x_ = dragWindow_.x + (cursor.x - dragCursor_.x);
    y_ = dragWindow_.y + (cursor.y - dragCursor_.y);
    ClampToWorkArea(&cursor);
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        x_,
        y_,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE);
}

void TimerWidget::EndDrag(bool commit)
{
    if (!dragging_)
        return;
    dragging_ = false;
    if (GetCapture() == hwnd_)
        ReleaseCapture();
    positionSet_ = true;
    hoverGrip_ = true;

    HMONITOR monitor = MonitorFromPoint(POINT{x_ + width_ / 2, y_ + height_ / 2}, MONITOR_DEFAULTTONEAREST);
    const UINT dpi = DpiForMonitor(monitor);
    if (dpi != static_cast<UINT>(scaleDpi_))
        RebuildLayout();

    if (commit && host_)
        host_->OnTimerPositionChanged(x_, y_);
    Paint();
}

UINT TimerWidget::SystemDpi()
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

UINT TimerWidget::DpiForMonitor(HMONITOR monitor)
{
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static HMODULE shcore = LoadLibraryW(L"shcore.dll");
    static GetDpiForMonitorFn getDpiForMonitor = shcore
        ? reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"))
        : nullptr;
    if (getDpiForMonitor && monitor)
    {
        UINT dpiX = 0;
        UINT dpiY = 0;
        if (SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX >= 72 && dpiX <= 480)
            return dpiX;
    }
    return SystemDpi();
}

void TimerWidget::ShowCustomCountdownDialog()
{
    if (!hwnd_ || !instance_)
        return;
    pendingCustomMinutes_ = 0;
    const INT_PTR result = DialogBoxParamW(
        instance_,
        MAKEINTRESOURCEW(IDD_COUNTDOWN),
        hwnd_,
        CountdownDialogProc,
        reinterpret_cast<LPARAM>(this));
    if (result == IDOK && pendingCustomMinutes_ > 0)
        SetMode(TimerMode::CountDown, pendingCustomMinutes_);
}

INT_PTR CALLBACK TimerWidget::CountdownDialogProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    TimerWidget* self = reinterpret_cast<TimerWidget*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message)
    {
    case WM_INITDIALOG:
    {
        self = reinterpret_cast<TimerWidget*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        SetWindowTextW(hwnd, L"自定义倒计时");
        SetDlgItemTextW(hwnd, IDC_COUNTDOWN_LABEL, L"分钟（1–99）");
        SetDlgItemTextW(hwnd, IDOK, L"确定");
        SetDlgItemTextW(hwnd, IDCANCEL, L"取消");
        wchar_t minutes[16] = {};
        swprintf_s(minutes, L"%d", self ? self->countdownMinutes_ : 5);
        SetDlgItemTextW(hwnd, IDC_COUNTDOWN_EDIT, minutes);
        SendDlgItemMessageW(hwnd, IDC_COUNTDOWN_EDIT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(hwnd, IDC_COUNTDOWN_EDIT));

        RECT owner = {};
        RECT dialog = {};
        if (self && self->hwnd_ && GetWindowRect(self->hwnd_, &owner) &&
            GetWindowRect(hwnd, &dialog))
        {
            const int x = owner.left + (owner.right - owner.left - (dialog.right - dialog.left)) / 2;
            const int y = owner.top - (dialog.bottom - dialog.top) - 8;
            SetWindowPos(
                hwnd,
                HWND_TOPMOST,
                x,
                y,
                0,
                0,
                SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            BOOL valid = FALSE;
            const UINT value = GetDlgItemInt(hwnd, IDC_COUNTDOWN_EDIT, &valid, FALSE);
            if (!valid || value < 1 || value > 99)
            {
                MessageBoxW(hwnd, L"请输入 1–99 之间的分钟数。", L"自定义倒计时", MB_OK | MB_ICONINFORMATION);
                SetFocus(GetDlgItem(hwnd, IDC_COUNTDOWN_EDIT));
                SendDlgItemMessageW(hwnd, IDC_COUNTDOWN_EDIT, EM_SETSEL, 0, -1);
                return TRUE;
            }
            if (self)
                self->pendingCustomMinutes_ = static_cast<int>(value);
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

LRESULT CALLBACK TimerWidget::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    TimerWidget* self = reinterpret_cast<TimerWidget*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<TimerWidget*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        self->hwnd_ = nullptr;
        self->visible_ = false;
        self->dragging_ = false;
        self->trackingLeave_ = false;
        return 0;
    case WM_PAINT:
        self->Paint();
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId)
        {
            self->OnTick();
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (self->dragging_)
            self->UpdateDrag();
        else
            self->UpdateHover(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSELEAVE:
        self->trackingLeave_ = false;
        self->hoverPlay_ = false;
        self->hoverStop_ = false;
        self->hoverMark_ = false;
        self->hoverGrip_ = false;
        self->Paint();
        return 0;
    case WM_LBUTTONDOWN:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (x >= self->gripRect_.left && x < self->gripRect_.right &&
            y >= self->gripRect_.top && y < self->gripRect_.bottom)
        {
            self->BeginDrag();
            return 0;
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        if (self->dragging_)
        {
            self->EndDrag(true);
            return 0;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (x >= self->playRect_.left && x < self->playRect_.right &&
            y >= self->playRect_.top && y < self->playRect_.bottom)
        {
            if (!self->running_)
                self->Start();
        }
        else if (x >= self->stopRect_.left && x < self->stopRect_.right &&
            y >= self->stopRect_.top && y < self->stopRect_.bottom)
        {
            if (self->running_)
                self->Stop();
            else
                self->Reset();
        }
        else if (x >= self->markRect_.left && x < self->markRect_.right &&
            y >= self->markRect_.top && y < self->markRect_.bottom)
        {
            self->MarkSegment();
        }
        return 0;
    }
    case WM_RBUTTONUP:
        self->ShowModeMenu();
        return 0;
    case WM_CAPTURECHANGED:
        if (self->dragging_)
            self->EndDrag(true);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT cursor = {};
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);
            if (self->dragging_ ||
                (cursor.x >= self->gripRect_.left && cursor.x < self->gripRect_.right &&
                    cursor.y >= self->gripRect_.top && cursor.y < self->gripRect_.bottom))
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            }
            else if (
                (cursor.x >= self->playRect_.left && cursor.x < self->playRect_.right &&
                    cursor.y >= self->playRect_.top && cursor.y < self->playRect_.bottom) ||
                (cursor.x >= self->stopRect_.left && cursor.x < self->stopRect_.right &&
                    cursor.y >= self->stopRect_.top && cursor.y < self->stopRect_.bottom) ||
                (cursor.x >= self->markRect_.left && cursor.x < self->markRect_.right &&
                    cursor.y >= self->markRect_.top && cursor.y < self->markRect_.bottom))
            {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
            }
            else
            {
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            }
            return TRUE;
        }
        break;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
    case WM_DPICHANGED:
        if (self->enabled_)
            self->Refresh();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
