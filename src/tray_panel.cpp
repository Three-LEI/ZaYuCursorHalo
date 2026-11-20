#include "tray_panel.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace
{
constexpr wchar_t kTrayPanelClass[] = L"CursorHalo.TrayPanel.P1";
constexpr float kWidth = 280.0f;
constexpr float kHeight = 430.0f;
constexpr float kToggleTop = 64.0f;
constexpr float kToggleBottom = 102.0f;
constexpr float kShowKeysTop = 108.0f;
constexpr float kShowKeysBottom = 146.0f;
constexpr float kTimerTop = 152.0f;
constexpr float kTimerBottom = 190.0f;
constexpr float kTrailTop = 196.0f;
constexpr float kTrailBottom = 234.0f;
constexpr float kClickRippleTop = 240.0f;
constexpr float kClickRippleBottom = 278.0f;
constexpr float kClickSoundTop = 284.0f;
constexpr float kClickSoundBottom = 322.0f;
constexpr float kSettingsTop = 340.0f;
constexpr float kSettingsBottom = 378.0f;
constexpr float kExitTop = 382.0f;
constexpr float kExitBottom = 420.0f;

D2D1_COLOR_F Rgb(unsigned rgb, float alpha = 1.0f)
{
    return D2D1::ColorF(
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha);
}

D2D1_RECT_F Rect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, right, bottom);
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
    return dpi >= 72 && dpi <= 480 ? static_cast<UINT>(dpi) : 96;
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

TrayPanel::~TrayPanel()
{
    Destroy();
}

bool TrayPanel::Create(HINSTANCE instance, TrayPanelHost* host)
{
    if (hwnd_)
        return true;
    instance_ = instance;
    host_ = host;
    if (!instance_ || !host_)
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
    if (!CreateTextFormats() || !RegisterWindowClass())
        return false;

    scale_ = static_cast<float>(SystemDpi()) / 96.0f;
    const int pixelWidth = static_cast<int>(std::lround(kWidth * scale_));
    const int pixelHeight = static_cast<int>(std::lround(kHeight * scale_));
    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kTrayPanelClass,
        L"CursorHalo",
        WS_POPUP,
        0,
        0,
        pixelWidth,
        pixelHeight,
        nullptr,
        nullptr,
        instance_,
        this);
    return hwnd_ != nullptr;
}

void TrayPanel::Destroy()
{
    if (hwnd_)
    {
        visible_ = false;
        if (GetCapture() == hwnd_)
            ReleaseCapture();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (classRegistered_)
    {
        UnregisterClassW(kTrayPanelClass, instance_);
        classRegistered_ = false;
    }
    DiscardRenderTarget();
    brandFormat_.Reset();
    rowFormat_.Reset();
    rowStrongFormat_.Reset();
    captionFormat_.Reset();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();
    host_ = nullptr;
}

bool TrayPanel::ShowAt(
    const RECT& iconRect,
    bool enabled,
    unsigned accentRgb,
    const std::wstring& hotkeyText,
    bool showKeys,
    bool timerEnabled,
    bool trailEnabled,
    bool clickRipple,
    bool clickSound)
{
    if (!hwnd_)
        return false;

    enabled_ = enabled;
    accentRgb_ = accentRgb;
    hotkeyText_ = hotkeyText;
    showKeys_ = showKeys;
    timerEnabled_ = timerEnabled;
    trailEnabled_ = trailEnabled;
    clickRipple_ = clickRipple;
    clickSound_ = clickSound;

    RECT anchor = iconRect;
    if (anchor.right <= anchor.left || anchor.bottom <= anchor.top)
    {
        POINT cursor = {};
        GetCursorPos(&cursor);
        anchor = {cursor.x - 16, cursor.y - 16, cursor.x + 16, cursor.y + 16};
    }

    const POINT iconCenter = {
        (anchor.left + anchor.right) / 2,
        (anchor.top + anchor.bottom) / 2,
    };
    HMONITOR monitor = MonitorFromPoint(iconCenter, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo))
        return false;

    const UINT dpi = DpiForMonitor(monitor);
    scale_ = static_cast<float>(dpi) / 96.0f;
    const int width = static_cast<int>(std::lround(kWidth * scale_));
    const int height = static_cast<int>(std::lround(kHeight * scale_));
    const int margin = static_cast<int>(std::lround(8.0f * scale_));
    const RECT& work = monitorInfo.rcWork;

    int x = 0;
    int y = 0;
    if (iconCenter.y > work.bottom)
    {
        x = iconCenter.x - width / 2;
        y = work.bottom - height - margin;
    }
    else if (iconCenter.y < work.top)
    {
        x = iconCenter.x - width / 2;
        y = work.top + margin;
    }
    else if (iconCenter.x < work.left)
    {
        x = work.left + margin;
        y = iconCenter.y - height / 2;
    }
    else if (iconCenter.x > work.right)
    {
        x = work.right - width - margin;
        y = iconCenter.y - height / 2;
    }
    else
    {
        x = iconCenter.x - width / 2;
        y = anchor.top - height - margin;
    }

    const int minX = work.left + margin;
    const int maxX = work.right - width - margin;
    const int minY = work.top + margin;
    const int maxY = work.bottom - height - margin;
    x = maxX >= minX ? (std::max)(minX, (std::min)(maxX, x)) : work.left;
    y = maxY >= minY ? (std::max)(minY, (std::min)(maxY, y)) : work.top;

    DiscardRenderTarget();
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_NOACTIVATE);
    visible_ = true;
    hasFocus_ = false;
    hoverRow_ = Row::None;
    focusRow_ = Row::Toggle;
    InvalidateRect(hwnd_, nullptr, FALSE);
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    SetCapture(hwnd_);
    return true;
}

