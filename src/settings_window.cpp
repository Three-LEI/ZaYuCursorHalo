#include "settings_window.h"

#include "resource.h"
#include "sound_decoder.h"

#include <imm.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace
{
constexpr wchar_t kSettingsClass[] = L"ZaYuCursorHalo.Settings.P1";
constexpr UINT kHotkeyCaptureMessage = WM_APP + 32;
constexpr float kWidth = 600.0f;
constexpr float kHeight = 560.0f;
constexpr float kSidebarWidth = 156.0f;
constexpr float kContentX = 188.0f;
constexpr float kContentW = 388.0f;
constexpr float kNavX = 12.0f;
constexpr float kNavW = 132.0f;
constexpr float kNavY = 100.0f;
constexpr float kNavH = 36.0f;
constexpr float kNavGap = 8.0f;
constexpr float kSwitchW = 40.0f;
constexpr float kSwitchH = 22.0f;
constexpr float kSliderW = 388.0f;
constexpr float kPreviewTop = 82.0f;
constexpr float kPreviewBottom = 225.0f;
constexpr float kFooterY = 520.0f;
constexpr int kColorCount = 5;
const unsigned kColors[kColorCount] = {
    0x5B8DFF,
    0x37C4B1,
    0x4CC46A,
    0xF2B23F,
    0xEE718A,
};
constexpr float kSwatchY = 392.0f;
constexpr float kSwatchStartX = 202.0f;
constexpr float kSwatchGap = 42.0f;
constexpr float kCustomSwatchX = kSwatchStartX + kSwatchGap * kColorCount;
constexpr float kModalHeight = 282.0f;
constexpr float kModalTop = (kHeight - kModalHeight) / 2.0f;
constexpr float kHexFieldLeft = 122.0f;
constexpr float kHexFieldTop = kModalTop + 202.0f;
constexpr float kHexFieldRight = 300.0f;
constexpr float kHexFieldBottom = kModalTop + 238.0f;
constexpr wchar_t kRepositoryUrl[] = L"https://github.com/Three-LEI/ZaYuCursorHalo";
constexpr wchar_t kReleasesUrl[] = L"https://github.com/Three-LEI/ZaYuCursorHalo/releases";
constexpr wchar_t kBilibiliUrl[] = L"https://space.bilibili.com/353544977";

void OpenExternalUrl(const wchar_t* url)
{
    if (url && *url)
        ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void CopyTextToClipboard(HWND owner, const wchar_t* text)
{
    if (!text || !*text || !OpenClipboard(owner))
        return;
    EmptyClipboard();
    const size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory)
    {
        void* target = GlobalLock(memory);
        if (target)
        {
            memcpy(target, text, bytes);
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_UNICODETEXT, memory))
                GlobalFree(memory);
        }
        else
        {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
}

constexpr float kHexCellsLeft = 164.0f;
constexpr float kHexCellWidth = 16.5f;
constexpr float kHexCellTop = kModalTop + 210.0f;
constexpr float kHexCellBottom = kModalTop + 230.0f;
constexpr int kPageCount = 9;
const wchar_t* kNavLabels[9] = {
    L"高亮", L"外观", L"按键", L"堆叠", L"计时", L"点击", L"拖尾", L"通用", L"关于"};
const wchar_t* kPageTitles[9] = {
    L"鼠标高亮",
    L"外观",
    L"按键显示",
    L"按键堆叠",
    L"计时器",
    L"点击反馈",
    L"鼠标拖尾",
    L"通用",
    L"关于",
};
const wchar_t* kPageSubtitles[9] = {
    L"让指针在屏幕里更清晰",
    L"选择一个适合当前场景的光环颜色",
    L"键帽皮肤、位置与过滤规则",
    L"最近快捷键逐条堆叠、逐条消失",
    L"浮动时间码、拖动位置与计时行为",
    L"左键/右键波纹颜色与点击音效",
    L"独立于高亮，调整长度、平滑度与颜色",
    L"启动方式与快捷键",
    L"开源仓库、版本与第三方组件",
};

D2D1_COLOR_F Rgb(unsigned rgb, float alpha = 1.0f)
{
    return D2D1::ColorF(
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha);
}

D2D1_COLOR_F Blend(const D2D1_COLOR_F& base, const D2D1_COLOR_F& overlay, float amount)
{
    return D2D1::ColorF(
        base.r + (overlay.r - base.r) * amount,
        base.g + (overlay.g - base.g) * amount,
        base.b + (overlay.b - base.b) * amount,
        base.a + (overlay.a - base.a) * amount);
}

float Clamp01(float value)
{
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

unsigned HsvToRgb(float hue, float saturation, float value)
{
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f)
        hue += 360.0f;
    saturation = Clamp01(saturation);
    value = Clamp01(value);

    const float chroma = value * saturation;
    const float section = hue / 60.0f;
    const float x = chroma * (1.0f - std::fabs(std::fmod(section, 2.0f) - 1.0f));
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    switch (static_cast<int>(section) % 6)
    {
    case 0: r = chroma; g = x; break;
    case 1: r = x; g = chroma; break;
    case 2: g = chroma; b = x; break;
    case 3: g = x; b = chroma; break;
    case 4: r = x; b = chroma; break;
    default: r = chroma; b = x; break;
    }
    const float m = value - chroma;
    const unsigned red = static_cast<unsigned>(std::lround(Clamp01(r + m) * 255.0f));
    const unsigned green = static_cast<unsigned>(std::lround(Clamp01(g + m) * 255.0f));
    const unsigned blue = static_cast<unsigned>(std::lround(Clamp01(b + m) * 255.0f));
    return (red << 16) | (green << 8) | blue;
}

void RgbToHsv(unsigned rgb, float& hue, float& saturation, float& value)
{
    const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
    const float maximum = (std::max)(r, (std::max)(g, b));
    const float minimum = (std::min)(r, (std::min)(g, b));
    const float delta = maximum - minimum;

    value = maximum;
    saturation = maximum > 0.0f ? delta / maximum : 0.0f;
    hue = 0.0f;
    if (delta <= 0.0f)
        return;
    if (maximum == r)
        hue = 60.0f * std::fmod((g - b) / delta, 6.0f);
    else if (maximum == g)
        hue = 60.0f * (((b - r) / delta) + 2.0f);
    else
        hue = 60.0f * (((r - g) / delta) + 4.0f);
    if (hue < 0.0f)
        hue += 360.0f;
}

bool IsPresetColor(unsigned rgb)
{
    for (unsigned color : kColors)
    {
        if (color == rgb)
            return true;
    }
    return false;
}

bool IsLightColor(unsigned rgb)
{
    const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
    return (0.299f * r + 0.587f * g + 0.114f * b) > 0.68f;
}

bool InRect(float x, float y, const D2D1_RECT_F& rect)
{
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

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

using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

UINT DpiForMonitor(HMONITOR monitor)
{
    static HMODULE shcore = LoadLibraryW(L"shcore.dll");
    static GetDpiForMonitorFn getDpiForMonitor = shcore
        ? reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"))
        : nullptr;

    if (getDpiForMonitor)
    {
        UINT dpiX = 96;
        UINT dpiY = 96;
        if (SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX >= 72 && dpiX <= 480)
            return dpiX;
    }
    return SystemDpi();
}

UINT DpiForWindowHandle(HWND hwnd)
{
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static HMODULE user32 = GetModuleHandleW(L"user32.dll");
    static GetDpiForWindowFn getDpiForWindow = user32
        ? reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"))
        : nullptr;
    if (getDpiForWindow)
    {
        const UINT dpi = getDpiForWindow(hwnd);
        if (dpi >= 72 && dpi <= 480)
            return dpi;
    }
    return SystemDpi();
}

D2D1_RECT_F Rect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, right, bottom);
}

bool CreateTextFormat(
    IDWriteFactory* factory,
    const wchar_t* family,
    float size,
    DWRITE_FONT_WEIGHT weight,
    DWRITE_TEXT_ALIGNMENT textAlignment,
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
    IDWriteTextFormat** output)
{
    const HRESULT hr = factory->CreateTextFormat(
        family,
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"zh-CN",
        output);
    if (FAILED(hr) || !*output)
        return false;
    (*output)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    (*output)->SetTextAlignment(textAlignment);
    (*output)->SetParagraphAlignment(paragraphAlignment);
    return true;
}
} // namespace

HHOOK SettingsWindow::hotkeyHook_ = nullptr;
HWND SettingsWindow::hotkeyHookTarget_ = nullptr;

SettingsWindow::~SettingsWindow()
{
    Destroy();
}

bool SettingsWindow::Create(HINSTANCE instance, AppSettings* settings, SettingsWindowHost* host)
{
    if (hwnd_)
        return true;
    instance_ = instance;
    settings_ = settings;
    host_ = host;
    if (!instance_ || !settings_ || !host_)
        return false;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    if (FAILED(hr) || !d2dFactory_)
        return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr) || !dwriteFactory_)
        return false;

    if (!CreateTextFormats())
        return false;

    // Small pointer glyph used by the live preview.
    if (SUCCEEDED(d2dFactory_->CreatePathGeometry(cursorGeometry_.GetAddressOf())) && cursorGeometry_)
    {
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (SUCCEEDED(cursorGeometry_->Open(&sink)) && sink)
        {
            sink->BeginFigure(D2D1::Point2F(0.0f, 0.0f), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(0.0f, 13.5f));
            sink->AddLine(D2D1::Point2F(3.4f, 10.4f));
            sink->AddLine(D2D1::Point2F(5.8f, 15.8f));
            sink->AddLine(D2D1::Point2F(7.8f, 14.8f));
            sink->AddLine(D2D1::Point2F(5.4f, 9.5f));
            sink->AddLine(D2D1::Point2F(10.2f, 9.5f));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
        }
    }

    if (!RegisterWindowClass())
        return false;

    scale_ = static_cast<float>(SystemDpi()) / 96.0f;
    const int pixelWidth = static_cast<int>(std::lround(kWidth * scale_));
    const int pixelHeight = static_cast<int>(std::lround(kHeight * scale_));

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kSettingsClass,
        L"ZaYuCursorHalo 设置",
        WS_POPUP,
        0,
        0,
        pixelWidth,
        pixelHeight,
        nullptr,
        nullptr,
        instance_,
        this);
    if (!hwnd_)
        return false;

    // Hex input is ASCII-only. Disable IME composition for this window so
    // typing in a Chinese IME does not swallow A-F or start a composition.
    ImmAssociateContext(hwnd_, nullptr);
    return true;
}

void SettingsWindow::Destroy()
{
    StopKeyboardCaptureHook();
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (classRegistered_)
    {
        UnregisterClassW(kSettingsClass, instance_);
        classRegistered_ = false;
    }
    visible_ = false;
    draggingWindow_ = false;
    sliderDrag_ = SliderDrag::None;
    colorPickerOpen_ = false;
    colorDrag_ = ColorDrag::None;
    hexFocused_ = false;
    hexSelectAll_ = false;
    DiscardRenderTarget();
    cursorGeometry_.Reset();
    brandFormat_.Reset();
    titleFormat_.Reset();
    subtitleFormat_.Reset();
    bodyFormat_.Reset();
    bodyStrongFormat_.Reset();
    captionFormat_.Reset();
    captionCenterFormat_.Reset();
    navFormat_.Reset();
    valueRightFormat_.Reset();
    centerFormat_.Reset();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();
    settings_ = nullptr;
    host_ = nullptr;
}