void TrayPanel::Hide()
{
    if (!hwnd_ || !visible_)
        return;
    visible_ = false;
    hasFocus_ = false;
    firstRunHint_ = false;
    hoverRow_ = Row::None;
    if (GetCapture() == hwnd_)
        ReleaseCapture();
    ShowWindow(hwnd_, SW_HIDE);
    if (host_)
        host_->OnTrayPanelClosed();
}

void TrayPanel::SetState(bool enabled, unsigned accentRgb)
{
    if (enabled_ == enabled && accentRgb_ == accentRgb)
        return;
    enabled_ = enabled;
    accentRgb_ = accentRgb;
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void TrayPanel::SetToggles(
    bool showKeys,
    bool timerEnabled,
    bool trailEnabled,
    bool clickRipple,
    bool clickSound)
{
    if (showKeys_ == showKeys && timerEnabled_ == timerEnabled &&
        trailEnabled_ == trailEnabled && clickRipple_ == clickRipple &&
        clickSound_ == clickSound)
    {
        return;
    }
    showKeys_ = showKeys;
    timerEnabled_ = timerEnabled;
    trailEnabled_ = trailEnabled;
    clickRipple_ = clickRipple;
    clickSound_ = clickSound;
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void TrayPanel::SetHotkey(const std::wstring& hotkeyText)
{
    if (hotkeyText_ == hotkeyText)
        return;
    hotkeyText_ = hotkeyText;
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void TrayPanel::SetDarkMode(bool dark)
{
    if (darkMode_ == dark)
        return;
    darkMode_ = dark;
    palette_ = PaletteFor(dark);
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void TrayPanel::SetFirstRunHint(bool enabled)
{
    if (firstRunHint_ == enabled)
        return;
    firstRunHint_ = enabled;
    if (hwnd_)
        InvalidateRect(hwnd_, nullptr, FALSE);
}

bool TrayPanel::RegisterWindowClass()
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &TrayPanel::WndProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kTrayPanelClass;
    if (RegisterClassExW(&windowClass))
    {
        classRegistered_ = true;
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool TrayPanel::CreateTextFormats()
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
    if (FAILED(dwriteFactory_->GetSystemFontCollection(fonts.GetAddressOf(), FALSE)) || !fonts)
        return false;

    const wchar_t* candidates[] = {L"Microsoft YaHei UI", L"Microsoft YaHei", L"Segoe UI"};
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
            dwriteFactory_.Get(), family, 15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            brandFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 13.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
            rowFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
            rowStrongFormat_.GetAddressOf()))
        return false;
    if (!CreateTextFormat(
            dwriteFactory_.Get(), family, 10.5f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
            captionFormat_.GetAddressOf()))
        return false;
    return true;
}

bool TrayPanel::EnsureRenderTarget()
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

void TrayPanel::DiscardRenderTarget()
{
    brush_.Reset();
    renderTarget_.Reset();
    targetWidth_ = 0;
    targetHeight_ = 0;
}

void TrayPanel::Draw()
{
    if (!renderTarget_)
        return;
    const D2D1_COLOR_F accent = Rgb(accentRgb_);

    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(palette_.surface);

    const D2D1_POINT_2F brandCenter = D2D1::Point2F(26.0f, 22.0f);
    brush_->SetColor(accent);
    renderTarget_->DrawEllipse(D2D1::Ellipse(brandCenter, 6.5f, 6.5f), brush_.Get(), 1.4f);
    renderTarget_->FillEllipse(D2D1::Ellipse(brandCenter, 1.8f, 1.8f), brush_.Get());
    DrawTextBlock(L"CursorHalo", brandFormat_.Get(), Rect(44.0f, 8.0f, 190.0f, 33.0f), palette_.text);
    DrawTextBlock(
        L"v0.3.0 · three-lei × DeepSeek",
        captionFormat_.Get(),
        Rect(44.0f, 32.0f, 268.0f, 48.0f),
        palette_.subtle);

    brush_->SetColor(enabled_ ? accent : palette_.muted);
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(190.0f, 22.0f), 2.6f, 2.6f), brush_.Get());
    DrawTextBlock(
        enabled_ ? L"高亮已开启" : L"高亮已暂停",
        captionFormat_.Get(),
        Rect(198.0f, 12.0f, 268.0f, 31.0f),
        palette_.muted);

    const D2D1_RECT_F toggleRow = Rect(12.0f, kToggleTop, 268.0f, kToggleBottom);
    if (hoverRow_ == Row::Toggle)
        FillRounded(toggleRow, 9.0f, palette_.hover);
    DrawTextBlock(L"启用高亮", rowStrongFormat_.Get(), Rect(24.0f, kToggleTop, 190.0f, kToggleBottom), palette_.text);
    DrawSwitch(Rect(216.0f, kToggleTop + 11.0f, 256.0f, kToggleTop + 33.0f), enabled_);

    const D2D1_RECT_F showKeysRow = Rect(12.0f, kShowKeysTop, 268.0f, kShowKeysBottom);
    if (hoverRow_ == Row::ShowKeys)
        FillRounded(showKeysRow, 9.0f, palette_.hover);
    DrawTextBlock(L"显示按键", rowStrongFormat_.Get(), Rect(24.0f, kShowKeysTop, 190.0f, kShowKeysBottom), palette_.text);
    DrawSwitch(Rect(216.0f, kShowKeysTop + 11.0f, 256.0f, kShowKeysTop + 33.0f), showKeys_);

    const D2D1_RECT_F timerRow = Rect(12.0f, kTimerTop, 268.0f, kTimerBottom);
    if (hoverRow_ == Row::Timer)
        FillRounded(timerRow, 9.0f, palette_.hover);
    DrawTextBlock(L"浮动计时器", rowStrongFormat_.Get(), Rect(24.0f, kTimerTop, 200.0f, kTimerBottom), palette_.text);
    DrawSwitch(Rect(216.0f, kTimerTop + 11.0f, 256.0f, kTimerTop + 33.0f), timerEnabled_);

    const D2D1_RECT_F trailRow = Rect(12.0f, kTrailTop, 268.0f, kTrailBottom);
    if (hoverRow_ == Row::Trail)
        FillRounded(trailRow, 9.0f, palette_.hover);
    DrawTextBlock(L"鼠标拖尾", rowStrongFormat_.Get(), Rect(24.0f, kTrailTop, 200.0f, kTrailBottom), palette_.text);
    DrawSwitch(Rect(216.0f, kTrailTop + 11.0f, 256.0f, kTrailTop + 33.0f), trailEnabled_);

    const D2D1_RECT_F rippleRow = Rect(12.0f, kClickRippleTop, 268.0f, kClickRippleBottom);
    if (hoverRow_ == Row::ClickRipple)
        FillRounded(rippleRow, 9.0f, palette_.hover);
    DrawTextBlock(L"点击波纹", rowStrongFormat_.Get(), Rect(24.0f, kClickRippleTop, 200.0f, kClickRippleBottom), palette_.text);
    DrawSwitch(Rect(216.0f, kClickRippleTop + 11.0f, 256.0f, kClickRippleTop + 33.0f), clickRipple_);

    const D2D1_RECT_F soundRow = Rect(12.0f, kClickSoundTop, 268.0f, kClickSoundBottom);
    if (hoverRow_ == Row::ClickSound)
        FillRounded(soundRow, 9.0f, palette_.hover);
    DrawTextBlock(L"点击音效", rowStrongFormat_.Get(), Rect(24.0f, kClickSoundTop, 200.0f, kClickSoundBottom), palette_.text);
    DrawSwitch(Rect(216.0f, kClickSoundTop + 11.0f, 256.0f, kClickSoundTop + 33.0f), clickSound_);

    brush_->SetColor(palette_.divider);
    renderTarget_->FillRectangle(Rect(20.0f, 330.0f, 260.0f, 331.0f), brush_.Get());

    const D2D1_RECT_F settingsRow = Rect(12.0f, kSettingsTop, 268.0f, kSettingsBottom);
    if (hoverRow_ == Row::Settings)
        FillRounded(settingsRow, 9.0f, palette_.hover);
    DrawSettingsIcon(D2D1::Point2F(26.0f, kSettingsTop + 18.0f), palette_.muted);
    DrawTextBlock(L"设置…", rowFormat_.Get(), Rect(48.0f, kSettingsTop, 230.0f, kSettingsBottom), palette_.text);
    DrawChevron(D2D1::Point2F(252.0f, kSettingsTop + 18.0f), palette_.muted);

    const D2D1_RECT_F exitRow = Rect(12.0f, kExitTop, 268.0f, kExitBottom);
    if (hoverRow_ == Row::Exit)
        FillRounded(exitRow, 9.0f, palette_.hover);
    DrawPowerIcon(D2D1::Point2F(26.0f, kExitTop + 18.0f), palette_.muted);
    DrawTextBlock(L"退出 CursorHalo", rowFormat_.Get(), Rect(48.0f, kExitTop, 250.0f, kExitBottom), palette_.text);

    D2D1_RECT_F focusRect = {};
    switch (focusRow_)
    {
    case Row::Toggle: focusRect = toggleRow; break;
    case Row::ShowKeys: focusRect = showKeysRow; break;
    case Row::Timer: focusRect = timerRow; break;
    case Row::Trail: focusRect = trailRow; break;
    case Row::ClickRipple: focusRect = rippleRow; break;
    case Row::ClickSound: focusRect = soundRow; break;
    case Row::Settings: focusRect = settingsRow; break;
    case Row::Exit: focusRect = exitRow; break;
    default: break;
    }
    if (focusRow_ != Row::None)
    {
        StrokeRounded(
            Rect(focusRect.left + 1.0f, focusRect.top + 1.0f, focusRect.right - 1.0f, focusRect.bottom - 1.0f),
            9.0f,
            D2D1::ColorF(palette_.text.r, palette_.text.g, palette_.text.b, 0.82f),
            1.5f);
    }

    StrokeRounded(Rect(0.5f, 0.5f, kWidth - 0.5f, kHeight - 0.5f), 0.0f, palette_.border, 1.0f);
}

void TrayPanel::DrawSwitch(const D2D1_RECT_F& rect, bool on)
{
    const float radius = (rect.bottom - rect.top) / 2.0f;
    FillRounded(rect, radius, on ? Rgb(accentRgb_) : palette_.switchOff);
    const float knobRadius = radius - 4.0f;
    const float centerX = on ? rect.right - radius : rect.left + radius;
    brush_->SetColor(palette_.onSurface);
    renderTarget_->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(centerX, (rect.top + rect.bottom) / 2.0f), knobRadius, knobRadius),
        brush_.Get());
}

void TrayPanel::DrawSettingsIcon(D2D1_POINT_2F center, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    const float left = center.x - 7.0f;
    const float right = center.x + 7.0f;
    const float ys[3] = {center.y - 5.0f, center.y, center.y + 5.0f};
    const float knobX[3] = {center.x + 2.5f, center.x - 3.0f, center.x + 4.0f};
    for (int i = 0; i < 3; ++i)
    {
        renderTarget_->DrawLine(D2D1::Point2F(left, ys[i]), D2D1::Point2F(right, ys[i]), brush_.Get(), 1.2f);
        renderTarget_->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(knobX[i], ys[i]), 2.0f, 2.0f),
            brush_.Get(),
            1.2f);
    }
}