void SettingsWindow::Show()
{
    if (!hwnd_)
        return;
    if (visible_)
    {
        SetForegroundWindow(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    POINT cursor = {};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(monitor, &monitorInfo);

    const UINT dpi = DpiForMonitor(monitor);
    const float newScale = static_cast<float>(dpi) / 96.0f;
    const int pixelWidth = static_cast<int>(std::lround(kWidth * newScale));
    const int pixelHeight = static_cast<int>(std::lround(kHeight * newScale));
    const RECT& work = monitorInfo.rcWork;
    const int x = work.left + ((work.right - work.left) - pixelWidth) / 2;
    const int y = work.top + ((work.bottom - work.top) - pixelHeight) / 2;

    scale_ = newScale;
    DiscardRenderTarget();
    SetWindowPos(
        hwnd_,
        nullptr,
        x,
        y,
        pixelWidth,
        pixelHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    visible_ = true;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::Hide()
{
    if (!hwnd_ || !visible_)
        return;
    if (hotkeyRecording_)
        CancelHotkeyCapture();
    focus_ = FocusId::None;
    OnCaptureChanged();
    colorPickerOpen_ = false;
    colorDrag_ = ColorDrag::None;
    hoverColorCancel_ = false;
    hoverColorDone_ = false;
    hexFocused_ = false;
    hexSelectAll_ = false;
    ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
}

void SettingsWindow::Refresh()
{
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetDarkMode(bool dark)
{
    if (darkMode_ == dark)
        return;
    darkMode_ = dark;
    palette_ = PaletteFor(dark);
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetThemeMode(ThemeMode mode)
{
    if (!settings_)
        return;
    const std::wstring text = FormatThemeMode(mode);
    if (settings_->theme == text)
        return;
    settings_->theme = text;
    if (host_)
        host_->OnSettingsThemeChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetKeySkin(KeySkin skin)
{
    if (!settings_)
        return;
    const std::wstring text = FormatKeySkin(skin);
    if (settings_->keySkin == text)
        return;
    settings_->keySkin = text;
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetKeyPosition(KeyPosition position)
{
    if (!settings_)
        return;
    const std::wstring text = FormatKeyPosition(position);
    if (settings_->keyPosition == text)
        return;
    settings_->keyPosition = text;
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetKeyScale(int percent)
{
    if (!settings_)
        return;
    const int value = (std::max)(70, (std::min)(150, percent));
    if (settings_->keyScalePercent == value)
        return;
    settings_->keyScalePercent = value;
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetKeyOpacity(int percent)
{
    if (!settings_)
        return;
    const int value = (std::max)(20, (std::min)(100, percent));
    if (settings_->keyOpacityPercent == value)
        return;
    settings_->keyOpacityPercent = value;
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetTrailValue(int index, int value)
{
    if (!settings_)
        return;
    int* field = nullptr;
    int minimum = 0;
    int maximum = 100;
    switch (index)
    {
    case 0: field = &settings_->trailLengthPercent; minimum = 50; maximum = 200; break;
    case 1: field = &settings_->trailLifetimeMs; minimum = 100; maximum = 1200; break;
    case 2: field = &settings_->trailSmoothness; minimum = 0; maximum = 100; break;
    case 3: field = &settings_->trailWidthPercent; minimum = 50; maximum = 200; break;
    case 4: field = &settings_->trailOpacityPercent; minimum = 10; maximum = 100; break;
    case 5: field = &settings_->trailGlowPercent; minimum = 0; maximum = 100; break;
    default: return;
    }
    const int clamped = (std::max)(minimum, (std::min)(maximum, value));
    if (*field == clamped)
        return;
    *field = clamped;
    if (host_)
        host_->OnSettingsTrailSettingsChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetTrailColorAuto(bool automatic)
{
    if (!settings_ || settings_->trailColorAuto == automatic)
        return;
    const bool wasAutomatic = settings_->trailColorAuto;
    settings_->trailColorAuto = automatic;
    if (!automatic && wasAutomatic)
        settings_->trailColorRgb = settings_->accentRgb;
    if (host_)
        host_->OnSettingsTrailSettingsChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::ResetTrailDefaults()
{
    if (!settings_)
        return;
    settings_->trailEnabled = false;
    settings_->trailLengthPercent = 100;
    settings_->trailLifetimeMs = 320;
    settings_->trailSmoothness = 70;
    settings_->trailWidthPercent = 100;
    settings_->trailOpacityPercent = 100;
    settings_->trailGlowPercent = 60;
    settings_->trailColorAuto = true;
    settings_->trailColorRgb = 0x5B8DFF;
    if (host_)
        host_->OnSettingsTrailChanged(false);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::ResetAllDefaults()
{
    if (!settings_)
        return;
    AppSettings defaults;
    defaults.firstRun = false;
    *settings_ = defaults;
    if (host_)
        host_->OnSettingsResetAll();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::UpdateTrailValue(int index, float x)
{
    const D2D1_RECT_F track = TrailTrackRect(index);
    const float fraction = Clamp01((x - track.left) / (track.right - track.left));
    int minimum = 0;
    int maximum = 100;
    switch (index)
    {
    case 0: minimum = 50; maximum = 200; break;
    case 1: minimum = 100; maximum = 1200; break;
    case 3: minimum = 50; maximum = 200; break;
    case 4: minimum = 10; maximum = 100; break;
    default: break;
    }
    SetTrailValue(
        index,
        minimum + static_cast<int>(std::lround(fraction * static_cast<float>(maximum - minimum))));
}

void SettingsWindow::SetClickVolume(int percent)
{
    if (!settings_)
        return;
    const int value = (std::max)(0, (std::min)(100, percent));
    if (settings_->clickVolumePercent == value)
        return;
    settings_->clickVolumePercent = value;
    if (host_)
        host_->OnSettingsClickChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::UpdateClickVolumeValue(float x)
{
    const D2D1_RECT_F track = ClickVolumeTrackRect();
    const float fraction = Clamp01((x - track.left) / (track.right - track.left));
    SetClickVolume(static_cast<int>(std::lround(fraction * 100.0f)));
}

void SettingsWindow::UpdateKeyScaleValue(float x)
{
    const D2D1_RECT_F track = KeyScaleTrackRect();
    const float fraction = Clamp01((x - track.left) / (track.right - track.left));
    SetKeyScale(70 + static_cast<int>(std::lround(fraction * 80.0f)));
}

void SettingsWindow::UpdateKeyOpacityValue(float x)
{
    const D2D1_RECT_F track = KeyOpacityTrackRect();
    const float fraction = Clamp01((x - track.left) / (track.right - track.left));
    SetKeyOpacity(20 + static_cast<int>(std::lround(fraction * 80.0f)));
}

void SettingsWindow::SetKeyDelay(int delayMs)
{
    if (!settings_)
        return;
    const int value = ClampKeyDelay(delayMs);
    if (settings_->keyDelayMs == value)
        return;
    settings_->keyDelayMs = value;
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::SetKeyLetters(bool enabled)
{
    if (!settings_ || settings_->showLetterKeys == enabled)
        return;
    settings_->showLetterKeys = enabled;
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::UpdateKeyDelayValue(float x)
{
    if (!settings_)
        return;
    const float fraction = Clamp01((x - kContentX) / kContentW);
    SetKeyDelay(static_cast<int>(std::lround(
        static_cast<float>(kMinKeyDelayMs) +
        fraction * static_cast<float>(kMaxKeyDelayMs - kMinKeyDelayMs))));
}

void SettingsWindow::UpdateStackDelayValue(float x)
{
    if (!settings_)
        return;
    const float fraction = Clamp01((x - kContentX) / kContentW);
    settings_->keyStackDelayMs = (std::max)(0, (std::min)(
        3000,
        static_cast<int>(std::lround(fraction * 3000.0f))));
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

std::vector<SettingsWindow::FocusId> SettingsWindow::BuildFocusOrder() const
{
    std::vector<FocusId> order = {
        FocusId::NavHighlight,
        FocusId::NavAppearance,
        FocusId::NavKeys,
        FocusId::NavStack,
        FocusId::NavTimer,
        FocusId::NavClick,
        FocusId::NavTrail,
        FocusId::NavGeneral,
        FocusId::NavAbout,
    };
    if (page_ == Page::Highlight)
    {
        order.push_back(FocusId::Enable);
        order.push_back(FocusId::HaloStyleRing);
        order.push_back(FocusId::HaloStyleDouble);
        order.push_back(FocusId::HaloStyleCrosshair);
        order.push_back(FocusId::HaloStyleDot);
        order.push_back(FocusId::Radius);
        order.push_back(FocusId::Opacity);
    }
    else if (page_ == Page::Appearance)
    {
        order.push_back(FocusId::Color0);
        order.push_back(FocusId::Color1);
        order.push_back(FocusId::Color2);
        order.push_back(FocusId::Color3);
        order.push_back(FocusId::Color4);
        order.push_back(FocusId::CustomColor);
    }
    else if (page_ == Page::Keys)
    {
        order.push_back(FocusId::KeySkinClassic);
        order.push_back(FocusId::KeySkinMinimal);
        order.push_back(FocusId::KeySkinMechanical);
        order.push_back(FocusId::KeySkinWin98);
        order.push_back(FocusId::KeyPositionBottom);
        order.push_back(FocusId::KeyPositionTop);
        order.push_back(FocusId::KeyPositionCursor);
        order.push_back(FocusId::KeyPositionCustom);
        order.push_back(FocusId::KeyDelay);
        order.push_back(FocusId::KeyScale);
        order.push_back(FocusId::KeyOpacity);
        order.push_back(FocusId::KeyLetters);
        order.push_back(FocusId::KeyTextAuto);
        order.push_back(FocusId::KeyTextColor);
        order.push_back(FocusId::KeyStyleColor);
    }
    else if (page_ == Page::Click)
    {
        order.push_back(FocusId::ClickColorLeft);
        order.push_back(FocusId::ClickColorRight);
        order.push_back(FocusId::ClickRipple);
        order.push_back(FocusId::ClickSound);
        order.push_back(FocusId::ClickVolume);
        order.push_back(FocusId::ClickPresetDefault);
        order.push_back(FocusId::ClickPresetSoft);
        order.push_back(FocusId::ClickPresetCrisp);
        order.push_back(FocusId::ClickPresetWood);
        order.push_back(FocusId::ClickPresetSynth);
        order.push_back(FocusId::ClickImport);
    }
    else if (page_ == Page::Trail)
    {
        order.push_back(FocusId::TrailEnabled);
        order.push_back(FocusId::TrailLength);
        order.push_back(FocusId::TrailLifetime);
        order.push_back(FocusId::TrailSmoothness);
        order.push_back(FocusId::TrailWidth);
        order.push_back(FocusId::TrailOpacity);
        order.push_back(FocusId::TrailGlow);
        order.push_back(FocusId::TrailColorAuto);
        order.push_back(FocusId::TrailColorCustom);
        order.push_back(FocusId::TrailColorSwatch);
        order.push_back(FocusId::TrailReset);
    }
    else if (page_ == Page::Stack)
    {
        order.push_back(FocusId::StackEnabled);
        order.push_back(FocusId::StackDelay);
    }
    else if (page_ == Page::Timer)
    {
        order.push_back(FocusId::TimerEnabled);
        order.push_back(FocusId::TimerAutoStart);
        order.push_back(FocusId::TimerShowHours);
        order.push_back(FocusId::TimerResetOnStop);
        order.push_back(FocusId::TimerReset);
    }
    else if (page_ == Page::General)
    {
        order.push_back(FocusId::AutoStart);
        order.push_back(FocusId::Hotkey);
        order.push_back(FocusId::ThemeSystem);
        order.push_back(FocusId::ThemeLight);
        order.push_back(FocusId::ThemeDark);
        order.push_back(FocusId::ShowKeys);
        order.push_back(FocusId::TrailEnabled);
        order.push_back(FocusId::ResetAll);
    }
    else
    {
        order.push_back(FocusId::AboutRepo);
        order.push_back(FocusId::AboutCopy);
        order.push_back(FocusId::AboutReleases);
        order.push_back(FocusId::AboutBilibili);
    }
    order.push_back(FocusId::Close);
    return order;
}

void SettingsWindow::SetFocusId(FocusId id)
{
    if (focus_ == id)
        return;
    focus_ = id;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::AdvanceFocus(bool backwards)
{
    const std::vector<FocusId> order = BuildFocusOrder();
    if (order.empty())
        return;
    auto it = std::find(order.begin(), order.end(), focus_);
    if (it == order.end())
    {
        SetFocusId(backwards ? order.back() : order.front());
        return;
    }
    if (backwards)
    {
        if (it == order.begin())
            SetFocusId(order.back());
        else
            SetFocusId(*(it - 1));
    }
    else
    {
        ++it;
        SetFocusId(it == order.end() ? order.front() : *it);
    }
}

bool SettingsWindow::HandleFocusKey(WPARAM key)
{
    if (key == VK_TAB)
    {
        AdvanceFocus((GetKeyState(VK_SHIFT) & 0x8000) != 0);
        return true;
    }
    if (focus_ == FocusId::None)
        return false;

    if (key == VK_RETURN || key == VK_SPACE)
    {
        ActivateFocus();
        return true;
    }

    const auto navIndex = [&]() -> int {
        const int index = static_cast<int>(focus_) - static_cast<int>(FocusId::NavHighlight);
        return (index >= 0 && index < kPageCount) ? index : -1;
    };
    if (navIndex() >= 0)
    {
        int index = navIndex();
        if (key == VK_UP || key == VK_LEFT)
            index = (index + kPageCount - 1) % kPageCount;
        else if (key == VK_DOWN || key == VK_RIGHT)
            index = (index + 1) % kPageCount;
        else
            return false;
        SetFocusId(static_cast<FocusId>(
            static_cast<int>(FocusId::NavHighlight) + index));
        return true;
    }

    if (focus_ == FocusId::Radius || focus_ == FocusId::Opacity)
    {
        const bool radius = focus_ == FocusId::Radius;
        int minimum = radius ? 20 : 30;
        int maximum = radius ? 80 : 100;
        int value = radius ? settings_->radiusDip : settings_->opacityPercent;
        int step = 1;
        if (key == VK_LEFT || key == VK_DOWN)
            value -= step;
        else if (key == VK_RIGHT || key == VK_UP)
            value += step;
        else if (key == VK_PRIOR)
            value += 5;
        else if (key == VK_NEXT)
            value -= 5;
        else if (key == VK_HOME)
            value = minimum;
        else if (key == VK_END)
            value = maximum;
        else
            return false;
        value = (std::max)(minimum, (std::min)(maximum, value));
        if (radius)
            settings_->radiusDip = value;
        else
            settings_->opacityPercent = value;
        if (host_)
            host_->OnSettingsStyleChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    int skinIndex = -1;
    switch (focus_)
    {
    case FocusId::KeySkinClassic: skinIndex = 0; break;
    case FocusId::KeySkinMinimal: skinIndex = 1; break;
    case FocusId::KeySkinMechanical: skinIndex = 2; break;
    case FocusId::KeySkinWin98: skinIndex = 3; break;
    default: break;
    }
    if (skinIndex >= 0)
    {
        int next = skinIndex;
        if (key == VK_LEFT || key == VK_UP)
            next = (skinIndex + 3) % 4;
        else if (key == VK_RIGHT || key == VK_DOWN)
            next = (skinIndex + 1) % 4;
        else if (key == VK_HOME)
            next = 0;
        else if (key == VK_END)
            next = 3;
        else
            return false;
        constexpr FocusId focusOrder[4] = {
            FocusId::KeySkinClassic,
            FocusId::KeySkinMinimal,
            FocusId::KeySkinMechanical,
            FocusId::KeySkinWin98,
        };
        constexpr KeySkin skinOrder[4] = {
            KeySkin::Classic,
            KeySkin::Minimal,
            KeySkin::Mechanical,
            KeySkin::Win98,
        };
        SetFocusId(focusOrder[next]);
        SetKeySkin(skinOrder[next]);
        return true;
    }

    int positionIndex = -1;
    if (focus_ >= FocusId::KeyPositionBottom && focus_ <= FocusId::KeyPositionCustom)
        positionIndex = static_cast<int>(focus_) - static_cast<int>(FocusId::KeyPositionBottom);
    if (positionIndex >= 0)
    {
        int next = positionIndex;
        if (key == VK_LEFT || key == VK_UP)
            next = (positionIndex + 3) % 4;
        else if (key == VK_RIGHT || key == VK_DOWN)
            next = (positionIndex + 1) % 4;
        else if (key == VK_HOME)
            next = 0;
        else if (key == VK_END)
            next = 3;
        else
            return false;
        SetFocusId(static_cast<FocusId>(
            static_cast<int>(FocusId::KeyPositionBottom) + next));
        if (next < 3)
            SetKeyPosition(static_cast<KeyPosition>(next));
        return true;
    }

    if (focus_ == FocusId::KeyDelay)
    {
        int value = settings_->keyDelayMs;
        if (key == VK_LEFT || key == VK_DOWN)
            value -= 50;
        else if (key == VK_RIGHT || key == VK_UP)
            value += 50;
        else if (key == VK_PRIOR)
            value += 200;
        else if (key == VK_NEXT)
            value -= 200;
        else if (key == VK_HOME)
            value = kMinKeyDelayMs;
        else if (key == VK_END)
            value = kMaxKeyDelayMs;
        else
            return false;
        SetKeyDelay(value);
        return true;
    }

    if (focus_ == FocusId::KeyScale)
    {
        int value = settings_->keyScalePercent;
        if (key == VK_LEFT || key == VK_DOWN)
            value -= 5;
        else if (key == VK_RIGHT || key == VK_UP)
            value += 5;
        else if (key == VK_PRIOR)
            value += 10;
        else if (key == VK_NEXT)
            value -= 10;
        else if (key == VK_HOME)
            value = 70;
        else if (key == VK_END)
            value = 150;
        else
            return false;
        SetKeyScale(value);
        return true;
    }

    if (focus_ == FocusId::KeyOpacity)
    {
        int value = settings_->keyOpacityPercent;
        if (key == VK_LEFT || key == VK_DOWN)
            value -= 5;
        else if (key == VK_RIGHT || key == VK_UP)
            value += 5;
        else if (key == VK_PRIOR)
            value += 10;
        else if (key == VK_NEXT)
            value -= 10;
        else if (key == VK_HOME)
            value = 20;
        else if (key == VK_END)
            value = 100;
        else
            return false;
        SetKeyOpacity(value);
        return true;
    }

    if (focus_ == FocusId::ClickVolume)
    {
        int value = settings_->clickVolumePercent;
        if (key == VK_LEFT || key == VK_DOWN)
            value -= 5;
        else if (key == VK_RIGHT || key == VK_UP)
            value += 5;
        else if (key == VK_PRIOR)
            value += 10;
        else if (key == VK_NEXT)
            value -= 10;
        else if (key == VK_HOME)
            value = 0;
        else if (key == VK_END)
            value = 100;
        else
            return false;
        SetClickVolume(value);
        return true;
    }

    if (focus_ == FocusId::StackDelay)
    {
        int value = settings_->keyStackDelayMs;
        if (key == VK_LEFT || key == VK_DOWN)
            value -= 100;
        else if (key == VK_RIGHT || key == VK_UP)
            value += 100;
        else if (key == VK_PRIOR)
            value += 500;
        else if (key == VK_NEXT)
            value -= 500;
        else if (key == VK_HOME)
            value = 0;
        else if (key == VK_END)
            value = 3000;
        else
            return false;
        settings_->keyStackDelayMs = (std::max)(0, (std::min)(3000, value));
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    const int trailSlider = [&]() -> int {
        switch (focus_)
        {
        case FocusId::TrailLength: return 0;
        case FocusId::TrailLifetime: return 1;
        case FocusId::TrailSmoothness: return 2;
        case FocusId::TrailWidth: return 3;
        case FocusId::TrailOpacity: return 4;
        case FocusId::TrailGlow: return 5;
        default: return -1;
        }
    }();
    if (trailSlider >= 0)
    {
        int minimum = 0;
        int maximum = 100;
        int step = 5;
        switch (trailSlider)
        {
        case 1: minimum = 100; maximum = 1200; step = 20; break;
        case 3: minimum = 50; maximum = 200; break;
        case 4: minimum = 10; maximum = 100; break;
        default: break;
        }
        int value = 0;
        switch (trailSlider)
        {
        case 0: value = settings_->trailLengthPercent; break;
        case 1: value = settings_->trailLifetimeMs; break;
        case 2: value = settings_->trailSmoothness; break;
        case 3: value = settings_->trailWidthPercent; break;
        case 4: value = settings_->trailOpacityPercent; break;
        case 5: value = settings_->trailGlowPercent; break;
        }
        if (key == VK_LEFT || key == VK_DOWN)
            value -= step;
        else if (key == VK_RIGHT || key == VK_UP)
            value += step;
        else if (key == VK_PRIOR)
            value += step * 2;
        else if (key == VK_NEXT)
            value -= step * 2;
        else if (key == VK_HOME)
            value = minimum;
        else if (key == VK_END)
            value = maximum;
        else
            return false;
        SetTrailValue(trailSlider, value);
        return true;
    }

    if (focus_ == FocusId::TrailColorAuto ||
        focus_ == FocusId::TrailColorCustom ||
        focus_ == FocusId::TrailColorSwatch)
    {
        int index = focus_ == FocusId::TrailColorAuto ? 0 :
            (focus_ == FocusId::TrailColorCustom ? 1 : 2);
        if (key == VK_LEFT)
            index = (index + 2) % 3;
        else if (key == VK_RIGHT)
            index = (index + 1) % 3;
        else if (key == VK_HOME)
            index = 0;
        else if (key == VK_END)
            index = 2;
        else
            return false;
        SetFocusId(static_cast<FocusId>(static_cast<int>(FocusId::TrailColorAuto) + index));
        return true;
    }

    const int colorIndex = [&]() -> int {
        switch (focus_)
        {
        case FocusId::Color0: return 0;
        case FocusId::Color1: return 1;
        case FocusId::Color2: return 2;
        case FocusId::Color3: return 3;
        case FocusId::Color4: return 4;
        case FocusId::CustomColor: return 5;
        default: return -1;
        }
    }();
    if (colorIndex >= 0)
    {
        int next = colorIndex;
        if (key == VK_LEFT)
            next = (colorIndex + 5) % 6;
        else if (key == VK_RIGHT)
            next = (colorIndex + 1) % 6;
        else if (key == VK_HOME)
            next = 0;
        else if (key == VK_END)
            next = 5;
        else
            return false;

        SetFocusId(static_cast<FocusId>(
            static_cast<int>(FocusId::Color0) + next));
        if (next < kColorCount)
        {
            settings_->accentRgb = kColors[next];
            if (host_)
                host_->OnSettingsStyleChanged();
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    if (focus_ == FocusId::ThemeSystem || focus_ == FocusId::ThemeLight ||
        focus_ == FocusId::ThemeDark)
    {
        int mode = static_cast<int>(ParseThemeMode(settings_->theme));
        if (key == VK_LEFT)
            mode = (mode + 2) % 3;
        else if (key == VK_RIGHT)
            mode = (mode + 1) % 3;
        else
            return false;
        SetFocusId(static_cast<FocusId>(
            static_cast<int>(FocusId::ThemeSystem) + mode));
        SetThemeMode(static_cast<ThemeMode>(mode));
        return true;
    }

    return false;
}

void SettingsWindow::ActivateFocus()
{
    switch (focus_)
    {
    case FocusId::NavHighlight:
        SwitchPage(Page::Highlight);
        break;
    case FocusId::NavAppearance:
        SwitchPage(Page::Appearance);
        break;
    case FocusId::NavKeys:
        SwitchPage(Page::Keys);
        break;
    case FocusId::NavTimer:
        SwitchPage(Page::Timer);
        break;
    case FocusId::NavClick:
        SwitchPage(Page::Click);
        break;
    case FocusId::NavStack:
        SwitchPage(Page::Stack);
        break;
    case FocusId::NavGeneral:
        SwitchPage(Page::General);
        break;
    case FocusId::Enable:
        settings_->enabled = !settings_->enabled;
        if (host_)
            host_->OnSettingsEnabledChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::HaloStyleRing:
    case FocusId::HaloStyleDouble:
    case FocusId::HaloStyleCrosshair:
    case FocusId::HaloStyleDot:
    {
        const int style = static_cast<int>(focus_) - static_cast<int>(FocusId::HaloStyleRing);
        settings_->haloStyle = FormatHaloStyle(static_cast<HaloStyle>(style));
        if (host_)
            host_->OnSettingsStyleChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    }
    case FocusId::Color0:
    case FocusId::Color1:
    case FocusId::Color2:
    case FocusId::Color3:
    case FocusId::Color4:
    {
        const int index = static_cast<int>(focus_) - static_cast<int>(FocusId::Color0);
        settings_->accentRgb = kColors[index];
        if (host_)
            host_->OnSettingsStyleChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    }
    case FocusId::CustomColor:
        OpenColorPicker();
        break;
    case FocusId::AutoStart:
        settings_->startWithWindows = !settings_->startWithWindows;
        if (host_)
            host_->OnSettingsAutoStartChanged(settings_->startWithWindows);
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::Hotkey:
        StartHotkeyCapture();
        break;
    case FocusId::ThemeSystem:
        SetThemeMode(ThemeMode::System);
        break;
    case FocusId::ThemeLight:
        SetThemeMode(ThemeMode::Light);
        break;
    case FocusId::ThemeDark:
        SetThemeMode(ThemeMode::Dark);
        break;
    case FocusId::ShowKeys:
        settings_->showKeys = !settings_->showKeys;
        if (host_)
            host_->OnSettingsShowKeysChanged(settings_->showKeys);
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::TrailEnabled:
        settings_->trailEnabled = !settings_->trailEnabled;
        if (host_)
            host_->OnSettingsTrailChanged(settings_->trailEnabled);
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::TrailColorAuto:
        SetTrailColorAuto(true);
        break;
    case FocusId::TrailColorCustom:
    case FocusId::TrailColorSwatch:
    {
        const bool wasAutomatic = settings_->trailColorAuto;
        OpenColorPicker(ColorTarget::Trail);
        if (wasAutomatic)
            SetTrailColorAuto(false);
        break;
    }
    case FocusId::TrailReset:
        ResetTrailDefaults();
        break;
    case FocusId::ResetAll:
        ResetAllDefaults();
        break;
    case FocusId::AboutRepo:
        OpenExternalUrl(kRepositoryUrl);
        break;
    case FocusId::AboutCopy:
        CopyTextToClipboard(hwnd_, kRepositoryUrl);
        break;
    case FocusId::AboutReleases:
        OpenExternalUrl(kReleasesUrl);
        break;
    case FocusId::AboutBilibili:
        OpenExternalUrl(kBilibiliUrl);
        break;
    case FocusId::StackEnabled:
        settings_->keyStack = !settings_->keyStack;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::StackDelay:
        break;
    case FocusId::SkinFill:
        OpenColorPicker(ColorTarget::KeyFill);
        break;
    case FocusId::SkinBorder:
        OpenColorPicker(ColorTarget::KeyBorder);
        break;
    case FocusId::SkinText:
        OpenColorPicker(ColorTarget::KeyText);
        break;
    case FocusId::SkinImage:
        ImportKeyBackground();
        break;
    case FocusId::SkinImageClear:
        settings_->keyBackgroundImage.clear();
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::KeySkinClassic:
        SetKeySkin(KeySkin::Classic);
        break;
    case FocusId::KeySkinMinimal:
        SetKeySkin(KeySkin::Minimal);
        break;
    case FocusId::KeySkinNeon:
        SetKeySkin(KeySkin::Neon);
        break;
    case FocusId::KeySkinPaper:
        SetKeySkin(KeySkin::Paper);
        break;
    case FocusId::KeySkinTerminal:
        SetKeySkin(KeySkin::Terminal);
        break;
    case FocusId::KeySkinMechanical:
        SetKeySkin(KeySkin::Mechanical);
        break;
    case FocusId::KeySkinWin98:
        SetKeySkin(KeySkin::Win98);
        break;
    case FocusId::KeyTextAuto:
        settings_->keyTextAuto = true;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::KeyTextColor:
        OpenColorPicker(ColorTarget::KeyText);
        break;
    case FocusId::KeyStyleColor:
        OpenColorPicker(ColorTarget::KeyStyle);
        break;
    case FocusId::KeyPositionBottom:
        SetKeyPosition(KeyPosition::BottomCenter);
        break;
    case FocusId::KeyPositionTop:
        SetKeyPosition(KeyPosition::TopCenter);
        break;
    case FocusId::KeyPositionCursor:
        SetKeyPosition(KeyPosition::FollowCursor);
        break;
    case FocusId::KeyPositionCustom:
        if (host_)
            host_->OnSettingsKeyPositionCustomRequested();
        break;
    case FocusId::KeyLetters:
        SetKeyLetters(!settings_->showLetterKeys);
        break;
    case FocusId::ClickColorLeft:
        OpenColorPicker(ColorTarget::ClickLeft);
        break;
    case FocusId::ClickColorRight:
        OpenColorPicker(ColorTarget::ClickRight);
        break;
    case FocusId::ClickRipple:
        settings_->clickRippleEnabled = !settings_->clickRippleEnabled;
        if (host_)
            host_->OnSettingsClickChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::ClickSound:
        settings_->clickSoundEnabled = !settings_->clickSoundEnabled;
        if (host_)
            host_->OnSettingsClickChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::ClickPresetDefault:
    case FocusId::ClickPresetSoft:
    case FocusId::ClickPresetCrisp:
    case FocusId::ClickPresetWood:
    case FocusId::ClickPresetSynth:
    {
        const int preset = static_cast<int>(focus_) - static_cast<int>(FocusId::ClickPresetDefault);
        settings_->clickSoundPreset = FormatClickSoundPreset(static_cast<ClickSoundPreset>(preset));
        if (host_)
        {
            host_->OnSettingsClickChanged();
            host_->OnSettingsPreviewClickSound(false);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    }
    case FocusId::ClickImport:
        ImportClickSound();
        break;
    case FocusId::TimerEnabled:
        settings_->timerEnabled = !settings_->timerEnabled;
        if (host_)
            host_->OnSettingsTimerChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::TimerAutoStart:
        settings_->timerAutoStart = !settings_->timerAutoStart;
        if (host_)
            host_->OnSettingsTimerChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::TimerShowHours:
        settings_->timerShowHours = !settings_->timerShowHours;
        if (host_)
            host_->OnSettingsTimerChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::TimerResetOnStop:
        settings_->timerResetOnStop = !settings_->timerResetOnStop;
        if (host_)
            host_->OnSettingsTimerChanged();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case FocusId::TimerReset:
        if (host_)
            host_->OnSettingsTimerResetRequested();
        break;
    case FocusId::Close:
        CloseRequested();
        break;
    default:
        break;
    }
}

void SettingsWindow::DrawFocusRings()
{
    if (colorPickerOpen_ || focus_ == FocusId::None || !settings_)
        return;
    DrawFocusRingFor(focus_);
}

void SettingsWindow::DrawFocusRingFor(FocusId id)
{
    const D2D1_COLOR_F focusColor = D2D1::ColorF(
        palette_.text.r,
        palette_.text.g,
        palette_.text.b,
        0.88f);
    switch (id)
    {
    case FocusId::NavHighlight:
    case FocusId::NavAppearance:
    case FocusId::NavKeys:
    case FocusId::NavTimer:
    case FocusId::NavGeneral:
    {
        const int index = static_cast<int>(id) - static_cast<int>(FocusId::NavHighlight);
        StrokeRounded(NavRect(index), 8.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::Enable:
        StrokeRounded(Rect(532.0f, 334.0f, 580.0f, 364.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::HaloStyleRing:
    case FocusId::HaloStyleDouble:
    case FocusId::HaloStyleCrosshair:
    case FocusId::HaloStyleDot:
    {
        const int style = static_cast<int>(id) - static_cast<int>(FocusId::HaloStyleRing);
        StrokeRounded(HaloStyleChipRect(style), 9.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::Radius:
        StrokeRounded(Rect(176.0f, 416.0f, 588.0f, 442.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::Opacity:
        StrokeRounded(Rect(176.0f, 468.0f, 588.0f, 494.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::Color0:
    case FocusId::Color1:
    case FocusId::Color2:
    case FocusId::Color3:
    case FocusId::Color4:
    {
        const int index = static_cast<int>(id) - static_cast<int>(FocusId::Color0);
        if (settings_->accentRgb == kColors[index])
            break;
        const D2D1_POINT_2F center = D2D1::Point2F(kSwatchStartX + kSwatchGap * index, kSwatchY);
        brush_->SetColor(focusColor);
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 24.5f, 24.5f), brush_.Get(), 1.8f);
        break;
    }
    case FocusId::CustomColor:
        // The custom swatch is an action (open the picker), not one of the
        // preset color choices. Drawing a focus ring here looks like a second
        // selected color next to the actual preset selection.
        break;
    case FocusId::AutoStart:
        StrokeRounded(Rect(532.0f, 84.0f, 580.0f, 114.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::Hotkey:
        StrokeRounded(Rect(442.0f, 162.0f, 578.0f, 198.0f), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::ThemeSystem:
    case FocusId::ThemeLight:
    case FocusId::ThemeDark:
    {
        const int index = static_cast<int>(id) - static_cast<int>(FocusId::ThemeSystem);
        const D2D1_RECT_F chip = ThemeChipRect(index);
        StrokeRounded(Rect(chip.left + 2.0f, chip.top + 2.0f, chip.right - 2.0f, chip.bottom - 2.0f), 6.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::ShowKeys:
        StrokeRounded(Rect(532.0f, 320.0f, 580.0f, 350.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::TrailEnabled:
        if (page_ == Page::Trail)
            StrokeRounded(Rect(516.0f, 90.0f, 564.0f, 120.0f), 8.0f, focusColor, 1.6f);
        else
            StrokeRounded(Rect(532.0f, 386.0f, 580.0f, 416.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::TrailReset:
        StrokeRounded(Rect(184.0f, 488.0f, 312.0f, 520.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::ResetAll:
        StrokeRounded(Rect(396.0f, 506.0f, 580.0f, 538.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::AboutRepo:
        StrokeRounded(Rect(184.0f, 254.0f, 304.0f, 292.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::AboutCopy:
        StrokeRounded(Rect(304.0f, 254.0f, 412.0f, 292.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::AboutReleases:
        StrokeRounded(Rect(412.0f, 254.0f, 580.0f, 292.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::AboutBilibili:
        StrokeRounded(Rect(184.0f, 426.0f, 580.0f, 468.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::TrailLength:
    case FocusId::TrailLifetime:
    case FocusId::TrailSmoothness:
    case FocusId::TrailWidth:
    case FocusId::TrailOpacity:
    case FocusId::TrailGlow:
    {
        const int index = static_cast<int>(focus_) - static_cast<int>(FocusId::TrailLength);
        const float top = 142.0f + static_cast<float>(index) * 48.0f;
        StrokeRounded(Rect(176.0f, top, 588.0f, top + 42.0f), 8.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::TrailColorAuto:
        StrokeRounded(Rect(184.0f, 450.0f, 266.0f, 488.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::TrailColorCustom:
        StrokeRounded(Rect(266.0f, 450.0f, 348.0f, 488.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::TrailColorSwatch:
        StrokeRounded(Rect(348.0f, 450.0f, 388.0f, 488.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::StackEnabled:
        StrokeRounded(
            Rect(StackToggleRect().left - 4.0f, StackToggleRect().top - 4.0f,
                StackToggleRect().right + 4.0f, StackToggleRect().bottom + 4.0f),
            9.0f,
            focusColor,
            1.6f);
        break;
    case FocusId::StackDelay:
        StrokeRounded(Rect(176.0f, 184.0f, 588.0f, 212.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::SkinFill:
        StrokeRounded(SkinSwatchRect(0), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::SkinBorder:
        StrokeRounded(SkinSwatchRect(1), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::SkinText:
        StrokeRounded(SkinSwatchRect(2), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::SkinImage:
        StrokeRounded(SkinImageButtonRect(), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::SkinImageClear:
        StrokeRounded(SkinImageClearRect(), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::KeySkinClassic:
    case FocusId::KeySkinMinimal:
    case FocusId::KeySkinNeon:
    case FocusId::KeySkinPaper:
    case FocusId::KeySkinTerminal:
    case FocusId::KeySkinMechanical:
    case FocusId::KeySkinWin98:
    {
        const int index = static_cast<int>(id) - static_cast<int>(FocusId::KeySkinClassic);
        StrokeRounded(KeySkinChipRect(index), 9.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::KeyTextAuto:
        StrokeRounded(KeyTextAutoRect(), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::KeyTextColor:
        StrokeRounded(KeyTextColorRect(), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::KeyStyleColor:
        StrokeRounded(KeyStyleColorRect(), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::KeyPositionBottom:
    case FocusId::KeyPositionTop:
    case FocusId::KeyPositionCursor:
    case FocusId::KeyPositionCustom:
    {
        const int index = static_cast<int>(id) - static_cast<int>(FocusId::KeyPositionBottom);
        StrokeRounded(KeyPositionChipRect(index), 9.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::KeyDelay:
        StrokeRounded(Rect(176.0f, 402.0f, 588.0f, 430.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::KeyScale:
        StrokeRounded(Rect(381.0f, 112.0f, 567.0f, 136.0f), 7.0f, focusColor, 1.6f);
        break;
    case FocusId::KeyOpacity:
        StrokeRounded(Rect(381.0f, 140.0f, 567.0f, 164.0f), 7.0f, focusColor, 1.6f);
        break;
    case FocusId::KeyLetters:
        StrokeRounded(Rect(532.0f, 442.0f, 580.0f, 472.0f), 8.0f, focusColor, 1.6f);
        break;
    case FocusId::ClickColorLeft:
        StrokeRounded(ClickColorRect(0), 12.0f, focusColor, 1.6f);
        break;
    case FocusId::ClickColorRight:
        StrokeRounded(ClickColorRect(1), 12.0f, focusColor, 1.6f);
        break;
    case FocusId::ClickRipple:
        StrokeRounded(ClickToggleRect(0), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::ClickSound:
        StrokeRounded(ClickToggleRect(1), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::ClickVolume:
        StrokeRounded(Rect(388.0f, 392.0f, 502.0f, 416.0f), 7.0f, focusColor, 1.6f);
        break;
    case FocusId::ClickPresetDefault:
    case FocusId::ClickPresetSoft:
    case FocusId::ClickPresetCrisp:
    case FocusId::ClickPresetWood:
    case FocusId::ClickPresetSynth:
    {
        const int preset = static_cast<int>(id) - static_cast<int>(FocusId::ClickPresetDefault);
        StrokeRounded(ClickPresetChipRect(preset), 8.0f, focusColor, 1.6f);
        break;
    }
    case FocusId::ClickImport:
        StrokeRounded(ClickImportRect(), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::TimerEnabled:
    case FocusId::TimerAutoStart:
    case FocusId::TimerShowHours:
    case FocusId::TimerResetOnStop:
    {
        const int index = static_cast<int>(id) - static_cast<int>(FocusId::TimerEnabled);
        const D2D1_RECT_F toggle = TimerToggleRect(index);
        StrokeRounded(
            Rect(toggle.left - 4.0f, toggle.top - 4.0f, toggle.right + 4.0f, toggle.bottom + 4.0f),
            9.0f,
            focusColor,
            1.6f);
        break;
    }
    case FocusId::TimerReset:
        StrokeRounded(TimerResetButtonRect(), 9.0f, focusColor, 1.6f);
        break;
    case FocusId::Close:
        StrokeRounded(Rect(554.0f, 14.0f, 584.0f, 44.0f), 8.0f, focusColor, 1.6f);
        break;
    default:
        break;
    }
}

void SettingsWindow::AdvanceColorFocus(bool backwards)
{
    static const ColorFocus order[] = {
        ColorFocus::SaturationValue,
        ColorFocus::Hue,
        ColorFocus::Hex,
        ColorFocus::Cancel,
        ColorFocus::Done,
    };
    int index = 0;
    for (int i = 0; i < 5; ++i)
    {
        if (order[i] == colorFocus_)
        {
            index = i;
            break;
        }
    }
    index = backwards ? (index + 4) % 5 : (index + 1) % 5;
    colorFocus_ = order[index];
    hexFocused_ = colorFocus_ == ColorFocus::Hex;
    hexSelectAll_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool SettingsWindow::HandleColorPickerKey(WPARAM key)
{
    if (!colorPickerOpen_)
        return false;
    if (key == VK_TAB)
    {
        AdvanceColorFocus((GetKeyState(VK_SHIFT) & 0x8000) != 0);
        return true;
    }
    if (key == VK_ESCAPE)
    {
        CancelColorPicker();
        return true;
    }
    if (key == VK_RETURN)
    {
        if (colorFocus_ == ColorFocus::Cancel)
            CancelColorPicker();
        else
            CommitColorPicker();
        return true;
    }
    if (key == VK_SPACE)
    {
        if (colorFocus_ == ColorFocus::Cancel)
            CancelColorPicker();
        else if (colorFocus_ == ColorFocus::Done)
            CommitColorPicker();
        return true;
    }

    if (colorFocus_ == ColorFocus::Hex)
    {
        if (hexFocused_ && HandleHexKey(key))
            return true;
        return key != VK_LEFT && key != VK_RIGHT && key != VK_UP && key != VK_DOWN;
    }

    if (colorFocus_ == ColorFocus::SaturationValue)
    {
        bool changed = false;
        if (key == VK_LEFT)
        {
            colorSaturation_ = (std::max)(0.0f, colorSaturation_ - 0.02f);
            changed = true;
        }
        else if (key == VK_RIGHT)
        {
            colorSaturation_ = (std::min)(1.0f, colorSaturation_ + 0.02f);
            changed = true;
        }
        else if (key == VK_UP)
        {
            colorValue_ = (std::min)(1.0f, colorValue_ + 0.02f);
            changed = true;
        }
        else if (key == VK_DOWN)
        {
            colorValue_ = (std::max)(0.0f, colorValue_ - 0.02f);
            changed = true;
        }
        else if (key == VK_HOME)
        {
            colorSaturation_ = 0.0f;
            changed = true;
        }
        else if (key == VK_END)
        {
            colorSaturation_ = 1.0f;
            changed = true;
        }
        if (changed)
        {
            settings_->accentRgb = HsvToRgb(colorHue_, colorSaturation_, colorValue_);
            SyncHexFromColor();
            if (host_)
                host_->OnSettingsStyleChanged();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        return false;
    }

    if (colorFocus_ == ColorFocus::Hue)
    {
        bool changed = false;
        if (key == VK_LEFT)
        {
            colorHue_ -= 2.0f;
            changed = true;
        }
        else if (key == VK_RIGHT)
        {
            colorHue_ += 2.0f;
            changed = true;
        }
        else if (key == VK_HOME)
        {
            colorHue_ = 0.0f;
            changed = true;
        }
        else if (key == VK_END)
        {
            colorHue_ = 359.0f;
            changed = true;
        }
        if (changed)
        {
            if (colorHue_ < 0.0f)
                colorHue_ += 360.0f;
            if (colorHue_ >= 360.0f)
                colorHue_ -= 360.0f;
            settings_->accentRgb = HsvToRgb(colorHue_, colorSaturation_, colorValue_);
            SyncHexFromColor();
            if (host_)
                host_->OnSettingsStyleChanged();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
    }
    return false;
}

void SettingsWindow::DrawColorFocusRings()
{
    if (!colorPickerOpen_)
        return;
    const D2D1_COLOR_F focusColor = D2D1::ColorF(
        palette_.text.r,
        palette_.text.g,
        palette_.text.b,
        0.90f);
    switch (colorFocus_)
    {
    case ColorFocus::SaturationValue:
    {
        const D2D1_RECT_F rect = SaturationValueRect();
        StrokeRounded(Rect(rect.left - 3.0f, rect.top - 3.0f, rect.right + 3.0f, rect.bottom + 3.0f), 3.0f, focusColor, 1.8f);
        break;
    }
    case ColorFocus::Hue:
    {
        const D2D1_RECT_F rect = HueRect();
        StrokeRounded(Rect(rect.left - 3.0f, rect.top - 3.0f, rect.right + 3.0f, rect.bottom + 3.0f), 10.0f, focusColor, 1.8f);
        break;
    }
    case ColorFocus::Hex:
    {
        const D2D1_RECT_F rect = HexInputRect();
        StrokeRounded(Rect(rect.left - 2.5f, rect.top - 2.5f, rect.right + 2.5f, rect.bottom + 2.5f), 10.0f, focusColor, 1.8f);
        break;
    }
    case ColorFocus::Cancel:
    {
        const D2D1_RECT_F rect = ColorCancelRect();
        StrokeRounded(Rect(rect.left - 2.0f, rect.top - 2.0f, rect.right + 2.0f, rect.bottom + 2.0f), 9.0f, focusColor, 1.6f);
        break;
    }
    case ColorFocus::Done:
    {
        const D2D1_RECT_F rect = ColorDoneRect();
        StrokeRounded(Rect(rect.left - 2.0f, rect.top - 2.0f, rect.right + 2.0f, rect.bottom + 2.0f), 9.0f, palette_.text, 1.6f);
        break;
    }
    }
}

bool SettingsWindow::RegisterWindowClass()
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &SettingsWindow::WndProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_TRAY_ACTIVE), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_SHARED));
    windowClass.hIconSm = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_TRAY_ACTIVE), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED));
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kSettingsClass;

    if (RegisterClassExW(&windowClass))
    {
        classRegistered_ = true;
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool SettingsWindow::CreateTextFormats()
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    if (FAILED(dwriteFactory_->GetSystemFontCollection(fonts.GetAddressOf(), FALSE)) || !fonts)
        return false;

    const wchar_t* candidates[] = {
        L"Microsoft YaHei UI",
        L"Microsoft YaHei",
        L"Segoe UI",
    };
    const wchar_t* family = L"Segoe UI";
    for (const wchar_t* candidate : candidates)
    {
        UINT32 index = 0;
        BOOL exists = FALSE;
        if (SUCCEEDED(fonts->FindFamilyName(candidate, &index, &exists)) && exists)
        {
            family = candidate;
            break;
        }
    }

    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            brandFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            titleFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 12.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            subtitleFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 13.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            bodyFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            bodyStrongFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 11.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            captionFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 10.5f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
            captionCenterFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 13.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
            navFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 12.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            valueRightFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
            centerFormat_.GetAddressOf()))
        return false;
    return true;
}

bool SettingsWindow::EnsureRenderTarget()
{
    if (!hwnd_ || !d2dFactory_)
        return false;
    RECT client = {};
    GetClientRect(hwnd_, &client);
    const UINT width = static_cast<UINT>((std::max)(1L, client.right - client.left));
    const UINT height = static_cast<UINT>((std::max)(1L, client.bottom - client.top));

    if (renderTarget_ && targetWidth_ == width && targetHeight_ == height)
    {
        renderTarget_->SetDpi(96.0f * scale_, 96.0f * scale_);
        return true;
    }

    DiscardRenderTarget();
    const HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(width, height)),
        renderTarget_.GetAddressOf());
    if (FAILED(hr) || !renderTarget_)
        return false;
    renderTarget_->SetDpi(96.0f * scale_, 96.0f * scale_);

    if (FAILED(renderTarget_->CreateSolidColorBrush(Rgb(0xFFFFFF), brush_.GetAddressOf())) || !brush_)
    {
        DiscardRenderTarget();
        return false;
    }

    targetWidth_ = width;
    targetHeight_ = height;
    return true;
}

void SettingsWindow::DiscardRenderTarget()
{
    previewLayer_.Reset();
    brush_.Reset();
    renderTarget_.Reset();
    targetWidth_ = 0;
    targetHeight_ = 0;
}

void SettingsWindow::Draw()
{
    if (!renderTarget_ || !settings_)
        return;

    const D2D1_COLOR_F accent = Rgb(settings_->accentRgb);

    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(palette_.background);

    DrawSidebar(accent);
    DrawHeader(accent);

    switch (page_)
    {
    case Page::Highlight:
        DrawHighlightPage(accent);
        break;
    case Page::Appearance:
        DrawAppearancePage(accent);
        break;
    case Page::Keys:
        DrawKeysPage(accent);
        break;
    case Page::Timer:
        DrawTimerPage(accent);
        break;
    case Page::Click:
        DrawClickPage(accent);
        break;
    case Page::Stack:
        DrawStackPage(accent);
        break;
    case Page::Trail:
        DrawTrailPage(accent);
        break;
    case Page::General:
        DrawGeneralPage(accent);
        break;
    case Page::About:
        DrawAboutPage(accent);
        break;
    }

    DrawCloseButton();
    if (colorPickerOpen_)
        DrawColorPicker();
    else
        DrawFocusRings();
    StrokeRounded(Rect(0.5f, 0.5f, kWidth - 0.5f, kHeight - 0.5f), 0.0f, palette_.border, 1.0f);
}

void SettingsWindow::DrawSidebar(const D2D1_COLOR_F& accent)
{
    const D2D1_COLOR_F activeText = accent;
    FillRect(Rect(0.0f, 0.0f, kSidebarWidth, kHeight), palette_.sidebar);
    StrokeLine(kSidebarWidth - 0.5f, 0.0f, kSidebarWidth - 0.5f, kHeight, palette_.border, 1.0f);

    const D2D1_POINT_2F brandIcon = D2D1::Point2F(27.0f, 31.0f);
    brush_->SetColor(accent);
    renderTarget_->DrawEllipse(
        D2D1::Ellipse(brandIcon, 6.5f, 6.5f),
        brush_.Get(),
        1.5f);
    renderTarget_->FillEllipse(
        D2D1::Ellipse(brandIcon, 1.8f, 1.8f),
        brush_.Get());
    DrawTextBlock(L"ZaYuCursorHalo", brandFormat_.Get(), Rect(44.0f, 19.0f, 154.0f, 43.0f), palette_.text);

    brush_->SetColor(settings_->enabled ? accent : palette_.muted);
    renderTarget_->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(27.0f, 57.0f), 3.0f, 3.0f),
        brush_.Get());
    DrawTextBlock(
        settings_->enabled ? L"高亮已开启" : L"高亮已暂停",
        captionFormat_.Get(),
        Rect(36.0f, 48.0f, 148.0f, 67.0f),
        palette_.muted);

    for (int i = 0; i < kPageCount; ++i)
    {
        const D2D1_RECT_F rect = NavRect(i);
        const bool active = static_cast<int>(page_) == i;
        if (active)
        {
            FillRounded(rect, 8.0f, Blend(palette_.sidebar, accent, darkMode_ ? 0.22f : 0.11f));
        }
        else if (hoverNav_ == i)
        {
            FillRounded(rect, 8.0f, palette_.controlHover);
        }

        const D2D1_COLOR_F iconColor = active ? accent : palette_.muted;
        const int iconCodes[9] = {0, 1, 2, 4, 3, 5, 8, 7, 9};
        DrawLineIcon(iconCodes[i], D2D1::Point2F(28.0f, rect.top + kNavH / 2.0f), iconColor);
        DrawTextBlock(
            kNavLabels[i],
            navFormat_.Get(),
            Rect(48.0f, rect.top, 138.0f, rect.bottom),
            active ? activeText : palette_.muted);
    }

    DrawTextBlock(L"v0.3.0 · P5", captionFormat_.Get(), Rect(24.0f, 512.0f, 150.0f, 530.0f), palette_.subtle);
    DrawTextBlock(
        L"three-lei × DeepSeek",
        captionFormat_.Get(),
        Rect(24.0f, 530.0f, 156.0f, 548.0f),
        palette_.subtle);
}

void SettingsWindow::DrawHeader(const D2D1_COLOR_F& accent)
{
    (void)accent;
    DrawTextBlock(kPageTitles[static_cast<int>(page_)], titleFormat_.Get(), Rect(kContentX, 22.0f, 550.0f, 48.0f), palette_.text);
    DrawTextBlock(kPageSubtitles[static_cast<int>(page_)], subtitleFormat_.Get(), Rect(kContentX, 53.0f, 550.0f, 72.0f), palette_.muted);
}

void SettingsWindow::DrawPreview(const D2D1_COLOR_F& accent)
{
    const D2D1_RECT_F card = Rect(kContentX, kPreviewTop, kContentX + kContentW, kPreviewBottom);
    FillRounded(card, 12.0f, palette_.surfaceAlt);
    StrokeRounded(card, 12.0f, palette_.border, 1.0f);

    // A neutral mock app window provides a familiar scale reference. The
    // halo below is drawn at its real DIP size, not a proportional miniature.
    const D2D1_RECT_F mock = Rect(
        kContentX + 28.0f,
        kPreviewTop + 40.0f,
        kContentX + kContentW - 24.0f,
        kPreviewBottom - 30.0f);
    FillRounded(mock, 10.0f, palette_.surface);
    StrokeRounded(mock, 10.0f, palette_.border, 1.0f);
    FillRounded(Rect(mock.left + 12.0f, mock.top + 8.0f, mock.right - 12.0f, mock.top + 22.0f), 6.0f, palette_.control);
    const D2D1_COLOR_F lineColor = palette_.divider;
    FillRounded(Rect(mock.left + 14.0f, mock.top + 40.0f, mock.left + 132.0f, mock.top + 46.0f), 3.0f, lineColor);
    FillRounded(Rect(mock.left + 14.0f, mock.top + 56.0f, mock.left + 102.0f, mock.top + 62.0f), 3.0f, lineColor);
    FillRounded(Rect(mock.left + 14.0f, mock.top + 72.0f, mock.left + 142.0f, mock.top + 78.0f), 3.0f, lineColor);
    FillRounded(Rect(mock.right - 58.0f, mock.bottom - 34.0f, mock.right - 14.0f, mock.bottom - 18.0f), 7.0f, palette_.controlHover);
    for (int i = 0; i < 3; ++i)
    {
        brush_->SetColor(palette_.divider);
        renderTarget_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(mock.left + 22.0f + i * 8.0f, mock.top + 15.0f), 2.0f, 2.0f),
            brush_.Get());
    }

    DrawTextBlock(L"1:1 实际大小", captionFormat_.Get(), Rect(kContentX + 16.0f, kPreviewTop + 10.0f, 300.0f, kPreviewTop + 28.0f), palette_.muted);
    wchar_t diameter[32] = {};
    swprintf_s(diameter, L"直径 %d px", settings_->radiusDip * 2);
    DrawTextBlock(
        diameter,
        valueRightFormat_.Get(),
        Rect(kContentX + kContentW - 160.0f, kPreviewTop + 10.0f, kContentX + kContentW - 16.0f, kPreviewTop + 28.0f),
        palette_.muted);

    const float opacity = (std::max)(0.30f, (std::min)(1.0f, static_cast<float>(settings_->opacityPercent) / 100.0f));
    const float radius = static_cast<float>(settings_->radiusDip);
    const D2D1_POINT_2F center = D2D1::Point2F(kContentX + kContentW / 2.0f, (kPreviewTop + kPreviewBottom) / 2.0f);

    auto ring = [&](float offset, float thickness, float alpha) {
        brush_->SetColor(D2D1::ColorF(accent.r, accent.g, accent.b, alpha * opacity));
        renderTarget_->DrawEllipse(
            D2D1::Ellipse(center, radius + offset, radius + offset),
            brush_.Get(),
            thickness);
    };
    const HaloStyle style = ParseHaloStyle(settings_->haloStyle);
    if (style == HaloStyle::Dot)
    {
        brush_->SetColor(D2D1::ColorF(accent.r, accent.g, accent.b, 0.10f * opacity));
        renderTarget_->FillEllipse(
            D2D1::Ellipse(center, radius + 9.0f, radius + 9.0f),
            brush_.Get());
        ring(0.0f, 1.4f, 0.78f);
        brush_->SetColor(D2D1::ColorF(accent.r, accent.g, accent.b, 0.92f * opacity));
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 3.8f, 3.8f), brush_.Get());
        brush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.62f * opacity));
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 1.5f, 1.5f), brush_.Get());
    }
    else
    {
        ring(8.0f, 14.0f, 0.045f);
        ring(3.0f, 6.0f, 0.10f);
        brush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.35f * opacity));
        renderTarget_->DrawEllipse(
            D2D1::Ellipse(center, radius + 1.2f, radius + 1.2f),
            brush_.Get(),
            1.4f);
        ring(0.0f, 2.0f, 0.95f);
        if (style == HaloStyle::Double)
        {
            ring(-10.0f, 1.6f, 0.56f);
            ring(-17.0f, 1.0f, 0.22f);
        }
        else if (style == HaloStyle::Crosshair)
        {
            const auto tick = [&](float x1, float y1, float x2, float y2) {
                brush_->SetColor(D2D1::ColorF(accent.r, accent.g, accent.b, 0.82f * opacity));
                renderTarget_->DrawLine(
                    D2D1::Point2F(center.x + x1, center.y + y1),
                    D2D1::Point2F(center.x + x2, center.y + y2),
                    brush_.Get(),
                    2.2f);
            };
            const float inner = radius + 5.0f;
            const float outer = radius + 16.0f;
            tick(0.0f, -inner, 0.0f, -outer);
            tick(0.0f, inner, 0.0f, outer);
            tick(-inner, 0.0f, -outer, 0.0f);
            tick(inner, 0.0f, outer, 0.0f);
            brush_->SetColor(D2D1::ColorF(accent.r, accent.g, accent.b, 0.9f * opacity));
            renderTarget_->FillEllipse(D2D1::Ellipse(center, 2.4f, 2.4f), brush_.Get());
        }
        brush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.38f * opacity));
        renderTarget_->DrawEllipse(
            D2D1::Ellipse(center, radius - 2.4f, radius - 2.4f),
            brush_.Get(),
            1.0f);
    }

    if (cursorGeometry_)
    {
        brush_->SetColor(Rgb(0xFFFFFF));
        renderTarget_->SetTransform(D2D1::Matrix3x2F::Translation(center.x, center.y));
        renderTarget_->FillGeometry(cursorGeometry_.Get(), brush_.Get());
        brush_->SetColor(Rgb(0x292B30, 0.85f));
        renderTarget_->DrawGeometry(cursorGeometry_.Get(), brush_.Get(), 0.9f);
        renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    }
}

void SettingsWindow::DrawToggle(const D2D1_RECT_F& rect, bool on, const D2D1_COLOR_F& accent)
{
    const float radius = (rect.bottom - rect.top) / 2.0f;
    FillRounded(rect, radius, on ? accent : palette_.switchOff);
    const float knobRadius = radius - 4.0f;
    const float centerX = on ? rect.right - radius : rect.left + radius;
    brush_->SetColor(palette_.onSurface);
    renderTarget_->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(centerX, (rect.top + rect.bottom) / 2.0f), knobRadius, knobRadius),
        brush_.Get());
}

void SettingsWindow::DrawSlider(
    const wchar_t* label,
    const std::wstring& value,
    float labelY,
    float trackY,
    float fraction,
    const D2D1_COLOR_F& accent)
{
    DrawTextBlock(label, bodyStrongFormat_.Get(), Rect(kContentX, labelY, 400.0f, labelY + 22.0f), palette_.text);
    DrawTextBlock(value.c_str(), valueRightFormat_.Get(), Rect(kContentX, labelY, kContentX + kContentW, labelY + 22.0f), palette_.muted);

    const D2D1_RECT_F track = Rect(kContentX, trackY, kContentX + kContentW, trackY + 4.0f);
    FillRounded(track, 2.0f, palette_.track);
    const float clamped = Clamp01(fraction);
    if (clamped > 0.0f)
    {
        FillRounded(
            Rect(track.left, track.top, track.left + kContentW * clamped, track.bottom),
            2.0f,
            accent);
    }

    const D2D1_POINT_2F knob = D2D1::Point2F(track.left + kContentW * clamped, trackY + 2.0f);
    brush_->SetColor(palette_.onSurface);
    renderTarget_->FillEllipse(D2D1::Ellipse(knob, 8.0f, 8.0f), brush_.Get());
    brush_->SetColor(accent);
    renderTarget_->DrawEllipse(D2D1::Ellipse(knob, 8.0f, 8.0f), brush_.Get(), 2.4f);
}

void SettingsWindow::DrawHighlightPage(const D2D1_COLOR_F& accent)
{
    (void)accent;

    DrawPreview(accent);

    DrawTextBlock(L"光环样式", bodyStrongFormat_.Get(), Rect(kContentX, 244.0f, 400.0f, 266.0f), palette_.text);
    const wchar_t* styleLabels[4] = {L"光环", L"双环", L"准星", L"光点"};
    const int styleIndex = static_cast<int>(ParseHaloStyle(settings_->haloStyle));
    for (int i = 0; i < 4; ++i)
    {
        const D2D1_RECT_F chip = HaloStyleChipRect(i);
        const bool selected = styleIndex == i;
        FillRounded(chip, 9.0f, selected ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
        StrokeRounded(
            chip,
            9.0f,
            selected ? accent : palette_.border,
            selected ? 1.4f : 1.0f);
        DrawTextBlock(
            styleLabels[i],
            captionCenterFormat_.Get(),
            chip,
            selected ? palette_.text : palette_.muted);
    }

    DrawTextBlock(L"启用高亮", bodyStrongFormat_.Get(), Rect(kContentX, 334.0f, 470.0f, 356.0f), palette_.text);
    DrawTextBlock(L"关闭后暂停跟随与托盘状态", captionFormat_.Get(), Rect(kContentX, 356.0f, 500.0f, 374.0f), palette_.muted);
    DrawToggle(Rect(536.0f, 338.0f, 576.0f, 360.0f), settings_->enabled, accent);

    StrokeLine(kContentX, 384.0f, kContentX + kContentW, 384.0f, palette_.divider, 1.0f);

    wchar_t value[32] = {};
    swprintf_s(value, L"直径 %d px", settings_->radiusDip * 2);
    const float radiusFraction = (static_cast<float>(settings_->radiusDip) - 20.0f) / 60.0f;
    DrawSlider(L"光环大小", value, 400.0f, 428.0f, radiusFraction, accent);

    swprintf_s(value, L"%d%%", settings_->opacityPercent);
    const float opacityFraction = (static_cast<float>(settings_->opacityPercent) - 30.0f) / 70.0f;
    DrawSlider(L"不透明度", value, 452.0f, 480.0f, opacityFraction, accent);

    DrawTextBlock(L"修改会立即应用并自动保存", captionFormat_.Get(), Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f), palette_.subtle);
}

void SettingsWindow::DrawAppearancePage(const D2D1_COLOR_F& accent)
{
    (void)accent;
    DrawPreview(accent);

    DrawTextBlock(L"光环颜色", bodyStrongFormat_.Get(), Rect(kContentX, 334.0f, 400.0f, 356.0f), palette_.text);
    DrawTextBlock(L"点击色块即时切换", captionFormat_.Get(), Rect(kContentX, 356.0f, 500.0f, 374.0f), palette_.muted);

    for (int i = 0; i < kColorCount; ++i)
    {
        const float centerX = kSwatchStartX + kSwatchGap * i;
        const D2D1_POINT_2F center = D2D1::Point2F(centerX, kSwatchY);
        const D2D1_COLOR_F color = Rgb(kColors[i]);
        const bool selected = settings_->accentRgb == kColors[i];

        if (selected)
        {
            brush_->SetColor(D2D1::ColorF(palette_.text.r, palette_.text.g, palette_.text.b, 0.75f));
            renderTarget_->DrawEllipse(D2D1::Ellipse(center, 20.0f, 20.0f), brush_.Get(), 1.4f);
        }
        else if (InRect(mouse_.x, mouse_.y, Rect(centerX - 20.0f, kSwatchY - 20.0f, centerX + 20.0f, kSwatchY + 20.0f)))
        {
            brush_->SetColor(D2D1::ColorF(color.r, color.g, color.b, 0.35f));
            renderTarget_->DrawEllipse(D2D1::Ellipse(center, 19.0f, 19.0f), brush_.Get(), 1.2f);
        }

        brush_->SetColor(color);
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 14.0f, 14.0f), brush_.Get());
    }

    const D2D1_POINT_2F customCenter = D2D1::Point2F(kCustomSwatchX, kSwatchY);
    const bool customSelected = !IsPresetColor(settings_->accentRgb);
    if (customSelected)
    {
        brush_->SetColor(D2D1::ColorF(palette_.text.r, palette_.text.g, palette_.text.b, 0.75f));
        renderTarget_->DrawEllipse(D2D1::Ellipse(customCenter, 20.0f, 20.0f), brush_.Get(), 1.4f);
        brush_->SetColor(Rgb(settings_->accentRgb));
        renderTarget_->FillEllipse(D2D1::Ellipse(customCenter, 14.0f, 14.0f), brush_.Get());
    }
    else
    {
        brush_->SetColor(palette_.surface);
        renderTarget_->FillEllipse(D2D1::Ellipse(customCenter, 14.0f, 14.0f), brush_.Get());
        brush_->SetColor(palette_.border);
        renderTarget_->DrawEllipse(D2D1::Ellipse(customCenter, 14.0f, 14.0f), brush_.Get(), 1.2f);
    }

    if (!customSelected && InRect(mouse_.x, mouse_.y, Rect(kCustomSwatchX - 20.0f, kSwatchY - 20.0f, kCustomSwatchX + 20.0f, kSwatchY + 20.0f)))
    {
        brush_->SetColor(D2D1::ColorF(accent.r, accent.g, accent.b, 0.35f));
        renderTarget_->DrawEllipse(D2D1::Ellipse(customCenter, 19.0f, 19.0f), brush_.Get(), 1.2f);
    }

    const D2D1_COLOR_F customMark = customSelected
        ? Rgb(IsLightColor(settings_->accentRgb) ? 0x1B1C1F : 0xFFFFFF)
        : palette_.muted;
    brush_->SetColor(customMark);
    renderTarget_->DrawLine(
        D2D1::Point2F(customCenter.x - 4.5f, customCenter.y),
        D2D1::Point2F(customCenter.x + 4.5f, customCenter.y),
        brush_.Get(),
        1.3f);
    renderTarget_->DrawLine(
        D2D1::Point2F(customCenter.x, customCenter.y - 4.5f),
        D2D1::Point2F(customCenter.x, customCenter.y + 4.5f),
        brush_.Get(),
        1.3f);
    DrawTextBlock(L"自定义", captionCenterFormat_.Get(), Rect(kCustomSwatchX - 30.0f, 414.0f, kCustomSwatchX + 30.0f, 432.0f), palette_.muted);

    DrawTextBlock(L"颜色会同时作用于实际光环和预览", captionFormat_.Get(), Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f), palette_.subtle);
}

void SettingsWindow::DrawKeycapPreview(
    const D2D1_RECT_F& rect,
    const wchar_t* label,
    bool modifier,
    KeySkin skin,
    const D2D1_COLOR_F& accent)
{
    if (!settings_)
        return;

    D2D1_COLOR_F textColor = palette_.text;
    if (!settings_->keyTextAuto)
    {
        textColor = Rgb(settings_->keyTextRgb);
    }
    else if (skin == KeySkin::Neon)
    {
        textColor = Rgb(0xF5F7FF);
    }
    else if (skin == KeySkin::Paper)
    {
        textColor = Rgb(0x3B2A1A);
    }
    else if (skin == KeySkin::Terminal)
    {
        textColor = Rgb(0x6BFF8E);
    }
    else if (skin == KeySkin::Mechanical)
    {
        textColor = Rgb(0xF2F2F2);
    }
    else if (skin == KeySkin::Win98)
    {
        textColor = Rgb(0x0B0B0B);
    }

    if (skin == KeySkin::Minimal)
    {
        D2D1_COLOR_F fill = Rgb(
            settings_->keyStyleAuto ? settings_->keyFillRgb : settings_->keyStyleRgb,
            0.94f);
        if (modifier)
        {
            fill.r *= 0.92f;
            fill.g *= 0.92f;
            fill.b *= 0.92f;
        }
        FillRounded(rect, 7.0f, fill);
        StrokeRounded(
            rect,
            7.0f,
            Rgb(settings_->keyStyleAuto
                ? settings_->keyBorderRgb
                : DerivedKeyBorder(settings_->keyStyleRgb)),
            1.0f);
        DrawTextBlock(label, centerFormat_.Get(), rect, textColor);
        return;
    }

    if (skin == KeySkin::Neon)
    {
        FillRounded(
            Rect(rect.left - 2.0f, rect.top - 2.0f, rect.right + 2.0f, rect.bottom + 2.0f),
            9.0f,
            D2D1::ColorF(accent.r, accent.g, accent.b, 0.18f));
        FillRounded(rect, 7.0f, Rgb(0x0D0F14, 0.92f));
        StrokeRounded(rect, 7.0f, D2D1::ColorF(accent.r, accent.g, accent.b, 0.95f), 1.4f);
        DrawTextBlock(label, centerFormat_.Get(), rect, textColor);
        return;
    }

    if (skin == KeySkin::Paper)
    {
        FillRounded(
            Rect(rect.left + 1.0f, rect.top + 2.0f, rect.right + 1.0f, rect.bottom + 2.0f),
            7.0f,
            Rgb(0x5A3D1E, 0.12f));
        FillRounded(rect, 7.0f, modifier ? Rgb(0xE8D9BC) : Rgb(0xFAF2E2));
        StrokeRounded(rect, 7.0f, Rgb(0xB89763), 1.0f);
        DrawTextBlock(label, centerFormat_.Get(), rect, textColor);
        return;
    }

    if (skin == KeySkin::Terminal)
    {
        FillRounded(
            Rect(rect.left - 2.0f, rect.top - 2.0f, rect.right + 2.0f, rect.bottom + 2.0f),
            9.0f,
            Rgb(0x33F06B, 0.12f));
        FillRounded(rect, 7.0f, Rgb(0x08100A, 0.96f));
        StrokeRounded(rect, 7.0f, Rgb(0x40D466), 1.2f);
        DrawTextBlock(label, centerFormat_.Get(), rect, textColor);
        return;
    }

    if (skin == KeySkin::Mechanical)
    {
        FillRounded(
            Rect(rect.left + 1.0f, rect.top + 3.0f, rect.right + 1.0f, rect.bottom + 3.0f),
            6.0f,
            Rgb(0x000000, 0.42f));
        FillRounded(rect, 6.0f, Rgb(0x1A1B1F));
        const D2D1_RECT_F face = Rect(
            rect.left + 2.0f,
            rect.top + 2.0f,
            rect.right - 2.0f,
            rect.bottom - 3.0f);
        FillRounded(face, 4.0f, modifier ? Rgb(0x33363D) : Rgb(0x292C32));
        StrokeRounded(face, 4.0f, Rgb(0x8F949E, 0.42f), 1.0f);
        DrawTextBlock(label, centerFormat_.Get(), face, textColor);
        return;
    }

    if (skin == KeySkin::Win98)
    {
        FillRounded(rect, 0.0f, Rgb(0xC2C2C2));
        StrokeRounded(rect, 0.0f, Rgb(0x333333), 1.0f);
        brush_->SetColor(Rgb(0xFFFFFF));
        renderTarget_->DrawLine(
            D2D1::Point2F(rect.left + 1.0f, rect.top + 1.0f),
            D2D1::Point2F(rect.right - 1.0f, rect.top + 1.0f),
            brush_.Get(),
            1.0f);
        renderTarget_->DrawLine(
            D2D1::Point2F(rect.left + 1.0f, rect.top + 1.0f),
            D2D1::Point2F(rect.left + 1.0f, rect.bottom - 1.0f),
            brush_.Get(),
            1.0f);
        brush_->SetColor(Rgb(0x555555));
        renderTarget_->DrawLine(
            D2D1::Point2F(rect.left + 1.0f, rect.bottom - 1.0f),
            D2D1::Point2F(rect.right - 1.0f, rect.bottom - 1.0f),
            brush_.Get(),
            1.0f);
        renderTarget_->DrawLine(
            D2D1::Point2F(rect.right - 1.0f, rect.top + 1.0f),
            D2D1::Point2F(rect.right - 1.0f, rect.bottom - 1.0f),
            brush_.Get(),
            1.0f);
        DrawTextBlock(label, centerFormat_.Get(), rect, textColor);
        return;
    }

    FillRounded(
        Rect(rect.left + 1.0f, rect.top + 2.0f, rect.right + 1.0f, rect.bottom + 2.0f),
        7.0f,
        Rgb(0x000000, darkMode_ ? 0.26f : 0.16f));
    FillRounded(
        rect,
        7.0f,
        modifier ? palette_.control : palette_.surface);
    StrokeRounded(rect, 7.0f, palette_.border, 1.0f);
    DrawTextBlock(label, centerFormat_.Get(), rect, textColor);
}

void SettingsWindow::DrawKeyPreviewCaps(const D2D1_COLOR_F& accent)
{
    if (!settings_)
        return;

    const D2D1_RECT_F card = Rect(kContentX, 84.0f, kContentX + kContentW, 225.0f);
    const KeySkin skin = ParseKeySkin(settings_->keySkin);
    const KeyPosition position = ParseKeyPosition(settings_->keyPosition);
    constexpr float capHeight = 28.0f;
    constexpr float separator = 20.0f;
    const float widths[2] = {54.0f, 34.0f};
    const wchar_t* labels[2] = {L"Ctrl", L"S"};
    const float total = widths[0] + widths[1] + separator;
    const float factor =
        static_cast<float>((std::max)(70, (std::min)(150, settings_->keyScalePercent))) / 100.0f;
    const D2D1_POINT_2F anchor =
        D2D1::Point2F((card.left + card.right) / 2.0f, card.bottom - 16.0f);
    const D2D1::Matrix3x2F previewTransform =
        D2D1::Matrix3x2F::Translation(-anchor.x, -anchor.y) *
        D2D1::Matrix3x2F::Scale(factor, factor) *
        D2D1::Matrix3x2F::Translation(anchor.x, anchor.y);
    renderTarget_->SetTransform(previewTransform);

    bool layerPushed = false;
    const float previewOpacity = static_cast<float>(
        (std::max)(20, (std::min)(100, settings_->keyOpacityPercent))) / 100.0f;
    if (previewOpacity < 0.995f)
    {
        if (!previewLayer_)
            renderTarget_->CreateLayer(previewLayer_.GetAddressOf());
        if (previewLayer_)
        {
            D2D1_LAYER_PARAMETERS parameters = D2D1::LayerParameters();
            parameters.opacity = previewOpacity;
            renderTarget_->PushLayer(parameters, previewLayer_.Get());
            layerPushed = true;
        }
    }

    float left = (card.left + card.right) / 2.0f - total / 2.0f;
    const float top = card.bottom - 16.0f - capHeight;
    if (position == KeyPosition::FollowCursor || position == KeyPosition::Custom)
    {
        left += 18.0f;
        if (position == KeyPosition::FollowCursor && cursorGeometry_)
        {
            renderTarget_->SetTransform(
                previewTransform *
                D2D1::Matrix3x2F::Translation(left - 28.0f, top + 3.0f) *
                D2D1::Matrix3x2F::Scale(0.72f, 0.72f));
            brush_->SetColor(Rgb(0xFFFFFF));
            renderTarget_->FillGeometry(cursorGeometry_.Get(), brush_.Get());
            brush_->SetColor(Rgb(0x2A2C31, 0.88f));
            renderTarget_->DrawGeometry(cursorGeometry_.Get(), brush_.Get(), 1.0f);
            renderTarget_->SetTransform(previewTransform);
        }
        else
        {
            brush_->SetColor(accent);
            renderTarget_->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(left - 18.0f, top + 14.0f), 3.0f, 3.0f),
                brush_.Get());
        }
    }

    DrawKeycapPreview(Rect(left, top, left + widths[0], top + capHeight), labels[0], true, skin, accent);

    const D2D1_POINT_2F plus = D2D1::Point2F(
        left + widths[0] + separator / 2.0f,
        top + capHeight / 2.0f);
    D2D1_COLOR_F plusColor = palette_.muted;
    if (skin == KeySkin::Neon)
        plusColor = accent;
    else if (skin == KeySkin::Paper)
        plusColor = Rgb(0x6A4B28);
    else if (skin == KeySkin::Terminal)
        plusColor = Rgb(0x6BFF8E);
    else if (skin == KeySkin::Mechanical)
        plusColor = Rgb(0xF2F2F2);
    else if (skin == KeySkin::Win98)
        plusColor = Rgb(0x111111);
    if (settings_ && !settings_->keyTextAuto)
        plusColor = Rgb(settings_->keyTextRgb);
    brush_->SetColor(plusColor);
    renderTarget_->DrawLine(
        D2D1::Point2F(plus.x - 4.0f, plus.y),
        D2D1::Point2F(plus.x + 4.0f, plus.y),
        brush_.Get(),
        1.3f);
    renderTarget_->DrawLine(
        D2D1::Point2F(plus.x, plus.y - 4.0f),
        D2D1::Point2F(plus.x, plus.y + 4.0f),
        brush_.Get(),
        1.3f);

    DrawKeycapPreview(
        Rect(left + widths[0] + separator, top, left + widths[0] + separator + widths[1], top + capHeight),
        labels[1],
        false,
        skin,
        accent);
    if (layerPushed)
        renderTarget_->PopLayer();
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
}

void SettingsWindow::DrawPreviewSlider(
    const wchar_t* label,
    const std::wstring& value,
    float centerY,
    float fraction,
    const D2D1_RECT_F& track,
    const D2D1_COLOR_F& accent)
{
    DrawTextBlock(
        label,
        captionFormat_.Get(),
        Rect(204.0f, centerY - 9.0f, 300.0f, centerY + 9.0f),
        palette_.muted);
    DrawTextBlock(
        value.c_str(),
        valueRightFormat_.Get(),
        Rect(300.0f, centerY - 9.0f, 378.0f, centerY + 9.0f),
        palette_.subtle);

    const float y = (track.top + track.bottom) / 2.0f;
    FillRounded(
        Rect(track.left, y - 2.0f, track.right, y + 2.0f),
        2.0f,
        palette_.divider);
    const float x = track.left + (track.right - track.left) * Clamp01(fraction);
    FillRounded(
        Rect(track.left, y - 2.0f, x, y + 2.0f),
        2.0f,
        accent);
    brush_->SetColor(accent);
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 7.0f, 7.0f), brush_.Get());
    brush_->SetColor(Rgb(0xFFFFFF, 0.85f));
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 2.2f, 2.2f), brush_.Get());
}

void SettingsWindow::DrawKeysPage(const D2D1_COLOR_F& accent)
{
    const D2D1_RECT_F preview = Rect(kContentX, 84.0f, kContentX + kContentW, 225.0f);
    FillRounded(preview, 12.0f, palette_.surfaceAlt);
    StrokeRounded(preview, 12.0f, palette_.border, 1.0f);
    DrawTextBlock(
        L"键帽预览",
        captionFormat_.Get(),
        Rect(kContentX + 16.0f, 94.0f, 320.0f, 112.0f),
        palette_.muted);

    const wchar_t* positionLabels[4] = {L"底部居中", L"顶部居中", L"跟随光标", L"自定义"};
    const KeyPosition position = ParseKeyPosition(settings_->keyPosition);
    DrawTextBlock(
        positionLabels[static_cast<int>(position)],
        valueRightFormat_.Get(),
        Rect(kContentX + 260.0f, 94.0f, kContentX + kContentW - 16.0f, 112.0f),
        palette_.subtle);

    const float scaleFraction =
        static_cast<float>(settings_->keyScalePercent - 70) / 80.0f;
    const float opacityFraction =
        static_cast<float>(settings_->keyOpacityPercent - 20) / 80.0f;
    DrawPreviewSlider(
        L"缩放",
        std::to_wstring(settings_->keyScalePercent) + L"%",
        124.0f,
        scaleFraction,
        KeyScaleTrackRect(),
        accent);
    DrawPreviewSlider(
        L"不透明度",
        std::to_wstring(settings_->keyOpacityPercent) + L"%",
        152.0f,
        opacityFraction,
        KeyOpacityTrackRect(),
        accent);

    DrawKeyPreviewCaps(accent);

    DrawTextBlock(L"皮肤", bodyStrongFormat_.Get(), Rect(kContentX, 244.0f, 300.0f, 266.0f), palette_.text);
    const wchar_t* skinLabels[4] = {L"经典", L"极简", L"机械", L"Win98"};
    constexpr int skinOrder[4] = {0, 1, 5, 6};
    const KeySkin skin = ParseKeySkin(settings_->keySkin);
    for (int i = 0; i < 4; ++i)
    {
        const D2D1_RECT_F chip = KeySkinChipRect(i);
        const bool selected = static_cast<int>(skin) == skinOrder[i];
        FillRounded(chip, 9.0f, selected ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
        StrokeRounded(
            chip,
            9.0f,
            selected ? accent : palette_.border,
            selected ? 1.4f : 1.0f);
        DrawTextBlock(
            skinLabels[i],
            captionCenterFormat_.Get(),
            chip,
            selected ? palette_.text : palette_.muted);
    }

    DrawTextBlock(L"位置", bodyStrongFormat_.Get(), Rect(kContentX, 318.0f, 300.0f, 340.0f), palette_.text);
    for (int i = 0; i < 4; ++i)
    {
        const D2D1_RECT_F chip = KeyPositionChipRect(i);
        const bool selected = static_cast<int>(position) == i;
        FillRounded(chip, 9.0f, selected ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
        StrokeRounded(
            chip,
            9.0f,
            selected ? accent : palette_.border,
            selected ? 1.4f : 1.0f);
        DrawTextBlock(
            positionLabels[i],
            captionCenterFormat_.Get(),
            chip,
            selected ? palette_.text : palette_.muted);
    }

    const float delayFraction =
        static_cast<float>(settings_->keyDelayMs - kMinKeyDelayMs) /
        static_cast<float>(kMaxKeyDelayMs - kMinKeyDelayMs);
    DrawSlider(
        L"停留延迟",
        std::to_wstring(settings_->keyDelayMs) + L" ms",
        388.0f,
        414.0f,
        delayFraction,
        accent);

    DrawTextBlock(L"显示普通字母键", bodyStrongFormat_.Get(), Rect(kContentX, 442.0f, 470.0f, 464.0f), palette_.text);
    DrawTextBlock(
        L"关闭时只显示功能键和组合键",
        captionFormat_.Get(),
        Rect(kContentX, 462.0f, 500.0f, 480.0f),
        palette_.muted);
    DrawToggle(KeyLettersToggleRect(), settings_->showLetterKeys, accent);

    DrawTextBlock(L"文字 / 主色", bodyStrongFormat_.Get(), Rect(kContentX, 480.0f, 360.0f, 498.0f), palette_.text);
    DrawTextBlock(
        L"左文字色 / 右极简主色，右键恢复自动",
        captionFormat_.Get(),
        Rect(kContentX, 500.0f, 512.0f, 518.0f),
        palette_.muted);

    const D2D1_RECT_F autoRect = KeyTextAutoRect();
    FillRounded(autoRect, 8.0f, palette_.control);
    StrokeRounded(
        autoRect,
        8.0f,
        settings_->keyTextAuto ? accent : palette_.border,
        settings_->keyTextAuto ? 1.4f : 1.0f);
    DrawTextBlock(
        L"自动",
        captionCenterFormat_.Get(),
        autoRect,
        settings_->keyTextAuto ? palette_.text : palette_.muted);

    const D2D1_RECT_F textColorRect = KeyTextColorRect();
    FillRounded(
        textColorRect,
        8.0f,
        settings_->keyTextAuto ? palette_.surface : Rgb(settings_->keyTextRgb));
    StrokeRounded(
        textColorRect,
        8.0f,
        settings_->keyTextAuto ? palette_.border : accent,
        settings_->keyTextAuto ? 1.0f : 1.4f);
    if (settings_->keyTextAuto)
    {
        brush_->SetColor(palette_.muted);
        renderTarget_->DrawLine(
            D2D1::Point2F(textColorRect.left + 8.0f, textColorRect.bottom - 8.0f),
            D2D1::Point2F(textColorRect.right - 8.0f, textColorRect.top + 8.0f),
            brush_.Get(),
            1.2f);
    }

    const D2D1_RECT_F styleColorRect = KeyStyleColorRect();
    FillRounded(
        styleColorRect,
        8.0f,
        settings_->keyStyleAuto ? palette_.surface : Rgb(settings_->keyStyleRgb));
    StrokeRounded(
        styleColorRect,
        8.0f,
        settings_->keyStyleAuto ? palette_.border : accent,
        settings_->keyStyleAuto ? 1.0f : 1.4f);
    if (settings_->keyStyleAuto)
    {
        brush_->SetColor(palette_.muted);
        renderTarget_->DrawLine(
            D2D1::Point2F(styleColorRect.left + 8.0f, styleColorRect.bottom - 8.0f),
            D2D1::Point2F(styleColorRect.right - 8.0f, styleColorRect.top + 8.0f),
            brush_.Get(),
            1.2f);
    }

    DrawTextBlock(
        L"按键显示不记录内容，可在通用页完全关闭",
        captionFormat_.Get(),
        Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f),
        palette_.subtle);
}

void SettingsWindow::DrawTimerPage(const D2D1_COLOR_F& accent)
{
    const wchar_t* labels[4] = {
        L"启用计时器",
        L"启动时自动开始",
        L"显示小时位",
        L"停止时归零",
    };
    const TimerMode timerMode = ParseTimerMode(settings_->timerMode);
    std::wstring autoCaption = L"应用启动后自动从 00:00:00 开始计时";
    if (timerMode == TimerMode::CountDown)
    {
        wchar_t text[64] = {};
        swprintf_s(text, L"应用启动后自动从 %d:00 开始倒计时", settings_->timerCountdownMinutes);
        autoCaption = text;
    }
    const wchar_t* captions[4] = {
        L"在桌面浮动显示；不会覆盖任务栏或托盘图标",
        autoCaption.c_str(),
        L"使用 HH:MM:SS；关闭后使用 MM:SS",
        L"关闭时停止后保留时间，再次点击停止可归零",
    };
    const bool values[4] = {
        settings_->timerEnabled,
        settings_->timerAutoStart,
        settings_->timerShowHours,
        settings_->timerResetOnStop,
    };

    for (int i = 0; i < 4; ++i)
    {
        const float top = 84.0f + static_cast<float>(i) * 72.0f;
        if (i > 0)
            StrokeLine(kContentX, top - 12.0f, kContentX + kContentW, top - 12.0f, palette_.border, 1.0f);
        DrawTextBlock(labels[i], bodyStrongFormat_.Get(), Rect(kContentX, top, 480.0f, top + 22.0f), palette_.text);
        DrawTextBlock(captions[i], captionFormat_.Get(), Rect(kContentX, top + 22.0f, 510.0f, top + 42.0f), palette_.muted);
        DrawToggle(TimerToggleRect(i), values[i], accent);
    }

    StrokeLine(kContentX, 354.0f, kContentX + kContentW, 354.0f, palette_.border, 1.0f);
    DrawTextBlock(L"计时归零", bodyStrongFormat_.Get(), Rect(kContentX, 374.0f, 420.0f, 396.0f), palette_.text);
    DrawTextBlock(
        L"停止计时并把浮动计时器恢复为 00:00:00",
        captionFormat_.Get(),
        Rect(kContentX, 396.0f, 468.0f, 416.0f),
        palette_.muted);
    const D2D1_RECT_F reset = TimerResetButtonRect();
    FillRounded(reset, 9.0f, palette_.control);
    StrokeRounded(reset, 9.0f, palette_.border, 1.0f);
    DrawTextBlock(L"重置", captionCenterFormat_.Get(), reset, palette_.text);

    DrawTextBlock(L"计时模式", bodyStrongFormat_.Get(), Rect(kContentX, 440.0f, 300.0f, 462.0f), palette_.text);
    std::wstring modeText = ParseTimerMode(settings_->timerMode) == TimerMode::CountDown
        ? L"倒计时 " + std::to_wstring(settings_->timerCountdownMinutes) + L" 分钟"
        : L"正计时";
    DrawTextBlock(
        modeText.c_str(),
        bodyStrongFormat_.Get(),
        Rect(kContentX + 220.0f, 440.0f, kContentX + kContentW, 462.0f),
        accent);
    DrawTextBlock(
        L"右键浮动计时器可切换正计时/倒计时、标记分段",
        captionFormat_.Get(),
        Rect(kContentX, 462.0f, kContentX + kContentW, 480.0f),
        palette_.muted);

    DrawTextBlock(
        L"拖动左侧握把移动计时器；位置自动保存，并限制在工作区内",
        captionFormat_.Get(),
        Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f),
        palette_.subtle);
}

void SettingsWindow::DrawTrailPage(const D2D1_COLOR_F& accent)
{
    const D2D1_RECT_F enableCard = TrailEnableRect();
    FillRounded(enableCard, 12.0f, palette_.surfaceAlt);
    StrokeRounded(enableCard, 12.0f, palette_.border, 1.0f);
    DrawTextBlock(
        L"启用鼠标拖尾",
        bodyStrongFormat_.Get(),
        Rect(kContentX + 20.0f, 92.0f, 480.0f, 112.0f),
        palette_.text);
    DrawTextBlock(
        L"独立于高亮，关闭光圈后也能使用",
        captionFormat_.Get(),
        Rect(kContentX + 20.0f, 110.0f, 500.0f, 126.0f),
        palette_.muted);
    DrawToggle(TrailEnableToggleRect(), settings_->trailEnabled, accent);

    const wchar_t* labels[6] = {
        L"拖尾长度",
        L"停留时间",
        L"曲线平滑度",
        L"拖尾宽度",
        L"不透明度",
        L"发光强度",
    };
    std::wstring values[6] = {
        std::to_wstring(settings_->trailLengthPercent) + L"%",
        std::to_wstring(settings_->trailLifetimeMs) + L" ms",
        std::to_wstring(settings_->trailSmoothness) + L"%",
        std::to_wstring(settings_->trailWidthPercent) + L"%",
        std::to_wstring(settings_->trailOpacityPercent) + L"%",
        std::to_wstring(settings_->trailGlowPercent) + L"%",
    };
    int minimums[6] = {50, 100, 0, 50, 10, 0};
    int maximums[6] = {200, 1200, 100, 200, 100, 100};
    int current[6] = {
        settings_->trailLengthPercent,
        settings_->trailLifetimeMs,
        settings_->trailSmoothness,
        settings_->trailWidthPercent,
        settings_->trailOpacityPercent,
        settings_->trailGlowPercent,
    };
    for (int i = 0; i < 6; ++i)
    {
        const float labelY = 146.0f + static_cast<float>(i) * 48.0f;
        const float trackY = 172.0f + static_cast<float>(i) * 48.0f;
        const float fraction = static_cast<float>(current[i] - minimums[i]) /
            static_cast<float>(maximums[i] - minimums[i]);
        DrawSlider(labels[i], values[i], labelY, trackY, fraction, accent);
    }

    DrawTextBlock(L"拖尾颜色", bodyStrongFormat_.Get(), Rect(kContentX, 430.0f, 300.0f, 450.0f), palette_.text);
    const bool automatic = settings_->trailColorAuto;
    const D2D1_RECT_F autoChip = TrailColorAutoRect();
    const D2D1_RECT_F customChip = TrailColorCustomRect();
    FillRounded(autoChip, 8.0f, automatic ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
    StrokeRounded(autoChip, 8.0f, automatic ? accent : palette_.border, automatic ? 1.4f : 1.0f);
    FillRounded(customChip, 8.0f, !automatic ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
    StrokeRounded(customChip, 8.0f, !automatic ? accent : palette_.border, !automatic ? 1.4f : 1.0f);
    DrawTextBlock(L"自动", captionCenterFormat_.Get(), autoChip, automatic ? palette_.text : palette_.muted);
    DrawTextBlock(L"自定义", captionCenterFormat_.Get(), customChip, !automatic ? palette_.text : palette_.muted);

    const unsigned trailColor = automatic ? settings_->accentRgb : settings_->trailColorRgb;
    const D2D1_RECT_F swatch = TrailColorSwatchRect();
    FillRounded(swatch, 8.0f, Rgb(trailColor));
    StrokeRounded(swatch, 8.0f, palette_.border, 1.0f);
    const std::wstring hex = L"#" + FormatRgbHex(trailColor);
    DrawTextBlock(hex.c_str(), captionFormat_.Get(), Rect(392.0f, 452.0f, 500.0f, 470.0f), palette_.muted);
    DrawTextBlock(
        automatic ? L"跟随高亮颜色" : L"自定义颜色",
        captionFormat_.Get(),
        Rect(392.0f, 470.0f, 540.0f, 490.0f),
        palette_.subtle);

    const D2D1_RECT_F reset = TrailResetRect();
    FillRounded(reset, 7.0f, palette_.control);
    StrokeRounded(reset, 7.0f, palette_.border, 1.0f);
    DrawTextBlock(
        L"重置拖尾设置",
        captionCenterFormat_.Get(),
        reset,
        palette_.muted);

    DrawTextBlock(
        L"修改会立即应用并自动保存",
        captionFormat_.Get(),
        Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f),
        palette_.subtle);
}

void SettingsWindow::DrawAboutPage(const D2D1_COLOR_F& accent)
{
    const D2D1_RECT_F card = Rect(kContentX, 84.0f, kContentX + kContentW, 190.0f);
    FillRounded(card, 12.0f, palette_.surfaceAlt);
    StrokeRounded(card, 12.0f, palette_.border, 1.0f);

    const D2D1_POINT_2F mark = D2D1::Point2F(kContentX + 34.0f, 118.0f);
    brush_->SetColor(Blend(palette_.surfaceAlt, accent, 0.14f));
    renderTarget_->FillEllipse(D2D1::Ellipse(mark, 20.0f, 20.0f), brush_.Get());
    brush_->SetColor(accent);
    renderTarget_->DrawEllipse(D2D1::Ellipse(mark, 7.0f, 7.0f), brush_.Get(), 2.0f);
    renderTarget_->FillEllipse(D2D1::Ellipse(mark, 2.2f, 2.2f), brush_.Get());

    DrawTextBlock(
        L"ZaYuCursorHalo",
        bodyStrongFormat_.Get(),
        Rect(kContentX + 66.0f, 94.0f, kContentX + kContentW - 20.0f, 116.0f),
        palette_.text);
    DrawTextBlock(
        L"v0.3.0 · Windows 7 SP1 x86 优先",
        captionFormat_.Get(),
        Rect(kContentX + 66.0f, 116.0f, kContentX + kContentW - 20.0f, 134.0f),
        palette_.muted);
    DrawTextBlock(
        L"原生鼠标高亮 / 按键显示 / 浮动计时器",
        captionFormat_.Get(),
        Rect(kContentX + 66.0f, 136.0f, kContentX + kContentW - 20.0f, 154.0f),
        palette_.subtle);
    DrawTextBlock(
        L"three-lei × DeepSeek",
        captionFormat_.Get(),
        Rect(kContentX + 66.0f, 156.0f, kContentX + kContentW - 20.0f, 174.0f),
        palette_.subtle);

    DrawTextBlock(L"开源仓库", bodyStrongFormat_.Get(), Rect(kContentX, 204.0f, 300.0f, 224.0f), palette_.text);
    const D2D1_RECT_F urlCard = Rect(kContentX, 224.0f, kContentX + kContentW, 254.0f);
    FillRounded(urlCard, 8.0f, palette_.control);
    StrokeRounded(urlCard, 8.0f, palette_.border, 1.0f);
    DrawTextBlock(
        kRepositoryUrl,
        captionFormat_.Get(),
        Rect(kContentX + 12.0f, 230.0f, kContentX + kContentW - 12.0f, 250.0f),
        accent);

    const wchar_t* buttonLabels[3] = {L"打开仓库", L"复制地址", L"查看 Releases"};
    const D2D1_RECT_F buttonRects[3] = {
        AboutRepoRect(), AboutCopyRect(), AboutReleasesRect()};
    for (int i = 0; i < 3; ++i)
    {
        FillRounded(buttonRects[i], 8.0f, palette_.control);
        StrokeRounded(buttonRects[i], 8.0f, palette_.border, 1.0f);
        DrawTextBlock(
            buttonLabels[i],
            captionCenterFormat_.Get(),
            buttonRects[i],
            i == 0 ? palette_.text : palette_.muted);
    }

    StrokeLine(kContentX, 304.0f, kContentX + kContentW, 304.0f, palette_.divider, 1.0f);
    DrawTextBlock(L"版本信息", bodyStrongFormat_.Get(), Rect(kContentX, 318.0f, 300.0f, 338.0f), palette_.text);

    const wchar_t* infoLabels[3] = {L"当前版本", L"许可证", L"第三方组件"};
    const wchar_t* infoValues[3] = {
        L"0.3.0 · Win7 x86",
        L"MIT License",
        L"minimp3 (CC0) · DSEG7 Modern (OFL 1.1)",
    };
    for (int i = 0; i < 3; ++i)
    {
        const float top = 346.0f + static_cast<float>(i) * 26.0f;
        DrawTextBlock(
            infoLabels[i],
            captionFormat_.Get(),
            Rect(kContentX, top, 280.0f, top + 20.0f),
            palette_.muted);
        DrawTextBlock(
            infoValues[i],
            captionFormat_.Get(),
            Rect(kContentX + 82.0f, top, kContentX + kContentW, top + 20.0f),
            palette_.text);
    }

    const D2D1_RECT_F bilibili = AboutBilibiliRect();
    FillRounded(bilibili, 8.0f, palette_.control);
    StrokeRounded(bilibili, 8.0f, palette_.border, 1.0f);
    DrawTextBlock(
        L"哔哩哔哩主页",
        captionCenterFormat_.Get(),
        bilibili,
        palette_.text);
}

void SettingsWindow::DrawClickPage(const D2D1_COLOR_F& accent)
{
    const wchar_t* labels[2] = {L"左键波纹", L"右键波纹"};
    const unsigned colors[2] = {settings_->clickLeftRgb, settings_->clickRightRgb};
    const wchar_t* hints[2] = {
        L"点击卡片打开颜色选择器",
        L"右键点击使用这组颜色",
    };

    for (int i = 0; i < 2; ++i)
    {
        const D2D1_RECT_F card = ClickColorRect(i);
        FillRounded(card, 12.0f, palette_.surfaceAlt);
        StrokeRounded(card, 12.0f, palette_.border, 1.0f);

        DrawTextBlock(
            labels[i],
            bodyStrongFormat_.Get(),
            Rect(card.left + 18.0f, card.top + 10.0f, card.right - 90.0f, card.top + 32.0f),
            palette_.text);
        const std::wstring hex = L"#" + FormatRgbHex(colors[i]);
        DrawTextBlock(
            hex.c_str(),
            captionFormat_.Get(),
            Rect(card.left + 18.0f, card.top + 32.0f, card.right - 90.0f, card.top + 48.0f),
            palette_.muted);
        DrawTextBlock(
            hints[i],
            captionFormat_.Get(),
            Rect(card.left + 18.0f, card.top + 50.0f, card.right - 84.0f, card.top + 68.0f),
            palette_.subtle);

        const D2D1_POINT_2F center = D2D1::Point2F(card.right - 48.0f, card.top + 40.0f);
        brush_->SetColor(D2D1::ColorF(
            static_cast<float>((colors[i] >> 16) & 0xFF) / 255.0f,
            static_cast<float>((colors[i] >> 8) & 0xFF) / 255.0f,
            static_cast<float>(colors[i] & 0xFF) / 255.0f,
            0.16f));
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 27.0f, 27.0f), brush_.Get());
        brush_->SetColor(Rgb(colors[i]));
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 21.0f, 21.0f), brush_.Get(), 2.2f);
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 4.0f, 4.0f), brush_.Get());
    }

    const wchar_t* toggleLabels[2] = {L"点击波纹", L"点击音效"};
    const wchar_t* toggleHints[2] = {
        L"左键、右键、中键显示不同颜色的扩散光圈",
        L"左键和右键播放不同音高的短音",
    };
    const bool toggleValues[2] = {settings_->clickRippleEnabled, settings_->clickSoundEnabled};
    for (int i = 0; i < 2; ++i)
    {
        const float top = 282.0f + static_cast<float>(i) * 54.0f;
        if (i > 0)
            StrokeLine(kContentX, top - 10.0f, kContentX + kContentW, top - 10.0f, palette_.divider, 1.0f);
        DrawTextBlock(toggleLabels[i], bodyStrongFormat_.Get(), Rect(kContentX, top, 470.0f, top + 22.0f), palette_.text);
        DrawTextBlock(toggleHints[i], captionFormat_.Get(), Rect(kContentX, top + 20.0f, 510.0f, top + 36.0f), palette_.muted);
        DrawToggle(ClickToggleRect(i), toggleValues[i], accent);
    }

    DrawTextBlock(L"音效预设", bodyStrongFormat_.Get(), Rect(kContentX, 394.0f, 300.0f, 416.0f), palette_.text);
    DrawTextBlock(L"音量", captionFormat_.Get(), Rect(330.0f, 394.0f, 386.0f, 414.0f), palette_.muted);
    const std::wstring volumeText = std::to_wstring(settings_->clickVolumePercent) + L"%";
    DrawTextBlock(volumeText.c_str(), valueRightFormat_.Get(), Rect(504.0f, 394.0f, 560.0f, 414.0f), palette_.subtle);
    const D2D1_RECT_F volumeTrack = ClickVolumeTrackRect();
    const float volumeY = (volumeTrack.top + volumeTrack.bottom) / 2.0f;
    FillRounded(
        Rect(volumeTrack.left, volumeY - 2.0f, volumeTrack.right, volumeY + 2.0f),
        2.0f,
        palette_.divider);
    const float volumeX = volumeTrack.left +
        (volumeTrack.right - volumeTrack.left) * static_cast<float>(settings_->clickVolumePercent) / 100.0f;
    FillRounded(
        Rect(volumeTrack.left, volumeY - 2.0f, volumeX, volumeY + 2.0f),
        2.0f,
        accent);
    brush_->SetColor(accent);
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(volumeX, volumeY), 6.0f, 6.0f), brush_.Get());
    brush_->SetColor(Rgb(0xFFFFFF, 0.85f));
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(volumeX, volumeY), 2.0f, 2.0f), brush_.Get());

    const wchar_t* presetLabels[5] = {L"默认", L"柔和", L"清脆", L"木质", L"电子"};
    const ClickSoundPreset preset = ParseClickSoundPreset(settings_->clickSoundPreset);
    for (int i = 0; i < 5; ++i)
    {
        const D2D1_RECT_F chip = ClickPresetChipRect(i);
        const bool selected = preset == static_cast<ClickSoundPreset>(i);
        FillRounded(chip, 8.0f, selected ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
        StrokeRounded(
            chip,
            8.0f,
            selected ? accent : palette_.border,
            selected ? 1.4f : 1.0f);
        DrawTextBlock(
            presetLabels[i],
            captionCenterFormat_.Get(),
            chip,
            selected ? palette_.text : palette_.muted);
    }

    const D2D1_RECT_F import = ClickImportRect();
    const bool custom = preset == ClickSoundPreset::Custom;
    FillRounded(import, 9.0f, custom ? Blend(palette_.surface, accent, 0.10f) : palette_.control);
    StrokeRounded(import, 9.0f, custom ? accent : palette_.border, custom ? 1.4f : 1.0f);
    DrawTextBlock(
        custom ? L"当前：自定义音效 · 重新导入…" : L"导入 WAV / MP3 音效…",
        captionCenterFormat_.Get(),
        import,
        custom ? palette_.text : palette_.muted);

    DrawTextBlock(
        L"预设/导入只影响点击音效；自定义文件会复制到配置目录",
        captionFormat_.Get(),
        Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f),
        palette_.subtle);
}

void SettingsWindow::ImportKeyBackground()
{
    if (!settings_ || !hwnd_)
        return;

    wchar_t file[1024] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter =
        L"图片 (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0所有文件 (*.*)\0*.*\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = _countof(file);
    dialog.lpstrTitle = L"选择键帽背景图片";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
        return;

    const wchar_t* extension = wcsrchr(file, L'.');
    if (!extension ||
        (_wcsicmp(extension, L".png") != 0 &&
         _wcsicmp(extension, L".jpg") != 0 &&
         _wcsicmp(extension, L".jpeg") != 0 &&
         _wcsicmp(extension, L".bmp") != 0))
    {
        MessageBoxW(hwnd_, L"只支持 PNG、JPG 或 BMP 图片。", L"选择键帽背景图片", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring configPath = SettingsStore::DefaultPath();
    const size_t slash = configPath.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return;
    std::wstring directory = configPath.substr(0, slash) + L"\\skins";
    CreateDirectoryW(directory.c_str(), nullptr);

    wchar_t name[64] = {};
    swprintf_s(name, L"keycap_%llu%s", GetTickCount64(), extension);
    const std::wstring destination = directory + L"\\" + name;
    if (!CopyFileW(file, destination.c_str(), FALSE))
    {
        MessageBoxW(hwnd_, L"无法复制图片，请检查配置目录权限。", L"选择键帽背景图片", MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring oldFile = settings_->keyBackgroundImage;
    settings_->keyBackgroundImage = destination;
    settings_->keySkin = FormatKeySkin(KeySkin::Minimal);
    if (!oldFile.empty() && oldFile != destination &&
        _wcsnicmp(oldFile.c_str(), directory.c_str(), directory.size()) == 0)
    {
        DeleteFileW(oldFile.c_str());
    }
    if (host_)
        host_->OnSettingsKeyDisplayChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::ImportClickSound()
{
    if (!settings_ || !hwnd_)
        return;

    wchar_t file[1024] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter =
        L"音频文件 (*.wav;*.mp3)\0*.wav;*.mp3\0所有文件 (*.*)\0*.*\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = _countof(file);
    dialog.lpstrTitle = L"导入点击音效";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
        return;

    const wchar_t* extension = wcsrchr(file, L'.');
    if (!extension ||
        (_wcsicmp(extension, L".wav") != 0 && _wcsicmp(extension, L".mp3") != 0))
    {
        MessageBoxW(hwnd_, L"只支持 WAV 或 MP3 文件。", L"导入点击音效", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const bool mp3 = _wcsicmp(extension, L".mp3") == 0;

    std::wstring configPath = SettingsStore::DefaultPath();
    const size_t slash = configPath.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return;
    std::wstring directory = configPath.substr(0, slash) + L"\\sounds";
    CreateDirectoryW(directory.c_str(), nullptr);

    wchar_t name[64] = {};
    swprintf_s(name, L"custom_%llu%s", GetTickCount64(), mp3 ? L".wav" : extension);
    const std::wstring destination = directory + L"\\" + name;
    if (mp3)
    {
        std::wstring decodeError;
        if (!DecodeMp3ToWav(file, destination, decodeError))
        {
            MessageBoxW(
                hwnd_,
                decodeError.c_str(),
                L"导入点击音效",
                MB_OK | MB_ICONERROR);
            return;
        }
    }
    else if (!CopyFileW(file, destination.c_str(), FALSE))
    {
        MessageBoxW(hwnd_, L"无法复制音效文件，请检查磁盘权限或空间。", L"导入点击音效", MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring oldFile = settings_->clickSoundFile;
    settings_->clickSoundPreset = L"custom";
    settings_->clickSoundFile = destination;
    if (!oldFile.empty() && oldFile != destination &&
        _wcsnicmp(oldFile.c_str(), directory.c_str(), directory.size()) == 0)
    {
        DeleteFileW(oldFile.c_str());
    }
    if (host_)
    {
        host_->OnSettingsClickChanged();
        host_->OnSettingsPreviewClickSound(false);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::DrawStackPage(const D2D1_COLOR_F& accent)
{
    DrawTextBlock(L"启用堆叠显示", bodyStrongFormat_.Get(), Rect(kContentX, 84.0f, 470.0f, 106.0f), palette_.text);
    DrawTextBlock(
        L"连续快捷键同时保留，不互相替换",
        captionFormat_.Get(),
        Rect(kContentX, 106.0f, 500.0f, 124.0f),
        palette_.muted);
    DrawToggle(StackToggleRect(), settings_->keyStack, accent);

    StrokeLine(kContentX, 150.0f, kContentX + kContentW, 150.0f, palette_.divider, 1.0f);

    const float delayFraction = static_cast<float>(settings_->keyStackDelayMs) / 3000.0f;
    DrawSlider(
        L"堆叠消失延迟",
        std::to_wstring(settings_->keyStackDelayMs) + L" ms",
        168.0f,
        196.0f,
        delayFraction,
        accent);

    DrawTextBlock(
        L"每条快捷键在延迟结束后淡出，并逐条移除",
        captionFormat_.Get(),
        Rect(kContentX, 230.0f, kContentX + kContentW, 250.0f),
        palette_.muted);

    const D2D1_RECT_F card = Rect(kContentX, 270.0f, kContentX + kContentW, 430.0f);
    FillRounded(card, 12.0f, palette_.surfaceAlt);
    StrokeRounded(card, 12.0f, palette_.border, 1.0f);
    DrawTextBlock(
        L"最多保留最近 5 条",
        bodyStrongFormat_.Get(),
        Rect(card.left + 18.0f, card.top + 14.0f, card.right - 18.0f, card.top + 36.0f),
        palette_.text);
    DrawTextBlock(
        L"新记录在底部，旧记录向上堆叠；单独按修饰键也会保留一行。",
        captionFormat_.Get(),
        Rect(card.left + 18.0f, card.top + 40.0f, card.right - 18.0f, card.top + 60.0f),
        palette_.muted);

    for (int i = 0; i < 3; ++i)
    {
        const D2D1_RECT_F row = Rect(
            card.left + 18.0f,
            card.top + 74.0f + static_cast<float>(i) * 28.0f,
            card.right - 18.0f,
            card.top + 96.0f + static_cast<float>(i) * 28.0f);
        FillRounded(row, 7.0f, palette_.control);
        DrawTextBlock(
            i == 0 ? L"Ctrl + S" : (i == 1 ? L"Ctrl + T" : L"Ctrl + C"),
            captionFormat_.Get(),
            Rect(row.left + 12.0f, row.top, row.right - 12.0f, row.bottom),
            palette_.muted);
    }

    DrawTextBlock(
        L"关闭后立即清空历史，只显示当前按下的组合",
        captionFormat_.Get(),
        Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f),
        palette_.subtle);
}

void SettingsWindow::DrawSkinPage(const D2D1_COLOR_F& accent)
{
    const D2D1_RECT_F preview = Rect(kContentX, 84.0f, kContentX + kContentW, 206.0f);
    FillRounded(preview, 12.0f, palette_.surfaceAlt);
    StrokeRounded(preview, 12.0f, palette_.border, 1.0f);
    DrawKeycapPreview(
        Rect(kContentX + 92.0f, 124.0f, kContentX + 146.0f, 156.0f),
        L"Ctrl",
        true,
        KeySkin::Minimal,
        accent);
    DrawKeycapPreview(
        Rect(kContentX + 180.0f, 124.0f, kContentX + 214.0f, 156.0f),
        L"S",
        false,
        KeySkin::Minimal,
        accent);
    DrawTextBlock(
        L"极简皮肤预览",
        captionFormat_.Get(),
        Rect(kContentX + 16.0f, 94.0f, kContentX + 200.0f, 112.0f),
        palette_.muted);

    const wchar_t* labels[3] = {L"填充颜色", L"边框颜色", L"文字颜色"};
    const unsigned colors[3] = {
        settings_->keyFillRgb,
        settings_->keyBorderRgb,
        settings_->keyTextAuto ? 0xFFFFFF : settings_->keyTextRgb};
    for (int i = 0; i < 3; ++i)
    {
        const float top = 226.0f + static_cast<float>(i) * 58.0f;
        DrawTextBlock(
            labels[i],
            bodyStrongFormat_.Get(),
            Rect(kContentX, top, 360.0f, top + 22.0f),
            palette_.text);
        const std::wstring hex =
            (i == 2 && settings_->keyTextAuto) ? L"自动" : L"#" + FormatRgbHex(colors[i]);
        DrawTextBlock(
            hex.c_str(),
            captionFormat_.Get(),
            Rect(kContentX, top + 20.0f, 360.0f, top + 38.0f),
            palette_.muted);
        const D2D1_RECT_F swatch = SkinSwatchRect(i);
        FillRounded(
            swatch,
            8.0f,
            (i == 2 && settings_->keyTextAuto) ? palette_.control : Rgb(colors[i]));
        StrokeRounded(swatch, 8.0f, palette_.border, 1.0f);
        if (i == 2 && settings_->keyTextAuto)
        {
            brush_->SetColor(palette_.muted);
            renderTarget_->DrawLine(
                D2D1::Point2F(swatch.left + 7.0f, swatch.bottom - 7.0f),
                D2D1::Point2F(swatch.right - 7.0f, swatch.top + 7.0f),
                brush_.Get(),
                1.2f);
        }
    }

    DrawTextBlock(
        L"背景图片",
        bodyStrongFormat_.Get(),
        Rect(kContentX, 406.0f, 360.0f, 428.0f),
        palette_.text);
    DrawTextBlock(
        settings_->keyBackgroundImage.empty()
            ? L"未设置；建议 128×64 PNG/JPG，会按键帽拉伸"
            : L"已设置，可移除后恢复纯色",
        captionFormat_.Get(),
        Rect(kContentX, 426.0f, kContentX + kContentW, 446.0f),
        palette_.muted);

    const D2D1_RECT_F import = SkinImageButtonRect();
    FillRounded(import, 9.0f, palette_.control);
    StrokeRounded(import, 9.0f, palette_.border, 1.0f);
    DrawTextBlock(L"选择图片…", captionCenterFormat_.Get(), import, palette_.text);

    if (!settings_->keyBackgroundImage.empty())
    {
        const D2D1_RECT_F clear = SkinImageClearRect();
        FillRounded(clear, 9.0f, palette_.control);
        StrokeRounded(clear, 9.0f, palette_.border, 1.0f);
        DrawTextBlock(L"移除", captionCenterFormat_.Get(), clear, palette_.muted);
    }

    DrawTextBlock(
        L"颜色和背景只作用于极简皮肤；图片会拉伸到每个键帽",
        captionFormat_.Get(),
        Rect(kContentX, kFooterY, kContentX + kContentW, kFooterY + 18.0f),
        palette_.subtle);
}

void SettingsWindow::DrawGeneralPage(const D2D1_COLOR_F& accent)
{
    DrawTextBlock(L"开机启动", bodyStrongFormat_.Get(), Rect(kContentX, 84.0f, 470.0f, 106.0f), palette_.text);
    DrawTextBlock(L"登录 Windows 后自动在托盘启动", captionFormat_.Get(), Rect(kContentX, 106.0f, 500.0f, 124.0f), palette_.muted);
    DrawToggle(Rect(536.0f, 88.0f, 576.0f, 110.0f), settings_->startWithWindows, accent);

    StrokeLine(kContentX, 150.0f, kContentX + kContentW, 150.0f, palette_.divider, 1.0f);

    DrawTextBlock(L"全局快捷键", bodyStrongFormat_.Get(), Rect(kContentX, 168.0f, 400.0f, 190.0f), palette_.text);
    D2D1_COLOR_F hintColor = palette_.muted;
    std::wstring hint = L"点击右侧组合键可修改";
    if (!hotkeyError_.empty())
    {
        hint = hotkeyError_;
        hintColor = palette_.error;
    }
    else if (hotkeyRecording_)
    {
        hint = L"请按下新的组合键，Esc 取消";
        hintColor = accent;
    }
    DrawTextBlock(hint.c_str(), captionFormat_.Get(), Rect(kContentX, 190.0f, 440.0f, 208.0f), hintColor);

    const D2D1_RECT_F shortcutPill = HotkeyPillRect();
    const D2D1_COLOR_F pillFill = hotkeyRecording_
        ? Blend(palette_.surface, accent, 0.10f)
        : (hotkeyPillHover_ ? palette_.controlHover : palette_.control);
    FillRounded(shortcutPill, 8.0f, pillFill);
    StrokeRounded(
        shortcutPill,
        8.0f,
        hotkeyRecording_ ? accent : palette_.border,
        hotkeyRecording_ ? 1.4f : 1.0f);

    std::wstring hotkeyLabel;
    if (hotkeyRecording_)
    {
        hotkeyLabel = L"等待按键…";
    }
    else
    {
        HotkeyBinding binding;
        hotkeyLabel = ParseHotkey(settings_->hotkey, binding)
            ? FormatHotkey(binding)
            : L"未设置";
    }
    DrawTextBlock(
        hotkeyLabel.c_str(),
        centerFormat_.Get(),
        shortcutPill,
        hotkeyRecording_ ? accent : palette_.text);

    StrokeLine(kContentX, 228.0f, kContentX + kContentW, 228.0f, palette_.divider, 1.0f);

    DrawTextBlock(L"主题", bodyStrongFormat_.Get(), Rect(kContentX, 246.0f, 360.0f, 268.0f), palette_.text);
    DrawTextBlock(L"跟随系统时会自动切换", captionFormat_.Get(), Rect(kContentX, 268.0f, 380.0f, 286.0f), palette_.muted);

    const D2D1_RECT_F segment = ThemeSegmentRect();
    FillRounded(segment, 8.0f, palette_.control);
    const wchar_t* labels[3] = {L"跟随系统", L"浅色", L"深色"};
    const ThemeMode selectedMode = ParseThemeMode(settings_->theme);
    const int selectedIndex = static_cast<int>(selectedMode);
    for (int i = 0; i < 3; ++i)
    {
        const D2D1_RECT_F chip = ThemeChipRect(i);
        if (i == selectedIndex)
        {
            const D2D1_COLOR_F selectedFill = darkMode_ ? UiColor(0x3A3E46) : palette_.surface;
            FillRounded(Rect(chip.left + 2.0f, chip.top + 2.0f, chip.right - 2.0f, chip.bottom - 2.0f), 6.0f, selectedFill);
            StrokeRounded(Rect(chip.left + 2.0f, chip.top + 2.0f, chip.right - 2.0f, chip.bottom - 2.0f), 6.0f, palette_.border, 1.0f);
        }
        else if (hoverThemeChip_ == i)
        {
            FillRounded(Rect(chip.left + 2.0f, chip.top + 2.0f, chip.right - 2.0f, chip.bottom - 2.0f), 6.0f, palette_.controlHover);
        }
        DrawTextBlock(
            labels[i],
            captionCenterFormat_.Get(),
            chip,
            i == selectedIndex ? palette_.text : palette_.muted);
    }

    StrokeLine(kContentX, 302.0f, kContentX + kContentW, 302.0f, palette_.divider, 1.0f);

    DrawTextBlock(L"显示按键", bodyStrongFormat_.Get(), Rect(kContentX, 320.0f, 470.0f, 342.0f), palette_.text);
    DrawTextBlock(L"在屏幕底部显示当前按下的键", captionFormat_.Get(), Rect(kContentX, 342.0f, 500.0f, 360.0f), palette_.muted);
    DrawToggle(ShowKeysToggleRect(), settings_->showKeys, accent);

    StrokeLine(kContentX, 372.0f, kContentX + kContentW, 372.0f, palette_.divider, 1.0f);

    DrawTextBlock(L"鼠标拖尾", bodyStrongFormat_.Get(), Rect(kContentX, 386.0f, 470.0f, 408.0f), palette_.text);
    DrawTextBlock(
        L"独立于高亮，可单独开启使用",
        captionFormat_.Get(),
        Rect(kContentX, 408.0f, 500.0f, 426.0f),
        palette_.muted);
    DrawToggle(TrailToggleRect(), settings_->trailEnabled, accent);

    StrokeLine(kContentX, 440.0f, kContentX + kContentW, 440.0f, palette_.divider, 1.0f);

    DrawTextBlock(L"版本", bodyStrongFormat_.Get(), Rect(kContentX, 454.0f, 400.0f, 476.0f), palette_.text);
    DrawTextBlock(L"ZaYuCursorHalo 0.3.0", captionFormat_.Get(), Rect(kContentX, 472.0f, 500.0f, 490.0f), palette_.muted);
    DrawTextBlock(
        L"three-lei × DeepSeek",
        captionFormat_.Get(),
        Rect(kContentX, 490.0f, 500.0f, 508.0f),
        palette_.subtle);

    const D2D1_RECT_F resetAll = ResetAllRect();
    FillRounded(resetAll, 7.0f, palette_.control);
    StrokeRounded(resetAll, 7.0f, palette_.border, 1.0f);
    DrawTextBlock(
        L"重置所有选项",
        captionCenterFormat_.Get(),
        resetAll,
        palette_.muted);

    DrawTextBlock(L"设置会自动保存在当前用户目录", captionFormat_.Get(), Rect(kContentX, 540.0f, kContentX + kContentW, 558.0f), palette_.subtle);
}

void SettingsWindow::DrawCloseButton()
{
    const D2D1_RECT_F rect = CloseRect();
    if (hoverClose_)
        FillRounded(rect, 8.0f, palette_.controlHover);

    const D2D1_POINT_2F center = D2D1::Point2F((rect.left + rect.right) / 2.0f, (rect.top + rect.bottom) / 2.0f);
    brush_->SetColor(palette_.muted);
    renderTarget_->DrawLine(
        D2D1::Point2F(center.x - 5.0f, center.y - 5.0f),
        D2D1::Point2F(center.x + 5.0f, center.y + 5.0f),
        brush_.Get(),
        1.4f);
    renderTarget_->DrawLine(
        D2D1::Point2F(center.x + 5.0f, center.y - 5.0f),
        D2D1::Point2F(center.x - 5.0f, center.y + 5.0f),
        brush_.Get(),
        1.4f);
}

void SettingsWindow::DrawColorPicker()
{
    const D2D1_COLOR_F accent = Rgb(colorDraftRgb_);
    const D2D1_RECT_F card = ColorCardRect();

    // Dim the settings UI behind the modal.
    FillRect(Rect(0.0f, 0.0f, kWidth, kHeight), palette_.overlay);
    FillRounded(
        Rect(card.left + 2.0f, card.top + 5.0f, card.right + 2.0f, card.bottom + 5.0f),
        16.0f,
        palette_.shadow);
    FillRounded(card, 16.0f, palette_.surface);
    StrokeRounded(card, 16.0f, palette_.border, 1.0f);

    const wchar_t* pickerTitle = L"自定义高亮颜色";
    switch (colorTarget_)
    {
    case ColorTarget::KeyText: pickerTitle = L"按键文字颜色"; break;
    case ColorTarget::KeyStyle: pickerTitle = L"极简键帽主色"; break;
    case ColorTarget::KeyFill: pickerTitle = L"键帽填充颜色"; break;
    case ColorTarget::KeyBorder: pickerTitle = L"键帽边框颜色"; break;
    case ColorTarget::ClickLeft: pickerTitle = L"左键波纹颜色"; break;
    case ColorTarget::ClickRight: pickerTitle = L"右键波纹颜色"; break;
    case ColorTarget::Trail: pickerTitle = L"鼠标拖尾颜色"; break;
    default: break;
    }
    DrawTextBlock(
        pickerTitle,
        titleFormat_.Get(),
        Rect(124.0f, kModalTop + 12.0f, 430.0f, kModalTop + 38.0f),
        palette_.text);

    const D2D1_RECT_F sv = SaturationValueRect();
    const D2D1_RECT_F hue = HueRect();

    D2D1_GRADIENT_STOP saturationStops[2] = {
        {0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)},
        {1.0f, Rgb(HsvToRgb(colorHue_, 1.0f, 1.0f))},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> saturationCollection;
    if (SUCCEEDED(renderTarget_->CreateGradientStopCollection(
            saturationStops,
            2,
            D2D1_GAMMA_2_2,
            D2D1_EXTEND_MODE_CLAMP,
            saturationCollection.GetAddressOf())) &&
        saturationCollection)
    {
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> saturationBrush;
        const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties =
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(sv.left, sv.top),
                D2D1::Point2F(sv.right, sv.top));
        if (SUCCEEDED(renderTarget_->CreateLinearGradientBrush(
                properties,
                saturationCollection.Get(),
                saturationBrush.GetAddressOf())) &&
            saturationBrush)
        {
            renderTarget_->FillRectangle(sv, saturationBrush.Get());
        }
    }

    D2D1_GRADIENT_STOP valueStops[2] = {
        {0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)},
        {1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> valueCollection;
    if (SUCCEEDED(renderTarget_->CreateGradientStopCollection(
            valueStops,
            2,
            D2D1_GAMMA_2_2,
            D2D1_EXTEND_MODE_CLAMP,
            valueCollection.GetAddressOf())) &&
        valueCollection)
    {
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> valueBrush;
        const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties =
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(sv.left, sv.top),
                D2D1::Point2F(sv.left, sv.bottom));
        if (SUCCEEDED(renderTarget_->CreateLinearGradientBrush(
                properties,
                valueCollection.Get(),
                valueBrush.GetAddressOf())) &&
            valueBrush)
        {
            renderTarget_->FillRectangle(sv, valueBrush.Get());
        }
    }
    StrokeRounded(sv, 0.0f, palette_.border, 1.0f);

    const D2D1_POINT_2F marker = D2D1::Point2F(
        sv.left + colorSaturation_ * (sv.right - sv.left),
        sv.top + (1.0f - colorValue_) * (sv.bottom - sv.top));
    brush_->SetColor(Rgb(0x111111, 0.55f));
    renderTarget_->DrawEllipse(D2D1::Ellipse(marker, 7.0f, 7.0f), brush_.Get(), 1.4f);
    brush_->SetColor(Rgb(0xFFFFFF));
    renderTarget_->DrawEllipse(D2D1::Ellipse(marker, 5.5f, 5.5f), brush_.Get(), 1.6f);

    D2D1_GRADIENT_STOP hueStops[7] = {
        {0.0f, Rgb(0xFF3B30)},
        {1.0f / 6.0f, Rgb(0xFFCC00)},
        {2.0f / 6.0f, Rgb(0x34C759)},
        {3.0f / 6.0f, Rgb(0x32ADE6)},
        {4.0f / 6.0f, Rgb(0x007AFF)},
        {5.0f / 6.0f, Rgb(0xAF52DE)},
        {1.0f, Rgb(0xFF3B30)},
    };
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> hueCollection;
    if (SUCCEEDED(renderTarget_->CreateGradientStopCollection(
            hueStops,
            7,
            D2D1_GAMMA_2_2,
            D2D1_EXTEND_MODE_CLAMP,
            hueCollection.GetAddressOf())) &&
        hueCollection)
    {
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> hueBrush;
        const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties =
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(hue.left, hue.top),
                D2D1::Point2F(hue.right, hue.top));
        if (SUCCEEDED(renderTarget_->CreateLinearGradientBrush(
                properties,
                hueCollection.Get(),
                hueBrush.GetAddressOf())) &&
            hueBrush)
        {
            renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(hue, 8.0f, 8.0f), hueBrush.Get());
        }
    }
    StrokeRounded(hue, 8.0f, palette_.border, 1.0f);

    const float hueX = hue.left + (colorHue_ / 360.0f) * (hue.right - hue.left);
    brush_->SetColor(Rgb(0x111111, 0.65f));
    renderTarget_->FillRoundedRectangle(
        D2D1::RoundedRect(Rect(hueX - 3.5f, hue.top - 3.0f, hueX + 3.5f, hue.bottom + 3.0f), 3.5f, 3.5f),
        brush_.Get());
    brush_->SetColor(Rgb(0xFFFFFF));
    renderTarget_->FillRoundedRectangle(
        D2D1::RoundedRect(Rect(hueX - 1.8f, hue.top - 0.5f, hueX + 1.8f, hue.bottom + 0.5f), 1.8f, 1.8f),
        brush_.Get());

    DrawHexInput(accent);

    const D2D1_RECT_F cancel = ColorCancelRect();
    if (hoverColorCancel_)
        FillRounded(cancel, 8.0f, palette_.control);
    DrawTextBlock(L"取消", centerFormat_.Get(), cancel, palette_.muted);

    const D2D1_RECT_F done = ColorDoneRect();
    FillRounded(done, 8.0f, accent);
    DrawTextBlock(
        L"完成",
        centerFormat_.Get(),
        done,
        IsLightColor(colorDraftRgb_) ? Rgb(0x1B1C1F) : Rgb(0xFFFFFF));

    DrawTextBlock(
        L"输入或粘贴 6 位 HEX 后自动应用",
        captionFormat_.Get(),
        Rect(124.0f, kModalTop + 252.0f, 476.0f, kModalTop + 270.0f),
        palette_.subtle);

    DrawColorFocusRings();
}

void SettingsWindow::DrawHexInput(const D2D1_COLOR_F& accent)
{
    NormalizeHexState();
    const D2D1_RECT_F field = HexInputRect();
    const bool incomplete = hexFocused_ && !hexEdit_.empty() && hexEdit_.size() < 6;
    FillRounded(field, 8.0f, hexFocused_ ? palette_.surface : palette_.surfaceAlt);
    StrokeRounded(
        field,
        8.0f,
        incomplete ? Rgb(0xE0A33D) : (hexFocused_ ? accent : palette_.border),
        hexFocused_ ? 1.5f : 1.0f);

    const D2D1_POINT_2F dot = D2D1::Point2F(137.0f, kHexFieldTop + 18.0f);
    brush_->SetColor(accent);
    renderTarget_->FillEllipse(D2D1::Ellipse(dot, 8.0f, 8.0f), brush_.Get());
    brush_->SetColor(Rgb(0xFFFFFF, 0.65f));
    renderTarget_->DrawEllipse(D2D1::Ellipse(dot, 8.0f, 8.0f), brush_.Get(), 1.0f);

    DrawTextBlock(L"#", centerFormat_.Get(), Rect(150.0f, kHexCellTop, 162.0f, kHexCellBottom), palette_.muted);

    static const wchar_t kPlaceholders[] = L"RRGGBB";
    const bool showPlaceholders = hexEdit_.empty();
    for (int i = 0; i < 6; ++i)
    {
        const float cellLeft = kHexCellsLeft + kHexCellWidth * i;
        const D2D1_RECT_F cell = Rect(cellLeft, kHexCellTop, cellLeft + kHexCellWidth, kHexCellBottom);
        if (hexSelectAll_ && i < static_cast<int>(hexEdit_.size()))
        {
            FillRounded(
                Rect(cell.left + 0.8f, cell.top, cell.right - 0.8f, cell.bottom),
                3.0f,
                D2D1::ColorF(accent.r, accent.g, accent.b, 0.18f));
        }

        wchar_t character = L'\0';
        D2D1_COLOR_F color = palette_.text;
        if (i < static_cast<int>(hexEdit_.size()))
        {
            character = hexEdit_[i];
        }
        else if (showPlaceholders)
        {
            character = kPlaceholders[i];
            color = D2D1::ColorF(palette_.muted.r, palette_.muted.g, palette_.muted.b, 0.58f);
        }
        if (character == L'\0')
            continue;
        const wchar_t text[2] = {character, L'\0'};
        DrawTextBlock(text, centerFormat_.Get(), cell, color);
    }

    DrawTextBlock(L"HEX", captionCenterFormat_.Get(), Rect(270.0f, kHexCellTop, 294.0f, kHexCellBottom), palette_.muted);

    if (hexFocused_ && !hexSelectAll_)
    {
        const float caretX = kHexCellsLeft + kHexCellWidth * static_cast<float>(hexCaret_);
        brush_->SetColor(accent);
        renderTarget_->DrawLine(
            D2D1::Point2F(caretX, kHexCellTop + 2.0f),
            D2D1::Point2F(caretX, kHexCellBottom - 2.0f),
            brush_.Get(),
            1.4f);
    }
}

void SettingsWindow::DrawLineIcon(int icon, D2D1_POINT_2F center, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    if (icon == 0)
    {
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 6.5f, 6.5f), brush_.Get(), 1.4f);
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 1.8f, 1.8f), brush_.Get());
        return;
    }
    if (icon == 1)
    {
        const float left = center.x - 7.0f;
        const float right = center.x + 7.0f;
        const float ys[3] = {center.y - 5.0f, center.y, center.y + 5.0f};
        const float knobX[3] = {center.x + 2.5f, center.x - 3.0f, center.x + 4.0f};
        for (int i = 0; i < 3; ++i)
        {
            renderTarget_->DrawLine(D2D1::Point2F(left, ys[i]), D2D1::Point2F(right, ys[i]), brush_.Get(), 1.2f);
            renderTarget_->DrawEllipse(
                D2D1::Ellipse(D2D1::Point2F(knobX[i], ys[i]), 2.1f, 2.1f),
                brush_.Get(),
                1.2f);
        }
        return;
    }
    if (icon == 2)
    {
        const D2D1_RECT_F keyboard = Rect(center.x - 8.0f, center.y - 6.0f, center.x + 8.0f, center.y + 6.0f);
        renderTarget_->DrawRoundedRectangle(D2D1::RoundedRect(keyboard, 2.4f, 2.4f), brush_.Get(), 1.2f);
        const float xs[4] = {center.x - 5.0f, center.x - 2.0f, center.x + 1.0f, center.x + 4.0f};
        for (float x : xs)
        {
            renderTarget_->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(x, center.y - 2.0f), 0.9f, 0.9f),
                brush_.Get());
        }
        renderTarget_->DrawLine(
            D2D1::Point2F(center.x - 4.0f, center.y + 2.5f),
            D2D1::Point2F(center.x + 4.0f, center.y + 2.5f),
            brush_.Get(),
            1.2f);
        return;
    }
    if (icon == 3)
    {
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 7.0f, 7.0f), brush_.Get(), 1.3f);
        renderTarget_->DrawLine(
            D2D1::Point2F(center.x, center.y - 4.0f),
            D2D1::Point2F(center.x, center.y),
            brush_.Get(),
            1.3f);
        renderTarget_->DrawLine(
            D2D1::Point2F(center.x, center.y),
            D2D1::Point2F(center.x + 3.5f, center.y + 2.0f),
            brush_.Get(),
            1.3f);
        return;
    }
    if (icon == 4)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float top = center.y - 7.0f + static_cast<float>(i) * 5.5f;
            renderTarget_->DrawRoundedRectangle(
                D2D1::RoundedRect(
                    Rect(center.x - 8.0f, top, center.x + 8.0f, top + 3.6f),
                    1.8f,
                    1.8f),
                brush_.Get(),
                1.2f);
        }
        return;
    }
    if (icon == 5)
    {
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 7.0f, 7.0f), brush_.Get(), 1.2f);
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 3.6f, 3.6f), brush_.Get(), 1.1f);
        renderTarget_->FillEllipse(D2D1::Ellipse(center, 1.2f, 1.2f), brush_.Get());
        return;
    }
    if (icon == 6)
    {
        const float size = 5.4f;
        renderTarget_->DrawRoundedRectangle(
            D2D1::RoundedRect(
                Rect(center.x - 7.2f, center.y - 7.2f, center.x - 7.2f + size * 2.0f, center.y - 7.2f + size * 2.0f),
                1.4f,
                1.4f),
            brush_.Get(),
            1.1f);
        renderTarget_->DrawRoundedRectangle(
            D2D1::RoundedRect(
                Rect(center.x + 1.8f, center.y - 7.2f, center.x + 1.8f + size * 2.0f, center.y - 7.2f + size * 2.0f),
                1.4f,
                1.4f),
            brush_.Get(),
            1.1f);
        renderTarget_->DrawRoundedRectangle(
            D2D1::RoundedRect(
                Rect(center.x - 7.2f, center.y + 1.8f, center.x - 7.2f + size * 2.0f, center.y + 1.8f + size * 2.0f),
                1.4f,
                1.4f),
            brush_.Get(),
            1.1f);
        renderTarget_->FillRoundedRectangle(
            D2D1::RoundedRect(
                Rect(center.x + 1.8f, center.y + 1.8f, center.x + 1.8f + size * 2.0f, center.y + 1.8f + size * 2.0f),
                1.4f,
                1.4f),
            brush_.Get());
        return;
    }

    if (icon == 9)
    {
        renderTarget_->DrawEllipse(D2D1::Ellipse(center, 7.0f, 7.0f), brush_.Get(), 1.3f);
        renderTarget_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(center.x, center.y - 2.8f), 1.2f, 1.2f),
            brush_.Get());
        renderTarget_->DrawLine(
            D2D1::Point2F(center.x, center.y - 0.2f),
            D2D1::Point2F(center.x, center.y + 4.0f),
            brush_.Get(),
            1.4f);
        return;
    }

    if (icon == 8)
    {
        const D2D1_POINT_2F points[5] = {
            D2D1::Point2F(center.x - 8.0f, center.y + 5.0f),
            D2D1::Point2F(center.x - 4.0f, center.y - 4.0f),
            D2D1::Point2F(center.x, center.y + 3.0f),
            D2D1::Point2F(center.x + 4.0f, center.y - 5.0f),
            D2D1::Point2F(center.x + 8.0f, center.y + 1.0f),
        };
        for (int i = 0; i < 4; ++i)
        {
            renderTarget_->DrawLine(
                points[i],
                points[i + 1],
                brush_.Get(),
                1.5f);
        }
        renderTarget_->FillEllipse(
            D2D1::Ellipse(points[4], 1.5f, 1.5f),
            brush_.Get());
        return;
    }

    renderTarget_->DrawEllipse(D2D1::Ellipse(center, 4.4f, 4.4f), brush_.Get(), 1.3f);
    for (int i = 0; i < 8; ++i)
    {
        const float angle = static_cast<float>(i) * 3.14159265358979323846f / 4.0f;
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);
        renderTarget_->DrawLine(
            D2D1::Point2F(center.x + cosAngle * 6.2f, center.y + sinAngle * 6.2f),
            D2D1::Point2F(center.x + cosAngle * 8.0f, center.y + sinAngle * 8.0f),
            brush_.Get(),
            1.2f);
    }
}

void SettingsWindow::DrawTextBlock(
    const wchar_t* text,
    IDWriteTextFormat* format,
    const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color)
{
    if (!renderTarget_ || !format || !text)
        return;
    brush_->SetColor(color);
    renderTarget_->DrawTextW(
        text,
        static_cast<UINT32>(wcslen(text)),
        format,
        rect,
        brush_.Get(),
        D2D1_DRAW_TEXT_OPTIONS_NONE,
        DWRITE_MEASURING_MODE_NATURAL);
}

void SettingsWindow::FillRect(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    renderTarget_->FillRectangle(rect, brush_.Get());
}

void SettingsWindow::FillRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush_.Get());
}

void SettingsWindow::StrokeRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, float width)
{
    brush_->SetColor(color);
    renderTarget_->DrawRoundedRectangle(
        D2D1::RoundedRect(rect, radius, radius),
        brush_.Get(),
        width);
}

void SettingsWindow::StrokeLine(float x1, float y1, float x2, float y2, const D2D1_COLOR_F& color, float width)
{
    brush_->SetColor(color);
    renderTarget_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush_.Get(), width);
}

D2D1_RECT_F SettingsWindow::NavRect(int index) const
{
    const float top = kNavY + static_cast<float>(index) * (kNavH + kNavGap);
    return Rect(kNavX, top, kNavX + kNavW, top + kNavH);
}

D2D1_RECT_F SettingsWindow::CloseRect() const
{
    return Rect(554.0f, 14.0f, 584.0f, 44.0f);
}

D2D1_RECT_F SettingsWindow::EnableRowRect() const
{
    return Rect(kContentX, 324.0f, kContentX + kContentW, 378.0f);
}

D2D1_RECT_F SettingsWindow::HaloStyleChipRect(int index) const
{
    constexpr float width = 91.0f;
    constexpr float gap = 8.0f;
    const float left = kContentX + static_cast<float>(index) * (width + gap);
    return Rect(left, 268.0f, left + width, 298.0f);
}

D2D1_RECT_F SettingsWindow::RadiusTrackRect() const
{
    return Rect(kContentX, 392.0f, kContentX + kContentW, 442.0f);
}

D2D1_RECT_F SettingsWindow::OpacityTrackRect() const
{
    return Rect(kContentX, 444.0f, kContentX + kContentW, 494.0f);
}

D2D1_RECT_F SettingsWindow::AutoStartRowRect() const
{
    return Rect(kContentX, 72.0f, kContentX + kContentW, 132.0f);
}

D2D1_RECT_F SettingsWindow::CustomColorRect() const
{
    return Rect(kCustomSwatchX - 22.0f, kSwatchY - 28.0f, kCustomSwatchX + 22.0f, kSwatchY + 42.0f);
}

D2D1_RECT_F SettingsWindow::ColorCardRect() const
{
    return Rect(100.0f, kModalTop, 500.0f, kModalTop + kModalHeight);
}

D2D1_RECT_F SettingsWindow::SaturationValueRect() const
{
    return Rect(124.0f, kModalTop + 42.0f, 476.0f, kModalTop + 166.0f);
}

D2D1_RECT_F SettingsWindow::HueRect() const
{
    return Rect(124.0f, kModalTop + 176.0f, 476.0f, kModalTop + 192.0f);
}

D2D1_RECT_F SettingsWindow::ColorCancelRect() const
{
    return Rect(306.0f, kModalTop + 206.0f, 370.0f, kModalTop + 238.0f);
}

D2D1_RECT_F SettingsWindow::ColorDoneRect() const
{
    return Rect(382.0f, kModalTop + 206.0f, 476.0f, kModalTop + 238.0f);
}

D2D1_RECT_F SettingsWindow::HexInputRect() const
{
    return Rect(kHexFieldLeft, kHexFieldTop, kHexFieldRight, kHexFieldBottom);
}

D2D1_RECT_F SettingsWindow::HotkeyPillRect() const
{
    return Rect(444.0f, 164.0f, 576.0f, 196.0f);
}

D2D1_RECT_F SettingsWindow::ThemeSegmentRect() const
{
    return Rect(396.0f, 246.0f, 576.0f, 278.0f);
}

D2D1_RECT_F SettingsWindow::ThemeChipRect(int index) const
{
    const D2D1_RECT_F segment = ThemeSegmentRect();
    const float width = (segment.right - segment.left) / 3.0f;
    return Rect(
        segment.left + width * index,
        segment.top,
        segment.left + width * (index + 1),
        segment.bottom);
}

D2D1_RECT_F SettingsWindow::ShowKeysRowRect() const
{
    return Rect(kContentX, 320.0f, kContentX + kContentW, 360.0f);
}

D2D1_RECT_F SettingsWindow::ShowKeysToggleRect() const
{
    return Rect(536.0f, 324.0f, 576.0f, 346.0f);
}

D2D1_RECT_F SettingsWindow::TrailRowRect() const
{
    return Rect(kContentX, 386.0f, kContentX + kContentW, 426.0f);
}

D2D1_RECT_F SettingsWindow::TrailToggleRect() const
{
    return Rect(536.0f, 390.0f, 576.0f, 412.0f);
}

D2D1_RECT_F SettingsWindow::TrailEnableRect() const
{
    return Rect(kContentX, 84.0f, kContentX + kContentW, 128.0f);
}

D2D1_RECT_F SettingsWindow::TrailEnableToggleRect() const
{
    return Rect(520.0f, 94.0f, 560.0f, 116.0f);
}

D2D1_RECT_F SettingsWindow::TrailTrackRect(int index) const
{
    const float top = 172.0f + static_cast<float>(index) * 48.0f;
    return Rect(kContentX, top, kContentX + kContentW, top + 4.0f);
}

D2D1_RECT_F SettingsWindow::TrailColorAutoRect() const
{
    return Rect(188.0f, 454.0f, 262.0f, 484.0f);
}

D2D1_RECT_F SettingsWindow::TrailColorCustomRect() const
{
    return Rect(270.0f, 454.0f, 344.0f, 484.0f);
}

D2D1_RECT_F SettingsWindow::TrailColorSwatchRect() const
{
    return Rect(352.0f, 454.0f, 384.0f, 484.0f);
}

D2D1_RECT_F SettingsWindow::TrailResetRect() const
{
    return Rect(188.0f, 492.0f, 308.0f, 516.0f);
}

D2D1_RECT_F SettingsWindow::ResetAllRect() const
{
    return Rect(400.0f, 510.0f, 576.0f, 534.0f);
}

D2D1_RECT_F SettingsWindow::AboutRepoRect() const
{
    return Rect(188.0f, 258.0f, 300.0f, 288.0f);
}

D2D1_RECT_F SettingsWindow::AboutCopyRect() const
{
    return Rect(308.0f, 258.0f, 408.0f, 288.0f);
}

D2D1_RECT_F SettingsWindow::AboutReleasesRect() const
{
    return Rect(416.0f, 258.0f, 576.0f, 288.0f);
}

D2D1_RECT_F SettingsWindow::AboutBilibiliRect() const
{
    return Rect(188.0f, 430.0f, 576.0f, 464.0f);
}

D2D1_RECT_F SettingsWindow::StackToggleRect() const
{
    return Rect(536.0f, 88.0f, 576.0f, 110.0f);
}

D2D1_RECT_F SettingsWindow::StackDelayTrackRect() const
{
    return Rect(kContentX, 196.0f, kContentX + kContentW, 220.0f);
}

D2D1_RECT_F SettingsWindow::SkinSwatchRect(int index) const
{
    const float top = 226.0f + static_cast<float>(index) * 58.0f;
    return Rect(520.0f, top + 2.0f, 556.0f, top + 38.0f);
}

D2D1_RECT_F SettingsWindow::SkinImageButtonRect() const
{
    return Rect(kContentX, 452.0f, kContentX + 142.0f, 484.0f);
}

D2D1_RECT_F SettingsWindow::SkinImageClearRect() const
{
    return Rect(kContentX + 152.0f, 452.0f, kContentX + 230.0f, 484.0f);
}

D2D1_RECT_F SettingsWindow::KeySkinChipRect(int index) const
{
    constexpr float width = 91.0f;
    constexpr float gap = 8.0f;
    const float left = kContentX + static_cast<float>(index) * (width + gap);
    return Rect(left, 264.0f, left + width, 298.0f);
}

D2D1_RECT_F SettingsWindow::KeyTextAutoRect() const
{
    return Rect(400.0f, 482.0f, 454.0f, 510.0f);
}

D2D1_RECT_F SettingsWindow::KeyTextColorRect() const
{
    return Rect(462.0f, 482.0f, 500.0f, 510.0f);
}

D2D1_RECT_F SettingsWindow::KeyStyleColorRect() const
{
    return Rect(512.0f, 482.0f, 550.0f, 510.0f);
}

D2D1_RECT_F SettingsWindow::KeyPositionChipRect(int index) const
{
    constexpr float width = 91.0f;
    constexpr float gap = 8.0f;
    const float left = kContentX + static_cast<float>(index) * (width + gap);
    return Rect(left, 338.0f, left + width, 368.0f);
}

D2D1_RECT_F SettingsWindow::KeyDelayTrackRect() const
{
    return Rect(kContentX, 404.0f, kContentX + kContentW, 428.0f);
}

D2D1_RECT_F SettingsWindow::KeyScaleTrackRect() const
{
    return Rect(388.0f, 116.0f, 560.0f, 132.0f);
}

D2D1_RECT_F SettingsWindow::KeyOpacityTrackRect() const
{
    return Rect(388.0f, 144.0f, 560.0f, 160.0f);
}

D2D1_RECT_F SettingsWindow::KeyLettersRowRect() const
{
    return Rect(kContentX, 442.0f, kContentX + kContentW, 472.0f);
}

D2D1_RECT_F SettingsWindow::KeyLettersToggleRect() const
{
    return Rect(536.0f, 446.0f, 576.0f, 468.0f);
}

D2D1_RECT_F SettingsWindow::TimerToggleRect(int index) const
{
    const float top = 92.0f + static_cast<float>(index) * 72.0f;
    return Rect(536.0f, top, 576.0f, top + 22.0f);
}

D2D1_RECT_F SettingsWindow::TimerResetButtonRect() const
{
    return Rect(478.0f, 390.0f, 576.0f, 422.0f);
}

D2D1_RECT_F SettingsWindow::ClickColorRect(int index) const
{
    const float top = 84.0f + static_cast<float>(index) * 90.0f;
    return Rect(kContentX, top, kContentX + kContentW, top + 80.0f);
}

D2D1_RECT_F SettingsWindow::ClickToggleRect(int index) const
{
    const float top = 286.0f + static_cast<float>(index) * 54.0f;
    return Rect(536.0f, top, 576.0f, top + 22.0f);
}

D2D1_RECT_F SettingsWindow::ClickPresetChipRect(int index) const
{
    constexpr float width = 70.0f;
    constexpr float gap = 8.0f;
    const float left = kContentX + static_cast<float>(index) * (width + gap);
    return Rect(left, 420.0f, left + width, 450.0f);
}

D2D1_RECT_F SettingsWindow::ClickImportRect() const
{
    return Rect(kContentX, 462.0f, kContentX + kContentW, 492.0f);
}

D2D1_RECT_F SettingsWindow::ClickVolumeTrackRect() const
{
    return Rect(396.0f, 400.0f, 494.0f, 408.0f);
}

void SettingsWindow::SwitchPage(Page page)
{
    if (page_ == page)
        return;
    page_ = page;
    mouse_ = D2D1::Point2F(-1.0f, -1.0f);
    hoverNav_ = -1;
    hotkeyPillHover_ = false;
    hoverThemeChip_ = -1;
    const std::vector<FocusId> order = BuildFocusOrder();
    if (std::find(order.begin(), order.end(), focus_) == order.end())
        focus_ = FocusId::None;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::OnLeftDown(float x, float y)
{
    if (!settings_)
        return;

    if (hotkeyRecording_)
    {
        if (InRect(x, y, HotkeyPillRect()))
        {
            CancelHotkeyCapture();
            return;
        }
        CancelHotkeyCapture();
        // Fall through so a click on another control still performs its action
        // after the current shortcut has been restored.
    }

    if (colorPickerOpen_)
    {
        if (InRect(x, y, HexInputRect()))
        {
            colorFocus_ = ColorFocus::Hex;
            if (!hexFocused_)
            {
                FocusHexInput(true);
            }
            else
            {
                const float relative = (x - kHexCellsLeft) / kHexCellWidth;
                hexCaret_ = (std::max)(0, (std::min)(
                    static_cast<int>(hexEdit_.size()),
                    static_cast<int>(std::lround(relative))));
                hexSelectAll_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        }
        if (InRect(x, y, SaturationValueRect()))
        {
            colorFocus_ = ColorFocus::SaturationValue;
            hexFocused_ = false;
            hexSelectAll_ = false;
            colorDrag_ = ColorDrag::SaturationValue;
            SetCapture(hwnd_);
            UpdateColorFromSaturationValue(x, y);
            return;
        }
        if (InRect(x, y, HueRect()))
        {
            colorFocus_ = ColorFocus::Hue;
            hexFocused_ = false;
            hexSelectAll_ = false;
            colorDrag_ = ColorDrag::Hue;
            SetCapture(hwnd_);
            UpdateColorFromHue(x);
            return;
        }
        if (InRect(x, y, ColorCancelRect()))
        {
            colorFocus_ = ColorFocus::Cancel;
            CancelColorPicker();
            return;
        }
        if (InRect(x, y, ColorDoneRect()))
        {
            colorFocus_ = ColorFocus::Done;
            CommitColorPicker();
            return;
        }
        if (!InRect(x, y, ColorCardRect()))
        {
            CancelColorPicker();
        }
        return;
    }

    if (InRect(x, y, CloseRect()))
    {
        CloseRequested();
        return;
    }

    for (int i = 0; i < kPageCount; ++i)
    {
        if (InRect(x, y, NavRect(i)))
        {
            SetFocusId(static_cast<FocusId>(
                static_cast<int>(FocusId::NavHighlight) + i));
            SwitchPage(static_cast<Page>(i));
            return;
        }
    }

    if (y < 64.0f)
    {
        draggingWindow_ = true;
        GetCursorPos(&dragStartCursor_);
        GetWindowRect(hwnd_, &dragStartWindow_);
        SetCapture(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (page_ == Page::Highlight)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (InRect(x, y, HaloStyleChipRect(i)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::HaloStyleRing) + i));
                settings_->haloStyle = FormatHaloStyle(static_cast<HaloStyle>(i));
                if (host_)
                    host_->OnSettingsStyleChanged();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
        }
        if (InRect(x, y, EnableRowRect()))
        {
            SetFocusId(FocusId::Enable);
            settings_->enabled = !settings_->enabled;
            if (host_)
                host_->OnSettingsEnabledChanged();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (InRect(x, y, RadiusTrackRect()))
        {
            SetFocusId(FocusId::Radius);
            sliderDrag_ = SliderDrag::Radius;
            SetCapture(hwnd_);
            UpdateSliderValue(x, true);
            return;
        }
        if (InRect(x, y, OpacityTrackRect()))
        {
            SetFocusId(FocusId::Opacity);
            sliderDrag_ = SliderDrag::Opacity;
            SetCapture(hwnd_);
            UpdateSliderValue(x, false);
            return;
        }
    }
    else if (page_ == Page::Appearance)
    {
        if (InRect(x, y, CustomColorRect()))
        {
            SetFocusId(FocusId::CustomColor);
            OpenColorPicker();
            return;
        }
        for (int i = 0; i < kColorCount; ++i)
        {
            const float centerX = kSwatchStartX + kSwatchGap * i;
            if (InRect(x, y, Rect(centerX - 22.0f, kSwatchY - 24.0f, centerX + 22.0f, kSwatchY + 24.0f)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::Color0) + i));
                if (settings_->accentRgb != kColors[i])
                {
                    settings_->accentRgb = kColors[i];
                    if (host_)
                        host_->OnSettingsStyleChanged();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            }
        }
    }
    else if (page_ == Page::Keys)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (InRect(x, y, KeySkinChipRect(i)))
            {
                constexpr FocusId focusOrder[4] = {
                    FocusId::KeySkinClassic,
                    FocusId::KeySkinMinimal,
                    FocusId::KeySkinMechanical,
                    FocusId::KeySkinWin98,
                };
                constexpr KeySkin skinOrder[4] = {
                    KeySkin::Classic,
                    KeySkin::Minimal,
                    KeySkin::Mechanical,
                    KeySkin::Win98,
                };
                SetFocusId(focusOrder[i]);
                SetKeySkin(skinOrder[i]);
                return;
            }
        }

        for (int i = 0; i < 4; ++i)
        {
            if (InRect(x, y, KeyPositionChipRect(i)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::KeyPositionBottom) + i));
                if (i == 3)
                {
                    if (host_)
                        host_->OnSettingsKeyPositionCustomRequested();
                }
                else
                {
                    SetKeyPosition(static_cast<KeyPosition>(i));
                }
                return;
            }
        }

        if (InRect(x, y, KeyDelayTrackRect()))
        {
            SetFocusId(FocusId::KeyDelay);
            sliderDrag_ = SliderDrag::KeyDelay;
            SetCapture(hwnd_);
            UpdateKeyDelayValue(x);
            return;
        }
        if (InRect(x, y, KeyScaleTrackRect()))
        {
            SetFocusId(FocusId::KeyScale);
            sliderDrag_ = SliderDrag::KeyScale;
            SetCapture(hwnd_);
            UpdateKeyScaleValue(x);
            return;
        }
        if (InRect(x, y, KeyOpacityTrackRect()))
        {
            SetFocusId(FocusId::KeyOpacity);
            sliderDrag_ = SliderDrag::KeyOpacity;
            SetCapture(hwnd_);
            UpdateKeyOpacityValue(x);
            return;
        }

        if (InRect(x, y, KeyLettersRowRect()))
        {
            SetFocusId(FocusId::KeyLetters);
            SetKeyLetters(!settings_->showLetterKeys);
            return;
        }
        if (InRect(x, y, KeyTextAutoRect()))
        {
            SetFocusId(FocusId::KeyTextAuto);
            settings_->keyTextAuto = true;
            if (host_)
                host_->OnSettingsKeyDisplayChanged();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (InRect(x, y, KeyTextColorRect()))
        {
            SetFocusId(FocusId::KeyTextColor);
            OpenColorPicker(ColorTarget::KeyText);
            return;
        }
        if (InRect(x, y, KeyStyleColorRect()))
        {
            SetFocusId(FocusId::KeyStyleColor);
            OpenColorPicker(ColorTarget::KeyStyle);
            return;
        }
    }
    else if (page_ == Page::Click)
    {
        for (int i = 0; i < 2; ++i)
        {
            if (InRect(x, y, ClickColorRect(i)))
            {
                SetFocusId(i == 0 ? FocusId::ClickColorLeft : FocusId::ClickColorRight);
                OpenColorPicker(i == 0 ? ColorTarget::ClickLeft : ColorTarget::ClickRight);
                return;
            }
            if (InRect(x, y, ClickToggleRect(i)))
            {
                SetFocusId(i == 0 ? FocusId::ClickRipple : FocusId::ClickSound);
                if (i == 0)
                    settings_->clickRippleEnabled = !settings_->clickRippleEnabled;
                else
                    settings_->clickSoundEnabled = !settings_->clickSoundEnabled;
                if (host_)
                    host_->OnSettingsClickChanged();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
        }
        if (InRect(x, y, ClickVolumeTrackRect()))
        {
            SetFocusId(FocusId::ClickVolume);
            sliderDrag_ = SliderDrag::ClickVolume;
            SetCapture(hwnd_);
            UpdateClickVolumeValue(x);
            return;
        }
        for (int i = 0; i < 5; ++i)
        {
            if (InRect(x, y, ClickPresetChipRect(i)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::ClickPresetDefault) + i));
                settings_->clickSoundPreset =
                    FormatClickSoundPreset(static_cast<ClickSoundPreset>(i));
                if (host_)
                {
                    host_->OnSettingsClickChanged();
                    host_->OnSettingsPreviewClickSound(false);
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
        }
        if (InRect(x, y, ClickImportRect()))
        {
            SetFocusId(FocusId::ClickImport);
            ImportClickSound();
            return;
        }
    }
    else if (page_ == Page::Trail)
    {
        if (InRect(x, y, TrailEnableRect()))
        {
            SetFocusId(FocusId::TrailEnabled);
            settings_->trailEnabled = !settings_->trailEnabled;
            if (host_)
                host_->OnSettingsTrailChanged(settings_->trailEnabled);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        for (int i = 0; i < 6; ++i)
        {
            if (InRect(x, y, TrailTrackRect(i)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::TrailLength) + i));
                sliderDrag_ = static_cast<SliderDrag>(
                    static_cast<int>(SliderDrag::TrailLength) + i);
                SetCapture(hwnd_);
                UpdateTrailValue(i, x);
                return;
            }
        }
        if (InRect(x, y, TrailColorAutoRect()))
        {
            SetFocusId(FocusId::TrailColorAuto);
            SetTrailColorAuto(true);
            return;
        }
        if (InRect(x, y, TrailColorCustomRect()) || InRect(x, y, TrailColorSwatchRect()))
        {
            SetFocusId(InRect(x, y, TrailColorCustomRect())
                ? FocusId::TrailColorCustom
                : FocusId::TrailColorSwatch);
            const bool wasAutomatic = settings_->trailColorAuto;
            // Capture the automatic state for cancel, then switch the visible
            // draft to custom while the picker is open.
            OpenColorPicker(ColorTarget::Trail);
            if (wasAutomatic)
                SetTrailColorAuto(false);
            return;
        }
        if (InRect(x, y, TrailResetRect()))
        {
            SetFocusId(FocusId::TrailReset);
            ResetTrailDefaults();
            return;
        }
    }
    else if (page_ == Page::Stack)
    {
        if (InRect(x, y, Rect(kContentX, 84.0f, kContentX + kContentW, 124.0f)))
        {
            SetFocusId(FocusId::StackEnabled);
            settings_->keyStack = !settings_->keyStack;
            if (host_)
                host_->OnSettingsKeyDisplayChanged();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (InRect(x, y, StackDelayTrackRect()))
        {
            SetFocusId(FocusId::StackDelay);
            sliderDrag_ = SliderDrag::StackDelay;
            SetCapture(hwnd_);
            UpdateStackDelayValue(x);
            return;
        }
    }
    else if (page_ == Page::Timer)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float top = 84.0f + static_cast<float>(i) * 72.0f;
            if (InRect(x, y, Rect(kContentX, top - 4.0f, kContentX + kContentW, top + 46.0f)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::TimerEnabled) + i));
                switch (i)
                {
                case 0:
                    settings_->timerEnabled = !settings_->timerEnabled;
                    break;
                case 1:
                    settings_->timerAutoStart = !settings_->timerAutoStart;
                    break;
                case 2:
                    settings_->timerShowHours = !settings_->timerShowHours;
                    break;
                default:
                    settings_->timerResetOnStop = !settings_->timerResetOnStop;
                    break;
                }
                if (host_)
                    host_->OnSettingsTimerChanged();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
        }
        if (InRect(x, y, TimerResetButtonRect()))
        {
            SetFocusId(FocusId::TimerReset);
            if (host_)
                host_->OnSettingsTimerResetRequested();
            return;
        }
    }
    else if (page_ == Page::General)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (InRect(x, y, ThemeChipRect(i)))
            {
                SetFocusId(static_cast<FocusId>(
                    static_cast<int>(FocusId::ThemeSystem) + i));
                SetThemeMode(static_cast<ThemeMode>(i));
                return;
            }
        }
        if (InRect(x, y, HotkeyPillRect()))
        {
            SetFocusId(FocusId::Hotkey);
            StartHotkeyCapture();
            return;
        }
        if (InRect(x, y, AutoStartRowRect()))
        {
            SetFocusId(FocusId::AutoStart);
            settings_->startWithWindows = !settings_->startWithWindows;
            if (host_)
                host_->OnSettingsAutoStartChanged(settings_->startWithWindows);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (InRect(x, y, ShowKeysRowRect()))
        {
            SetFocusId(FocusId::ShowKeys);
            settings_->showKeys = !settings_->showKeys;
            if (host_)
                host_->OnSettingsShowKeysChanged(settings_->showKeys);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (InRect(x, y, TrailRowRect()))
        {
            SetFocusId(FocusId::TrailEnabled);
            settings_->trailEnabled = !settings_->trailEnabled;
            if (host_)
                host_->OnSettingsTrailChanged(settings_->trailEnabled);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (InRect(x, y, ResetAllRect()))
        {
            SetFocusId(FocusId::ResetAll);
            ResetAllDefaults();
            return;
        }
    }
    else if (page_ == Page::About)
    {
        if (InRect(x, y, AboutRepoRect()))
        {
            SetFocusId(FocusId::AboutRepo);
            OpenExternalUrl(kRepositoryUrl);
            return;
        }
        if (InRect(x, y, AboutCopyRect()))
        {
            SetFocusId(FocusId::AboutCopy);
            CopyTextToClipboard(hwnd_, kRepositoryUrl);
            return;
        }
        if (InRect(x, y, AboutReleasesRect()))
        {
            SetFocusId(FocusId::AboutReleases);
            OpenExternalUrl(kReleasesUrl);
            return;
        }
        if (InRect(x, y, AboutBilibiliRect()))
        {
            SetFocusId(FocusId::AboutBilibili);
            OpenExternalUrl(kBilibiliUrl);
            return;
        }
    }
}

void SettingsWindow::OnLeftUp()
{
    if (draggingWindow_ || sliderDrag_ != SliderDrag::None || colorDrag_ != ColorDrag::None)
    {
        const bool previewVolume = sliderDrag_ == SliderDrag::ClickVolume;
        draggingWindow_ = false;
        sliderDrag_ = SliderDrag::None;
        colorDrag_ = ColorDrag::None;
        if (GetCapture() == hwnd_)
            ReleaseCapture();
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (previewVolume && host_)
            host_->OnSettingsPreviewClickSound(false);
    }
}

void SettingsWindow::UpdateSliderValue(float x, bool radius)
{
    if (!settings_)
        return;
    const float fraction = Clamp01((x - kContentX) / kContentW);
    if (radius)
        settings_->radiusDip = 20 + static_cast<int>(std::lround(fraction * 60.0f));
    else
        settings_->opacityPercent = 30 + static_cast<int>(std::lround(fraction * 70.0f));

    if (host_)
        host_->OnSettingsStyleChanged();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::OnMouseMove(float x, float y)
{
    mouse_ = D2D1::Point2F(x, y);

    if (colorPickerOpen_)
    {
        if (colorDrag_ == ColorDrag::SaturationValue)
        {
            UpdateColorFromSaturationValue(x, y);
        }
        else if (colorDrag_ == ColorDrag::Hue)
        {
            UpdateColorFromHue(x);
        }
        else
        {
            const bool hoverCancel = InRect(x, y, ColorCancelRect());
            const bool hoverDone = InRect(x, y, ColorDoneRect());
            if (hoverCancel != hoverColorCancel_ || hoverDone != hoverColorDone_)
            {
                hoverColorCancel_ = hoverCancel;
                hoverColorDone_ = hoverDone;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
        }

        SetCursor(CursorForPoint(x, y));
        if (!trackingLeave_)
        {
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd_;
            TrackMouseEvent(&track);
            trackingLeave_ = true;
        }
        return;
    }

    if (draggingWindow_)
    {
        POINT cursor = {};
        GetCursorPos(&cursor);
        const int dx = cursor.x - dragStartCursor_.x;
        const int dy = cursor.y - dragStartCursor_.y;
        SetWindowPos(
            hwnd_,
            nullptr,
            dragStartWindow_.left + dx,
            dragStartWindow_.top + dy,
            0,
            0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    if (sliderDrag_ == SliderDrag::Radius)
    {
        UpdateSliderValue(x, true);
        return;
    }
    if (sliderDrag_ == SliderDrag::Opacity)
    {
        UpdateSliderValue(x, false);
        return;
    }
    if (sliderDrag_ == SliderDrag::KeyDelay)
    {
        UpdateKeyDelayValue(x);
        return;
    }
    if (sliderDrag_ == SliderDrag::KeyScale)
    {
        UpdateKeyScaleValue(x);
        return;
    }
    if (sliderDrag_ == SliderDrag::KeyOpacity)
    {
        UpdateKeyOpacityValue(x);
        return;
    }
    if (sliderDrag_ == SliderDrag::ClickVolume)
    {
        UpdateClickVolumeValue(x);
        return;
    }
    if (sliderDrag_ >= SliderDrag::TrailLength &&
        sliderDrag_ <= SliderDrag::TrailGlow)
    {
        const int index = static_cast<int>(sliderDrag_) -
            static_cast<int>(SliderDrag::TrailLength);
        UpdateTrailValue(index, x);
        return;
    }
    if (sliderDrag_ == SliderDrag::StackDelay)
    {
        UpdateStackDelayValue(x);
        return;
    }

    int nav = -1;
    for (int i = 0; i < kPageCount; ++i)
    {
        if (InRect(x, y, NavRect(i)))
        {
            nav = i;
            break;
        }
    }
    const bool closeHover = InRect(x, y, CloseRect());
    const bool hotkeyPillHover = page_ == Page::General && InRect(x, y, HotkeyPillRect());
    int themeHover = -1;
    if (page_ == Page::General)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (InRect(x, y, ThemeChipRect(i)))
            {
                themeHover = i;
                break;
            }
        }
    }
    if (nav != hoverNav_ || closeHover != hoverClose_ ||
        hotkeyPillHover != hotkeyPillHover_ || themeHover != hoverThemeChip_)
    {
        hoverNav_ = nav;
        hoverClose_ = closeHover;
        hotkeyPillHover_ = hotkeyPillHover;
        hoverThemeChip_ = themeHover;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    SetCursor(CursorForPoint(x, y));

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

void SettingsWindow::OnMouseLeave()
{
    trackingLeave_ = false;
    if (hoverNav_ != -1 || hoverClose_ || hoverColorCancel_ || hoverColorDone_ ||
        hotkeyPillHover_ || hoverThemeChip_ != -1)
    {
        hoverNav_ = -1;
        hoverClose_ = false;
        hoverColorCancel_ = false;
        hoverColorDone_ = false;
        hotkeyPillHover_ = false;
        hoverThemeChip_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    mouse_ = D2D1::Point2F(-1.0f, -1.0f);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
}

void SettingsWindow::OnCaptureChanged()
{
    draggingWindow_ = false;
    sliderDrag_ = SliderDrag::None;
    colorDrag_ = ColorDrag::None;
}

void SettingsWindow::OpenColorPicker(ColorTarget target)
{
    if (!settings_)
        return;
    colorTarget_ = target;
    colorBeforeKeyTextAuto_ = settings_->keyTextAuto;
    colorBeforeKeyTextRgb_ = settings_->keyTextRgb;
    colorBeforeKeyStyleAuto_ = settings_->keyStyleAuto;
    switch (target)
    {
    case ColorTarget::Accent:
        colorBeforePicker_ = settings_->accentRgb;
        break;
    case ColorTarget::KeyText:
        colorBeforePicker_ = settings_->keyTextAuto ? 0xFFFFFFu : settings_->keyTextRgb;
        break;
    case ColorTarget::KeyStyle:
        colorBeforePicker_ = settings_->keyStyleAuto ? 0xFFFFFFu : settings_->keyStyleRgb;
        break;
    case ColorTarget::KeyFill:
        colorBeforePicker_ = settings_->keyFillRgb;
        break;
    case ColorTarget::KeyBorder:
        colorBeforePicker_ = settings_->keyBorderRgb;
        break;
    case ColorTarget::ClickLeft:
        colorBeforePicker_ = settings_->clickLeftRgb;
        break;
    case ColorTarget::ClickRight:
        colorBeforePicker_ = settings_->clickRightRgb;
        break;
    case ColorTarget::Trail:
        colorBeforeTrailAuto_ = settings_->trailColorAuto;
        colorBeforeTrailRgb_ = settings_->trailColorRgb;
        colorBeforePicker_ = settings_->trailColorAuto
            ? settings_->accentRgb
            : settings_->trailColorRgb;
        break;
    }
    colorDraftRgb_ = colorBeforePicker_;
    RgbToHsv(colorDraftRgb_, colorHue_, colorSaturation_, colorValue_);
    colorPickerOpen_ = true;
    hexSkipChar_ = L'\0';
    colorDrag_ = ColorDrag::None;
    hoverColorCancel_ = false;
    hoverColorDone_ = false;
    SyncHexFromColor();
    colorFocus_ = ColorFocus::Hex;
    hexSelectAll_ = true;
    hexFocused_ = true;
    SetFocus(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::CancelColorPicker()
{
    if (!colorPickerOpen_)
        return;
    colorPickerOpen_ = false;
    colorDrag_ = ColorDrag::None;
    hexFocused_ = false;
    hexSelectAll_ = false;
    hexSkipChar_ = L'\0';
    switch (colorTarget_)
    {
    case ColorTarget::Accent:
        focus_ = FocusId::CustomColor;
        for (int i = 0; i < kColorCount; ++i)
        {
            if (settings_->accentRgb == kColors[i])
            {
                focus_ = static_cast<FocusId>(static_cast<int>(FocusId::Color0) + i);
                break;
            }
        }
        break;
    case ColorTarget::KeyText:
        focus_ = FocusId::KeyTextColor;
        break;
    case ColorTarget::KeyStyle:
        focus_ = FocusId::KeyStyleColor;
        break;
    case ColorTarget::KeyFill:
        focus_ = FocusId::SkinFill;
        break;
    case ColorTarget::KeyBorder:
        focus_ = FocusId::SkinBorder;
        break;
    case ColorTarget::ClickLeft:
        focus_ = FocusId::ClickColorLeft;
        break;
    case ColorTarget::ClickRight:
        focus_ = FocusId::ClickColorRight;
        break;
    case ColorTarget::Trail:
        focus_ = FocusId::TrailColorSwatch;
        break;
    }
    if (GetCapture() == hwnd_)
        ReleaseCapture();
    switch (colorTarget_)
    {
    case ColorTarget::Accent:
        if (settings_->accentRgb != colorBeforePicker_)
        {
            settings_->accentRgb = colorBeforePicker_;
            if (host_)
                host_->OnSettingsStyleChanged();
        }
        break;
    case ColorTarget::KeyText:
        settings_->keyTextRgb = colorBeforeKeyTextRgb_;
        settings_->keyTextAuto = colorBeforeKeyTextAuto_;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyStyle:
        settings_->keyStyleAuto = colorBeforeKeyStyleAuto_;
        settings_->keyStyleRgb = colorBeforePicker_;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyFill:
        settings_->keyFillRgb = colorBeforePicker_;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyBorder:
        settings_->keyBorderRgb = colorBeforePicker_;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::ClickLeft:
        settings_->clickLeftRgb = colorBeforePicker_;
        if (host_)
            host_->OnSettingsClickChanged();
        break;
    case ColorTarget::ClickRight:
        settings_->clickRightRgb = colorBeforePicker_;
        if (host_)
            host_->OnSettingsClickChanged();
        break;
    case ColorTarget::Trail:
        settings_->trailColorAuto = colorBeforeTrailAuto_;
        settings_->trailColorRgb = colorBeforeTrailRgb_;
        if (host_)
            host_->OnSettingsTrailSettingsChanged();
        break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::CommitColorPicker()
{
    if (!colorPickerOpen_)
        return;
    colorPickerOpen_ = false;
    colorDrag_ = ColorDrag::None;
    hoverColorCancel_ = false;
    hoverColorDone_ = false;
    hexFocused_ = false;
    hexSelectAll_ = false;
    hexSkipChar_ = L'\0';
    switch (colorTarget_)
    {
    case ColorTarget::Accent:
        focus_ = FocusId::CustomColor;
        break;
    case ColorTarget::KeyText:
        focus_ = FocusId::KeyTextColor;
        break;
    case ColorTarget::KeyStyle:
        focus_ = FocusId::KeyStyleColor;
        break;
    case ColorTarget::KeyFill:
        focus_ = FocusId::SkinFill;
        break;
    case ColorTarget::KeyBorder:
        focus_ = FocusId::SkinBorder;
        break;
    case ColorTarget::ClickLeft:
        focus_ = FocusId::ClickColorLeft;
        break;
    case ColorTarget::ClickRight:
        focus_ = FocusId::ClickColorRight;
        break;
    case ColorTarget::Trail:
        focus_ = FocusId::TrailColorSwatch;
        break;
    }
    if (GetCapture() == hwnd_)
        ReleaseCapture();
    switch (colorTarget_)
    {
    case ColorTarget::KeyText:
        settings_->keyTextRgb = colorDraftRgb_ & 0xFFFFFFu;
        settings_->keyTextAuto = false;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyStyle:
        settings_->keyStyleRgb = colorDraftRgb_ & 0xFFFFFFu;
        settings_->keyStyleAuto = false;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyFill:
        settings_->keyFillRgb = colorDraftRgb_ & 0xFFFFFFu;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyBorder:
        settings_->keyBorderRgb = colorDraftRgb_ & 0xFFFFFFu;
        if (host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::ClickLeft:
        settings_->clickLeftRgb = colorDraftRgb_ & 0xFFFFFFu;
        if (host_)
            host_->OnSettingsClickChanged();
        break;
    case ColorTarget::ClickRight:
        settings_->clickRightRgb = colorDraftRgb_ & 0xFFFFFFu;
        if (host_)
            host_->OnSettingsClickChanged();
        break;
    case ColorTarget::Trail:
        settings_->trailColorRgb = colorDraftRgb_ & 0xFFFFFFu;
        settings_->trailColorAuto = false;
        if (host_)
            host_->OnSettingsTrailSettingsChanged();
        break;
    default:
        break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::ApplyColorDraft(unsigned rgb, bool notify)
{
    colorDraftRgb_ = rgb & 0xFFFFFFu;
    switch (colorTarget_)
    {
    case ColorTarget::Accent:
        settings_->accentRgb = colorDraftRgb_;
        if (notify && host_)
            host_->OnSettingsStyleChanged();
        break;
    case ColorTarget::KeyText:
        settings_->keyTextRgb = colorDraftRgb_;
        settings_->keyTextAuto = false;
        if (notify && host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyStyle:
        settings_->keyStyleRgb = colorDraftRgb_;
        settings_->keyStyleAuto = false;
        if (notify && host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyFill:
        settings_->keyFillRgb = colorDraftRgb_;
        if (notify && host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::KeyBorder:
        settings_->keyBorderRgb = colorDraftRgb_;
        if (notify && host_)
            host_->OnSettingsKeyDisplayChanged();
        break;
    case ColorTarget::ClickLeft:
        settings_->clickLeftRgb = colorDraftRgb_;
        if (notify && host_)
            host_->OnSettingsClickChanged();
        break;
    case ColorTarget::ClickRight:
        settings_->clickRightRgb = colorDraftRgb_;
        if (notify && host_)
            host_->OnSettingsClickChanged();
        break;
    case ColorTarget::Trail:
        settings_->trailColorRgb = colorDraftRgb_;
        settings_->trailColorAuto = false;
        if (notify && host_)
            host_->OnSettingsTrailSettingsChanged();
        break;
    }
    SyncHexFromColor();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::UpdateColorFromSaturationValue(float x, float y)
{
    const D2D1_RECT_F sv = SaturationValueRect();
    colorSaturation_ = Clamp01((x - sv.left) / (sv.right - sv.left));
    colorValue_ = 1.0f - Clamp01((y - sv.top) / (sv.bottom - sv.top));
    ApplyColorDraft(HsvToRgb(colorHue_, colorSaturation_, colorValue_), true);
}

void SettingsWindow::UpdateColorFromHue(float x)
{
    const D2D1_RECT_F hue = HueRect();
    colorHue_ = Clamp01((x - hue.left) / (hue.right - hue.left)) * 360.0f;
    ApplyColorDraft(HsvToRgb(colorHue_, colorSaturation_, colorValue_), true);
}

void SettingsWindow::SyncHexFromColor()
{
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"%06X", colorDraftRgb_ & 0xFFFFFFu);
    hexEdit_ = buffer;
    hexCaret_ = 6;
    hexSelectAll_ = false;
}

void SettingsWindow::NormalizeHexState()
{
    if (hexEdit_.size() > 6)
        hexEdit_.resize(6);
    const int size = static_cast<int>(hexEdit_.size());
    if (hexCaret_ < 0)
        hexCaret_ = 0;
    else if (hexCaret_ > size)
        hexCaret_ = size;
}

void SettingsWindow::ApplyHexInput()
{
    NormalizeHexState();
    if (!settings_ || hexEdit_.size() != 6)
        return;
    wchar_t* end = nullptr;
    const unsigned long value = wcstoul(hexEdit_.c_str(), &end, 16);
    if (!end || end != hexEdit_.c_str() + 6)
        return;
    const unsigned rgb = static_cast<unsigned>(value) & 0xFFFFFFu;
    ApplyColorDraft(rgb, true);
    RgbToHsv(rgb, colorHue_, colorSaturation_, colorValue_);
}

void SettingsWindow::FocusHexInput(bool selectAll)
{
    hexFocused_ = true;
    hexSkipChar_ = L'\0';
    hexSelectAll_ = selectAll;
    if (selectAll)
        hexCaret_ = static_cast<int>(hexEdit_.size());
    SetFocus(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool SettingsWindow::HandleHexChar(wchar_t character)
{
    NormalizeHexState();
    hexSkipChar_ = L'\0';
    if (character >= L'a' && character <= L'f')
        character = static_cast<wchar_t>(character - L'a' + L'A');
    const bool isHex = (character >= L'0' && character <= L'9') ||
        (character >= L'A' && character <= L'F');
    if (!isHex)
        return false;

    if (hexSelectAll_)
    {
        hexEdit_.clear();
        hexCaret_ = 0;
        hexSelectAll_ = false;
    }
    if (hexEdit_.size() >= 6)
        return true;
    NormalizeHexState();
    hexEdit_.insert(static_cast<size_t>(hexCaret_), 1, character);
    ++hexCaret_;
    if (hexEdit_.size() == 6)
        ApplyHexInput();
    InvalidateRect(hwnd_, nullptr, FALSE);
    return true;
}

bool SettingsWindow::HandleHexKey(WPARAM key)
{
    NormalizeHexState();
    const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (control && (key == 'A' || key == 'a'))
    {
        hexSelectAll_ = true;
        hexCaret_ = static_cast<int>(hexEdit_.size());
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }
    if (control && (key == 'V' || key == 'v'))
    {
        PasteHexFromClipboard();
        return true;
    }
    if (key == VK_INSERT && (GetKeyState(VK_SHIFT) & 0x8000) != 0)
    {
        PasteHexFromClipboard();
        return true;
    }

    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
    {
        if (hexEdit_.size() >= 6)
            return true;
        if (hexSelectAll_)
        {
            hexEdit_.clear();
            hexCaret_ = 0;
            hexSelectAll_ = false;
        }
        NormalizeHexState();
        hexEdit_.insert(
            static_cast<size_t>(hexCaret_),
            1,
            static_cast<wchar_t>(L'0' + (key - VK_NUMPAD0)));
        ++hexCaret_;
        hexSkipChar_ = static_cast<wchar_t>(L'0' + (key - VK_NUMPAD0));
        if (hexEdit_.size() == 6)
            ApplyHexInput();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    switch (key)
    {
    case VK_BACK:
        if (hexSelectAll_)
        {
            hexEdit_.clear();
            hexCaret_ = 0;
            hexSelectAll_ = false;
        }
        else if (hexCaret_ > 0)
        {
            hexEdit_.erase(static_cast<size_t>(hexCaret_ - 1), 1);
            --hexCaret_;
        }
        break;
    case VK_DELETE:
        if (hexSelectAll_)
        {
            hexEdit_.clear();
            hexCaret_ = 0;
            hexSelectAll_ = false;
        }
        else if (hexCaret_ < static_cast<int>(hexEdit_.size()))
        {
            hexEdit_.erase(static_cast<size_t>(hexCaret_), 1);
        }
        break;
    case VK_LEFT:
        if (hexSelectAll_)
        {
            hexSelectAll_ = false;
            hexCaret_ = 0;
        }
        else if (hexCaret_ > 0)
        {
            --hexCaret_;
        }
        break;
    case VK_RIGHT:
        if (hexSelectAll_)
        {
            hexSelectAll_ = false;
            hexCaret_ = static_cast<int>(hexEdit_.size());
        }
        else if (hexCaret_ < static_cast<int>(hexEdit_.size()))
        {
            ++hexCaret_;
        }
        break;
    case VK_HOME:
        hexSelectAll_ = false;
        hexCaret_ = 0;
        break;
    case VK_END:
        hexSelectAll_ = false;
        hexCaret_ = static_cast<int>(hexEdit_.size());
        break;
    default:
        return false;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    return true;
}

void SettingsWindow::PasteHexFromClipboard()
{
    if (!OpenClipboard(hwnd_))
        return;
    std::wstring parsed;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
    {
        if (const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data)))
        {
            const wchar_t* cursor = text;
            while (*cursor == L' ' || *cursor == L'\t' || *cursor == L'\r' || *cursor == L'\n')
                ++cursor;
            if (*cursor == L'#')
                ++cursor;
            else if (cursor[0] == L'0' && (cursor[1] == L'x' || cursor[1] == L'X'))
                cursor += 2;

            for (; *cursor && parsed.size() < 6; ++cursor)
            {
                wchar_t character = *cursor;
                if (character >= L'a' && character <= L'f')
                    character = static_cast<wchar_t>(character - L'a' + L'A');
                const bool isHex = (character >= L'0' && character <= L'9') ||
                    (character >= L'A' && character <= L'F');
                if (!isHex)
                    break;
                parsed.push_back(character);
            }
            GlobalUnlock(data);
        }
    }
    CloseClipboard();

    if (parsed.empty())
        return;
    hexEdit_ = parsed;
    hexCaret_ = static_cast<int>(hexEdit_.size());
    hexSelectAll_ = false;
    if (hexEdit_.size() == 6)
        ApplyHexInput();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

HCURSOR SettingsWindow::CursorForPoint(float x, float y) const
{
    if (colorPickerOpen_)
    {
        if (InRect(x, y, HexInputRect()))
            return LoadCursorW(nullptr, IDC_IBEAM);
        if (InRect(x, y, SaturationValueRect()) || InRect(x, y, HueRect()))
            return LoadCursorW(nullptr, IDC_CROSS);
        if (InRect(x, y, ColorCancelRect()) || InRect(x, y, ColorDoneRect()))
            return LoadCursorW(nullptr, IDC_HAND);
        return LoadCursorW(nullptr, IDC_ARROW);
    }
    if (HitTestInteractive(x, y))
        return LoadCursorW(nullptr, IDC_HAND);
    return LoadCursorW(nullptr, IDC_ARROW);
}

bool SettingsWindow::HitTestInteractive(float x, float y) const
{
    if (InRect(x, y, CloseRect()))
        return true;
    for (int i = 0; i < kPageCount; ++i)
    {
        if (InRect(x, y, NavRect(i)))
            return true;
    }
    if (page_ == Page::Highlight)
    {
        return InRect(x, y, EnableRowRect()) ||
            InRect(x, y, RadiusTrackRect()) ||
            InRect(x, y, OpacityTrackRect());
    }
    if (page_ == Page::Appearance)
    {
        if (InRect(x, y, CustomColorRect()))
            return true;
        for (int i = 0; i < kColorCount; ++i)
        {
            const float centerX = kSwatchStartX + kSwatchGap * i;
            if (InRect(x, y, Rect(centerX - 22.0f, kSwatchY - 24.0f, centerX + 22.0f, kSwatchY + 24.0f)))
                return true;
        }
    }
    if (page_ == Page::Keys)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (InRect(x, y, KeySkinChipRect(i)))
                return true;
        }
        for (int i = 0; i < 4; ++i)
        {
            if (InRect(x, y, KeyPositionChipRect(i)))
                return true;
        }
        return InRect(x, y, KeyDelayTrackRect()) ||
            InRect(x, y, KeyScaleTrackRect()) || InRect(x, y, KeyOpacityTrackRect()) ||
            InRect(x, y, KeyLettersRowRect()) ||
            InRect(x, y, KeyTextAutoRect()) || InRect(x, y, KeyTextColorRect()) ||
            InRect(x, y, KeyStyleColorRect());
    }
    if (page_ == Page::Trail)
    {
        if (InRect(x, y, TrailEnableRect()) ||
            InRect(x, y, TrailColorAutoRect()) ||
            InRect(x, y, TrailColorCustomRect()) ||
            InRect(x, y, TrailColorSwatchRect()) ||
            InRect(x, y, TrailResetRect()))
        {
            return true;
        }
        for (int i = 0; i < 6; ++i)
        {
            if (InRect(x, y, TrailTrackRect(i)))
                return true;
        }
        return false;
    }
    if (page_ == Page::Stack)
    {
        return InRect(x, y, Rect(kContentX, 84.0f, kContentX + kContentW, 124.0f)) ||
            InRect(x, y, StackDelayTrackRect());
    }
    if (page_ == Page::Click)
    {
        if (InRect(x, y, ClickColorRect(0)) || InRect(x, y, ClickColorRect(1)) ||
            InRect(x, y, ClickToggleRect(0)) || InRect(x, y, ClickToggleRect(1)) ||
            InRect(x, y, ClickImportRect()) || InRect(x, y, ClickVolumeTrackRect()))
        {
            return true;
        }
        for (int i = 0; i < 5; ++i)
        {
            if (InRect(x, y, ClickPresetChipRect(i)))
                return true;
        }
        return false;
    }
    if (page_ == Page::Timer)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float top = 84.0f + static_cast<float>(i) * 72.0f;
            if (InRect(x, y, Rect(kContentX, top - 4.0f, kContentX + kContentW, top + 46.0f)))
                return true;
        }
        return InRect(x, y, TimerResetButtonRect());
    }
    if (page_ == Page::General)
    {
        if (!InRect(x, y, AutoStartRowRect()) && !InRect(x, y, HotkeyPillRect()) &&
            !InRect(x, y, ShowKeysRowRect()) && !InRect(x, y, TrailRowRect()) &&
            !InRect(x, y, ResetAllRect()))
        {
            for (int i = 0; i < 3; ++i)
            {
                if (InRect(x, y, ThemeChipRect(i)))
                    return true;
            }
            return false;
        }
        return true;
    }
    if (page_ == Page::About)
    {
        return InRect(x, y, AboutRepoRect()) ||
            InRect(x, y, AboutCopyRect()) ||
            InRect(x, y, AboutReleasesRect()) ||
            InRect(x, y, AboutBilibiliRect());
    }
    return false;
}

bool SettingsWindow::StartKeyboardCaptureHook()
{
    if (hotkeyHook_)
        return true;
    hotkeyHookTarget_ = hwnd_;
    hotkeyHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyHookProc, instance_, 0);
    if (!hotkeyHook_)
    {
        hotkeyHookTarget_ = nullptr;
        return false;
    }
    return true;
}

void SettingsWindow::StopKeyboardCaptureHook()
{
    if (hotkeyHook_)
    {
        UnhookWindowsHookEx(hotkeyHook_);
        hotkeyHook_ = nullptr;
    }
    hotkeyHookTarget_ = nullptr;
}

LRESULT CALLBACK SettingsWindow::HotkeyHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && hotkeyHookTarget_)
    {
        const KBDLLHOOKSTRUCT* data = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (data)
        {
            const UINT key = data->vkCode;
            if (!IsModifierKey(key))
            {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                {
                    UINT modifiers = 0;
                    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
                        modifiers |= MOD_CONTROL;
                    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0)
                        modifiers |= MOD_ALT;
                    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
                        modifiers |= MOD_SHIFT;
                    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                        (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0)
                    {
                        modifiers |= MOD_WIN;
                    }
                    PostMessageW(hotkeyHookTarget_, kHotkeyCaptureMessage, key, modifiers);
                    return 1;
                }
                if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                    return 1;
            }
        }
    }
    return CallNextHookEx(hotkeyHook_, code, wParam, lParam);
}

void SettingsWindow::StartHotkeyCapture()
{
    if (!settings_ || !host_ || hotkeyRecording_)
        return;
    std::wstring error;
    if (!host_->BeginHotkeyRecording(error))
    {
        hotkeyError_ = error.empty() ? L"暂时无法开始录制快捷键。" : error;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (!StartKeyboardCaptureHook())
    {
        host_->CancelHotkeyRecording();
        hotkeyError_ = L"无法启动键盘录制，请重试。";
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    hotkeyRecording_ = true;
    hotkeyError_.clear();
    hotkeyPillHover_ = true;
    SetFocus(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::CancelHotkeyCapture()
{
    if (!hotkeyRecording_)
        return;
    StopKeyboardCaptureHook();
    hotkeyRecording_ = false;
    hotkeyError_.clear();
    if (host_)
        host_->CancelHotkeyRecording();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::HandleHotkeyKey(WPARAM key)
{
    if (!hotkeyRecording_)
        return;
    if (key == VK_ESCAPE)
    {
        CancelHotkeyCapture();
        return;
    }
    if (IsModifierKey(static_cast<UINT>(key)))
        return;

    UINT modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
        modifiers |= MOD_CONTROL;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0)
        modifiers |= MOD_ALT;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
        modifiers |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0)
        modifiers |= MOD_WIN;
    HandleCapturedHotkey(static_cast<UINT>(key), modifiers);
}

void SettingsWindow::HandleCapturedHotkey(UINT key, UINT modifiers)
{
    if (!hotkeyRecording_)
        return;
    if (key == VK_ESCAPE)
    {
        CancelHotkeyCapture();
        return;
    }
    if (IsModifierKey(key))
        return;
    if (modifiers == 0)
    {
        hotkeyError_ = L"请按住 Ctrl、Alt、Shift 或 Win 再试。";
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (!IsSupportedHotkeyKey(key))
    {
        hotkeyError_ = L"这个按键暂不支持，请换一个。";
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    StopKeyboardCaptureHook();
    const HotkeyBinding binding{modifiers, key};
    std::wstring error;
    if (host_ && host_->CommitHotkey(binding, error))
    {
        hotkeyRecording_ = false;
        hotkeyError_.clear();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    hotkeyRecording_ = false;
    hotkeyError_ = error.empty() ? L"无法注册该快捷键，请换一个组合。" : error;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::CloseRequested()
{
    if (hotkeyRecording_)
        CancelHotkeyCapture();
    colorPickerOpen_ = false;
    colorDrag_ = ColorDrag::None;
    hoverColorCancel_ = false;
    hoverColorDone_ = false;
    hexFocused_ = false;
    hexSelectAll_ = false;
    Hide();
    if (host_)
        host_->OnSettingsClosed();
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SettingsWindow* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }
    if (!self)
        return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_SETFOCUS:
        ImmAssociateContext(hwnd, nullptr);
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT paint = {};
        BeginPaint(hwnd, &paint);
        if (self->EnsureRenderTarget())
        {
            self->renderTarget_->BeginDraw();
            self->Draw();
            const HRESULT hr = self->renderTarget_->EndDraw();
            if (FAILED(hr))
            {
                self->DiscardRenderTarget();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case kHotkeyCaptureMessage:
        if (self->hotkeyRecording_)
            self->HandleCapturedHotkey(static_cast<UINT>(wParam), static_cast<UINT>(lParam));
        return 0;
    case WM_RBUTTONUP:
    {
        const float x = static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_;
        const float y = static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_;
        if (self->page_ == Page::Keys && InRect(x, y, self->KeyStyleColorRect()))
        {
            self->settings_->keyStyleAuto = true;
            self->SetFocusId(FocusId::KeyStyleColor);
            if (self->host_)
                self->host_->OnSettingsKeyDisplayChanged();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN:
        self->OnLeftDown(
            static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_,
            static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_);
        return 0;
    case WM_LBUTTONUP:
        self->OnLeftUp();
        return 0;
    case WM_MOUSEMOVE:
        self->OnMouseMove(
            static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_,
            static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_);
        return 0;
    case WM_MOUSELEAVE:
        self->OnMouseLeave();
        return 0;
    case WM_CAPTURECHANGED:
        self->OnCaptureChanged();
        return 0;
    case WM_CLOSE:
        self->CloseRequested();
        return 0;
    case WM_SYSKEYDOWN:
        if (self->hotkeyRecording_)
        {
            self->HandleHotkeyKey(wParam);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (self->hotkeyRecording_)
        {
            self->HandleHotkeyKey(wParam);
            return 0;
        }
        if (self->colorPickerOpen_)
        {
            if (self->HandleColorPickerKey(wParam))
                return 0;
            break;
        }
        if (self->HandleFocusKey(wParam))
            return 0;
        if (wParam == VK_ESCAPE)
        {
            self->CloseRequested();
            return 0;
        }
        break;
    case WM_CHAR:
        if (self->hotkeyRecording_)
            return 0;
        if (self->colorPickerOpen_ && self->hexFocused_)
        {
            const wchar_t character = static_cast<wchar_t>(wParam);
            if (self->hexSkipChar_ != L'\0')
            {
                const bool same = character == self->hexSkipChar_;
                self->hexSkipChar_ = L'\0';
                if (same)
                    return 0;
            }
            self->HandleHexChar(character);
            return 0;
        }
        return 0;
    case WM_SYSCHAR:
        return 0;
    case WM_PASTE:
        if (self->colorPickerOpen_ && self->hexFocused_)
        {
            self->PasteHexFromClipboard();
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        if (self->hotkeyRecording_)
            self->CancelHotkeyCapture();
        if (self->hexFocused_)
        {
            self->hexFocused_ = false;
            self->hexSelectAll_ = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT cursor = {};
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);
            const float x = static_cast<float>(cursor.x) / self->scale_;
            const float y = static_cast<float>(cursor.y) / self->scale_;
            SetCursor(self->CursorForPoint(x, y));
            return TRUE;
        }
        break;
    case WM_SIZE:
        self->DiscardRenderTarget();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED:
    {
        const UINT dpi = HIWORD(wParam);
        if (dpi >= 72 && dpi <= 480)
            self->scale_ = static_cast<float>(dpi) / 96.0f;
        self->DiscardRenderTarget();
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        const int width = static_cast<int>(std::lround(kWidth * self->scale_));
        const int height = static_cast<int>(std::lround(kHeight * self->scale_));
        SetWindowPos(
            hwnd,
            nullptr,
            suggested ? suggested->left : 0,
            suggested ? suggested->top : 0,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_SHOWWINDOW:
        self->visible_ = wParam != FALSE;
        return 0;
    case WM_DISPLAYCHANGE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