void TrayPanel::DrawPowerIcon(D2D1_POINT_2F center, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    renderTarget_->DrawEllipse(D2D1::Ellipse(center, 5.5f, 5.5f), brush_.Get(), 1.3f);
    renderTarget_->DrawLine(
        D2D1::Point2F(center.x, center.y - 9.0f),
        D2D1::Point2F(center.x, center.y - 2.0f),
        brush_.Get(),
        1.3f);
}

void TrayPanel::DrawChevron(D2D1_POINT_2F center, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    renderTarget_->DrawLine(
        D2D1::Point2F(center.x - 2.0f, center.y - 4.0f),
        D2D1::Point2F(center.x + 2.0f, center.y),
        brush_.Get(),
        1.3f);
    renderTarget_->DrawLine(
        D2D1::Point2F(center.x + 2.0f, center.y),
        D2D1::Point2F(center.x - 2.0f, center.y + 4.0f),
        brush_.Get(),
        1.3f);
}

void TrayPanel::DrawTextBlock(
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

void TrayPanel::FillRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color)
{
    brush_->SetColor(color);
    renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush_.Get());
}

void TrayPanel::StrokeRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, float width)
{
    brush_->SetColor(color);
    renderTarget_->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush_.Get(), width);
}

TrayPanel::Row TrayPanel::RowAt(float x, float y) const
{
    if (x < 12.0f || x > 268.0f)
        return Row::None;
    if (y >= kToggleTop && y <= kToggleBottom)
        return Row::Toggle;
    if (y >= kShowKeysTop && y <= kShowKeysBottom)
        return Row::ShowKeys;
    if (y >= kTimerTop && y <= kTimerBottom)
        return Row::Timer;
    if (y >= kTrailTop && y <= kTrailBottom)
        return Row::Trail;
    if (y >= kClickRippleTop && y <= kClickRippleBottom)
        return Row::ClickRipple;
    if (y >= kClickSoundTop && y <= kClickSoundBottom)
        return Row::ClickSound;
    if (y >= kSettingsTop && y <= kSettingsBottom)
        return Row::Settings;
    if (y >= kExitTop && y <= kExitBottom)
        return Row::Exit;
    return Row::None;
}

void TrayPanel::ActivateRow(Row row)
{
    switch (row)
    {
    case Row::Toggle:
        enabled_ = !enabled_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (host_)
            host_->OnTrayPanelToggle();
        break;
    case Row::ShowKeys:
        showKeys_ = !showKeys_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (host_)
            host_->OnTrayPanelShowKeysChanged(showKeys_);
        break;
    case Row::Timer:
        timerEnabled_ = !timerEnabled_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (host_)
            host_->OnTrayPanelTimerChanged(timerEnabled_);
        break;
    case Row::Trail:
        trailEnabled_ = !trailEnabled_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (host_)
            host_->OnTrayPanelTrailChanged(trailEnabled_);
        break;
    case Row::ClickRipple:
        clickRipple_ = !clickRipple_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (host_)
            host_->OnTrayPanelClickRippleChanged(clickRipple_);
        break;
    case Row::ClickSound:
        clickSound_ = !clickSound_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (host_)
            host_->OnTrayPanelClickSoundChanged(clickSound_);
        break;
    case Row::Settings:
        Hide();
        if (host_)
            host_->OnTrayPanelSettings();
        break;
    case Row::Exit:
        Hide();
        if (host_)
            host_->OnTrayPanelExit();
        break;
    default:
        break;
    }
}

void TrayPanel::OnMouseMove(float x, float y)
{
    const Row row = RowAt(x, y);
    bool changed = row != hoverRow_;
    hoverRow_ = row;
    if (row != Row::None && row != focusRow_)
    {
        focusRow_ = row;
        changed = true;
    }
    if (changed)
        InvalidateRect(hwnd_, nullptr, FALSE);
    SetCursor(LoadCursorW(nullptr, row == Row::None ? IDC_ARROW : IDC_HAND));
}

void TrayPanel::OnMouseLeave()
{
    hoverRow_ = Row::None;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TrayPanel::MoveFocus(bool backwards)
{
    const Row order[] = {
        Row::Toggle,
        Row::ShowKeys,
        Row::Timer,
        Row::Trail,
        Row::ClickRipple,
        Row::ClickSound,
        Row::Settings,
        Row::Exit,
    };
    constexpr int count = static_cast<int>(sizeof(order) / sizeof(order[0]));
    int index = 0;
    for (int i = 0; i < count; ++i)
    {
        if (order[i] == focusRow_)
        {
            index = i;
            break;
        }
    }
    index = backwards ? (index + count - 1) % count : (index + 1) % count;
    focusRow_ = order[index];
    hoverRow_ = Row::None;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK TrayPanel::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    TrayPanel* self = reinterpret_cast<TrayPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<TrayPanel*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }
    if (!self)
        return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message)
    {
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
    case WM_MOUSEMOVE:
        self->OnMouseMove(
            static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_,
            static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_);
        return 0;
    case WM_MOUSELEAVE:
        self->OnMouseLeave();
        return 0;
    case WM_LBUTTONDOWN:
    {
        const float x = static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_;
        const float y = static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_;
        if (x < 0.0f || x > kWidth || y < 0.0f || y > kHeight)
            self->Hide();
        return 0;
    }
    case WM_LBUTTONUP:
    {
        const float x = static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_;
        const float y = static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_;
        if (x < 0.0f || x > kWidth || y < 0.0f || y > kHeight)
        {
            self->Hide();
            return 0;
        }
        const Row row = self->RowAt(x, y);
        if (row != Row::None)
            self->ActivateRow(row);
        return 0;
    }
    case WM_RBUTTONDOWN:
    {
        const float x = static_cast<float>(GET_X_LPARAM(lParam)) / self->scale_;
        const float y = static_cast<float>(GET_Y_LPARAM(lParam)) / self->scale_;
        if (x < 0.0f || x > kWidth || y < 0.0f || y > kHeight)
            self->Hide();
        return 0;
    }
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            self->Hide();
            return 0;
        case VK_TAB:
            self->MoveFocus((GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;
        case VK_UP:
        case VK_LEFT:
            self->MoveFocus(true);
            return 0;
        case VK_DOWN:
        case VK_RIGHT:
            self->MoveFocus(false);
            return 0;
        case VK_RETURN:
        case VK_SPACE:
            self->ActivateRow(self->focusRow_);
            return 0;
        default:
            break;
        }
        break;
    case WM_CHAR:
        return 0;
    case WM_SETFOCUS:
        self->hasFocus_ = true;
        return 0;
    case WM_KILLFOCUS:
        self->hasFocus_ = false;
        if (self->visible_)
            self->Hide();
        return 0;
    case WM_CAPTURECHANGED:
        if (self->visible_)
            self->Hide();
        return 0;
    case WM_CLOSE:
        self->Hide();
        return 0;
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
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT cursor = {};
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);
            const float x = static_cast<float>(cursor.x) / self->scale_;
            const float y = static_cast<float>(cursor.y) / self->scale_;
            SetCursor(LoadCursorW(nullptr, self->RowAt(x, y) == Row::None ? IDC_ARROW : IDC_HAND));
            return TRUE;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
