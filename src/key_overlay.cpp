#include "key_overlay.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace
{
constexpr wchar_t kOverlayClass[] = L"CursorHalo.KeyOverlay.P1";
constexpr wchar_t kCatcherClass[] = L"CursorHalo.KeyPlacementCatcher.P1";
constexpr float kCapHeight = 40.0f;
constexpr float kPlusGap = 8.0f;
constexpr float kPlusSize = 12.0f;
constexpr float kCapPaddingX = 14.0f;
constexpr float kCapMinWidth = 34.0f;
constexpr float kMargin = 10.0f;
constexpr float kBottomMargin = 28.0f;
constexpr float kFadeMs = 180.0f;
constexpr float kEnterMs = 140.0f;
constexpr float kExitMs = 160.0f;
constexpr UINT kTimerIntervalMs = 16;

D2D1_COLOR_F Color(float r, float g, float b, float a = 1.0f)
{
    return D2D1::ColorF(r, g, b, a);
}

D2D1_RECT_F Rect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, right, bottom);
}

void FillRounded(
    ID2D1RenderTarget* target,
    ID2D1SolidColorBrush* brush,
    const D2D1_RECT_F& rect,
    float radius,
    const D2D1_COLOR_F& color)
{
    brush->SetColor(color);
    target->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush);
}

void StrokeRounded(
    ID2D1RenderTarget* target,
    ID2D1SolidColorBrush* brush,
    const D2D1_RECT_F& rect,
    float radius,
    const D2D1_COLOR_F& color,
    float width)
{
    brush->SetColor(color);
    target->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush, width);
}

void DrawTextBlock(
    ID2D1RenderTarget* target,
    ID2D1SolidColorBrush* brush,
    const std::wstring& text,
    IDWriteTextFormat* format,
    const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color)
{
    if (text.empty())
        return;
    brush->SetColor(color);
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, brush);
}

ULONGLONG NowMs()
{
    return GetTickCount64();
}

bool CreateDWriteFormat(
    IDWriteFactory* factory,
    const wchar_t* family,
    float size,
    DWRITE_FONT_WEIGHT weight,
    DWRITE_TEXT_ALIGNMENT alignment,
    DWRITE_PARAGRAPH_ALIGNMENT paragraph,
    IDWriteTextFormat** format)
{
    const HRESULT hr = factory->CreateTextFormat(
        family,
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"",
        format);
    if (FAILED(hr) || !*format)
        return false;
    (*format)->SetTextAlignment(alignment);
    (*format)->SetParagraphAlignment(paragraph);
    (*format)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return true;
}
} // namespace

KeyOverlay::~KeyOverlay()
{
    Destroy();
}

bool KeyOverlay::Create(HINSTANCE instance)
{
    instance_ = instance;
    palette_ = PaletteFor(darkMode_);

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf())) || !d2dFactory_)
        return false;
    if (FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()))) ||
        !dwriteFactory_)
    {
        return false;
    }
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wicFactory_))) ||
        !wicFactory_)
    {
        return false;
    }
    if (!CreateTextFormat())
        return false;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &KeyOverlay::WndProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kOverlayClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    classRegistered_ = true;

    wc.lpfnWndProc = &KeyOverlay::CatcherProc;
    wc.lpszClassName = kCatcherClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    catcherClassRegistered_ = true;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        kOverlayClass,
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
    return true;
}

bool KeyOverlay::CreateTextFormat()
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

    if (!CreateDWriteFormat(
            dwriteFactory_.Get(),
            family,
            16.0f,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
            keyFormat_.GetAddressOf()))
    {
        return false;
    }
    return CreateDWriteFormat(
        dwriteFactory_.Get(),
        family,
        10.5f,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_TEXT_ALIGNMENT_CENTER,
        DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        placementHintFormat_.GetAddressOf());
}

void KeyOverlay::Destroy()
{
    hook_.Stop();
    StopTimer();
    DestroyCatcher();
    if (hwnd_)
    {
        ShowWindow(hwnd_, SW_HIDE);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    DiscardSurface();
    keyFormat_.Reset();
    placementHintFormat_.Reset();
    rowLayer_.Reset();
    backgroundBitmap_.Reset();
    rowLayer_.Reset();
    brush_.Reset();
    gdiInterop_.Reset();
    renderTarget_.Reset();
    bitmap_.Reset();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();
    wicFactory_.Reset();
    placementMode_ = false;
    placementArmed_ = false;
    placementTarget_ = nullptr;
    placementMessage_ = 0;
    if (classRegistered_)
    {
        UnregisterClassW(kOverlayClass, instance_);
        classRegistered_ = false;
    }
    if (catcherClassRegistered_)
    {
        UnregisterClassW(kCatcherClass, instance_);
        catcherClassRegistered_ = false;
    }
    enabled_ = false;
    suspended_ = false;
    visible_ = false;
    physicalKeys_.clear();
    labels_.clear();
}

bool KeyOverlay::SetEnabled(bool enabled)
{
    if (enabled_ == enabled)
    {
        if (enabled && !suspended_ && !hook_.IsRunning())
            return hook_.Start(hwnd_, kKeyMessage);
        return true;
    }

    enabled_ = enabled;
    if (!enabled)
    {
        hook_.Stop();
        HideNow();
        return true;
    }

    if (suspended_ || !hwnd_)
        return true;

    if (!hook_.Start(hwnd_, kKeyMessage))
    {
        enabled_ = false;
        return false;
    }
    return true;
}

void KeyOverlay::SetSuspended(bool suspended)
{
    if (suspended_ == suspended)
        return;
    suspended_ = suspended;
    if (suspended_)
    {
        hook_.Stop();
        HideForSuspend();
    }
    else if (enabled_ && hwnd_)
    {
        if (!hook_.Start(hwnd_, kKeyMessage))
            enabled_ = false;
        if (enabled_ && (!labels_.empty() || !history_.empty()))
        {
            const ULONGLONG now = NowMs();
            labelsStartedAt_ = now;
            for (HistoryRow& row : history_)
            {
                row.appearAt = now;
                row.exitAt = 0;
                row.hideAt = now + static_cast<ULONGLONG>(stackDelayMs_);
            }
            Render();
            if (stackKeys_)
            {
                StartTimer();
            }
            else if (!labels_.empty())
            {
                hideAt_ = NowMs() + static_cast<ULONGLONG>(delayMs_);
                StartTimer();
            }
        }
    }
}

void KeyOverlay::SetDarkMode(bool dark)
{
    if (darkMode_ == dark)
        return;
    darkMode_ = dark;
    palette_ = PaletteFor(dark);
    if (visible_)
        Render();
}

bool KeyOverlay::CreateCatcher()
{
    if (catcher_)
        return true;
    if (!hwnd_ || !catcherClassRegistered_)
        return false;

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0 || height <= 0)
        return false;

    catcher_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kCatcherClass,
        L"",
        WS_POPUP,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        instance_,
        this);
    if (!catcher_)
        return false;

    // Keep the window hit-testable but effectively invisible. A fully
    // transparent layered window can become click-through on some systems.
    SetLayeredWindowAttributes(catcher_, 0, 1, LWA_ALPHA);
    ShowWindow(catcher_, SW_SHOWNOACTIVATE);
    SetWindowPos(
        catcher_,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return true;
}

void KeyOverlay::DestroyCatcher()
{
    if (catcher_)
    {
        ShowWindow(catcher_, SW_HIDE);
        DestroyWindow(catcher_);
        catcher_ = nullptr;
    }
}

bool KeyOverlay::BeginPlacement(HWND target, UINT message)
{
    if (!target || message == 0 || !hwnd_)
        return false;
    if (placementMode_)
        EndPlacement(false);

    HideNow();
    placementTarget_ = target;
    placementMessage_ = message;
    placementMode_ = true;
    placementArmed_ = false;
    placementStarted_ = NowMs();
    placementValid_ = false;
    physicalKeys_.clear();
    latchedKeys_.clear();
    labels_.clear();
    labels_.push_back(L"Ctrl");
    labels_.push_back(L"S");

    if (!CreateCatcher())
    {
        placementMode_ = false;
        placementTarget_ = nullptr;
        placementMessage_ = 0;
        DestroyCatcher();
        return false;
    }

    Render();
    if (!visible_)
    {
        EndPlacement(false);
        return false;
    }
    StartTimer();
    return true;
}

void KeyOverlay::CancelPlacement()
{
    if (!placementMode_)
        return;
    if (placementTarget_ && placementMessage_)
        PostMessageW(placementTarget_, placementMessage_, 0, 0);
    EndPlacement(false);
}

void KeyOverlay::ConfirmPlacement()
{
    if (!placementMode_ || !placementArmed_)
        return;

    POINT cursor = {};
    GetCursorPos(&cursor);
    if (placementTarget_ && placementMessage_)
    {
        PostMessageW(
            placementTarget_,
            placementMessage_,
            1,
            MAKELPARAM(cursor.x, cursor.y));
    }
    EndPlacement(true);
}

void KeyOverlay::EndPlacement(bool confirm)
{
    (void)confirm;
    placementMode_ = false;
    placementArmed_ = false;
    StopTimer();
    DestroyCatcher();
    HideNow();
    placementTarget_ = nullptr;
    placementMessage_ = 0;
}

void KeyOverlay::OnPlacementTimer()
{
    if (!placementMode_)
    {
        StopTimer();
        return;
    }

    UpdatePlacementPosition();
    const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const ULONGLONG now = NowMs();
    if (!placementArmed_ && !leftDown && now - placementStarted_ >= 200)
        placementArmed_ = true;

    if (placementArmed_ && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
        CancelPlacement();
}

void KeyOverlay::UpdatePlacementPosition()
{
    if (!placementMode_ || !visible_ || pixelWidth_ == 0 || pixelHeight_ == 0)
        return;

    POINT cursor = {};
    GetCursorPos(&cursor);
    if (cursor.x < placement_.work.left || cursor.x >= placement_.work.right ||
        cursor.y < placement_.work.top || cursor.y >= placement_.work.bottom)
    {
        placementValid_ = false;
        if (!EnsurePlacement())
            return;
    }
    const int width = static_cast<int>(pixelWidth_);
    const int height = static_cast<int>(pixelHeight_);
    int x = cursor.x - width / 2;
    int y = cursor.y - height / 2;
    const int edge = static_cast<int>(std::lround(8.0f * placement_.scale));
    const int maxLeft = placement_.work.right - width - edge;
    const int maxTop = placement_.work.bottom - height - edge;
    if (x > maxLeft)
        x = maxLeft;
    if (x < placement_.work.left + edge)
        x = placement_.work.left + edge;
    if (y > maxTop)
        y = maxTop;
    if (y < placement_.work.top + edge)
        y = placement_.work.top + edge;

    if (x == destX_ && y == destY_)
        return;
    destX_ = x;
    destY_ = y;
    Present(1.0f);
}

void KeyOverlay::Configure(
    KeySkin skin,
    KeyPosition position,
    int delayMs,
    bool showLetterKeys,
    bool keyStack,
    int keyStackDelayMs,
    bool keyTextAuto,
    unsigned keyTextRgb,
    bool keyStyleAuto,
    unsigned keyStyleRgb,
    unsigned keyFillRgb,
    unsigned keyBorderRgb,
    const std::wstring& keyBackgroundImage,
    unsigned accentRgb,
    int customX,
    int customY)
{
    const bool positionChanged =
        position_ != position || customX_ != customX || customY_ != customY;
    skin_ = skin;
    position_ = position;
    delayMs_ = ClampKeyDelay(delayMs);
    showLetterKeys_ = showLetterKeys;
    stackKeys_ = keyStack;
    stackDelayMs_ = (std::max)(0, (std::min)(3000, keyStackDelayMs));
    if (!stackKeys_)
        history_.clear();
    keyTextAuto_ = keyTextAuto;
    keyTextRgb_ = keyTextRgb & 0xFFFFFFu;
    keyStyleAuto_ = keyStyleAuto;
    keyStyleRgb_ = keyStyleRgb & 0xFFFFFFu;
    keyFillRgb_ = keyFillRgb & 0xFFFFFFu;
    keyBorderRgb_ = keyBorderRgb & 0xFFFFFFu;
    if (keyBackgroundImage_ != keyBackgroundImage)
    {
        keyBackgroundImage_ = keyBackgroundImage;
        backgroundSource_.Reset();
        backgroundBitmap_.Reset();
        backgroundPath_.clear();
        LoadBackgroundImage();
    }
    accentRgb_ = accentRgb;
    customX_ = customX;
    customY_ = customY;
    if (positionChanged)
        placementValid_ = false;
    if (visible_)
    {
        std::vector<std::wstring> labels;
        if (BuildDisplay(labels))
        {
            labels_ = std::move(labels);
            labelsStale_ = false;
            Render();
        }
        else
        {
            HideNow();
        }
    }
}

void KeyOverlay::OnKey(WPARAM vk, bool down)
{
    if (!enabled_ || suspended_ || !hwnd_)
        return;

    const UINT key = static_cast<UINT>(vk);
    if (key == 0 || key == 0xFF || key == VK_PROCESSKEY || key == VK_PACKET)
        return;

    if (down)
    {
        if (std::find(physicalKeys_.begin(), physicalKeys_.end(), key) != physicalKeys_.end())
            return;

        if (physicalKeys_.empty())
        {
            latchedKeys_.clear();
        }
        else if (!IsModifierKey(CanonicalKey(key)))
        {
            // A new non-modifier key starts a new key display. Keep held
            // modifiers, but discard earlier non-modifier keys even if the
            // previous key-up has not been observed yet. Otherwise fast typing
            // can leave stale keys latched and keep the overlay alive.
            latchedKeys_.erase(
                std::remove_if(
                    latchedKeys_.begin(),
                    latchedKeys_.end(),
                    [](UINT raw) { return !IsModifierKey(CanonicalKey(raw)); }),
                latchedKeys_.end());
        }

        physicalKeys_.push_back(key);
        if (std::find(latchedKeys_.begin(), latchedKeys_.end(), key) == latchedKeys_.end())
            latchedKeys_.push_back(key);

        std::vector<std::wstring> labels;
        if (!BuildDisplay(labels))
        {
            // The key was deliberately filtered out (for example an ordinary
            // letter while "show letter keys" is off). Do not leave the
            // previous shortcut on screen just because more keys are down.
            if (!labels_.empty())
            {
                if (stackKeys_)
                {
                    PushHistory();
                    labels_.clear();
                    Render();
                    StartTimer();
                }
                else if (!labelsStale_)
                {
                    // Keep the previous shortcut visible for its normal delay,
                    // then fade it out. Typing filtered keys must not cut the
                    // exit animation short.
                    labelsStale_ = true;
                    if (hideAt_ == 0)
                        hideAt_ = NowMs() + static_cast<ULONGLONG>(delayMs_);
                    StartTimer();
                }
            }
            if (stackKeys_ && !history_.empty())
                StartTimer();
            return;
        }
        const bool changed = labels != labels_;
        labelsStale_ = false;
        if (changed)
        {
            PushHistory();
            labels_ = std::move(labels);
            labelsStartedAt_ = NowMs();
            Render();
        }
        else if (!visible_)
        {
            Render();
        }
        else
        {
            opacity_ = 1.0f;
            Present(1.0f);
        }
        hideAt_ = 0;
        StartTimer();
        return;
    }

    const auto it = std::find(physicalKeys_.begin(), physicalKeys_.end(), key);
    if (it == physicalKeys_.end())
        return;
    physicalKeys_.erase(it);

    // Keep the last chord visible until every key is released. This prevents
    // the final key of Ctrl+S from disappearing immediately when S is lifted.
    if (physicalKeys_.empty())
    {
        if (stackKeys_ && !labels_.empty())
        {
            PushHistory();
            labels_.clear();
            hideAt_ = 0;
            Render();
            StartTimer();
        }
        else if (!labels_.empty())
        {
            hideAt_ = NowMs() + static_cast<ULONGLONG>(delayMs_);
            StartTimer();
        }
        else
        {
            HideNow();
        }
    }
    else if (visible_)
    {
        opacity_ = 1.0f;
        hideAt_ = 0;
        Present(1.0f);
        StartTimer();
    }
}

bool KeyOverlay::BuildDisplay(std::vector<std::wstring>& labels) const
{
    bool hasModifier = false;
    for (UINT raw : latchedKeys_)
    {
        if (IsModifierKey(CanonicalKey(raw)))
        {
            hasModifier = true;
            break;
        }
    }

    std::vector<DisplayKey> display;
    for (UINT raw : latchedKeys_)
    {
        const UINT canonical = CanonicalKey(raw);
        if (canonical == 0)
            continue;
        if (!showLetterKeys_ && !hasModifier && IsCharacterKey(canonical))
            continue;
        const bool exists = std::any_of(
            display.begin(),
            display.end(),
            [canonical](const DisplayKey& key) { return key.vk == canonical; });
        if (exists)
            continue;

        DisplayKey key;
        key.vk = canonical;
        key.modifier = IsModifierKey(canonical);
        key.modifierRank = key.modifier ? ModifierRank(canonical) : 100;
        key.label = LabelForVk(canonical);
        if (key.label.empty())
            continue;
        display.push_back(std::move(key));
    }

    std::stable_sort(
        display.begin(),
        display.end(),
        [](const DisplayKey& a, const DisplayKey& b) {
            if (a.modifier != b.modifier)
                return a.modifier;
            if (a.modifier && b.modifier)
                return a.modifierRank < b.modifierRank;
            return false;
        });

    labels.clear();
    labels.reserve(display.size());
    for (const DisplayKey& key : display)
        labels.push_back(key.label);
    return !labels.empty();
}

void KeyOverlay::PushHistory()
{
    if (!stackKeys_ || labels_.empty())
        return;
    const bool hasNonModifier = std::any_of(
        labels_.begin(),
        labels_.end(),
        [](const std::wstring& label) {
            return label != L"Ctrl" && label != L"Shift" &&
                label != L"Alt" && label != L"Win";
        });
    if (!hasNonModifier)
        return;
    HistoryRow row;
    row.labels = labels_;
    row.hideAt = NowMs() + static_cast<ULONGLONG>(stackDelayMs_);
    row.appearAt = labelsStartedAt_ ? labelsStartedAt_ : NowMs();
    history_.push_back(std::move(row));
    constexpr size_t kMaxRows = 5;
    if (history_.size() > kMaxRows)
        history_.erase(history_.begin(), history_.begin() + (history_.size() - kMaxRows));
}

bool KeyOverlay::PruneHistory(ULONGLONG now)
{
    bool changed = false;
    for (HistoryRow& row : history_)
    {
        if (row.exitAt == 0 && row.hideAt != 0 && now >= row.hideAt)
        {
            row.exitAt = now;
            changed = true;
        }
    }
    const size_t before = history_.size();
    history_.erase(
        std::remove_if(
            history_.begin(),
            history_.end(),
            [now](const HistoryRow& row) {
                return row.exitAt != 0 &&
                    now >= row.exitAt + static_cast<ULONGLONG>(kExitMs);
            }),
        history_.end());
    return changed || history_.size() != before;
}

bool KeyOverlay::EnsurePlacement()
{
    if (placementValid_)
        return true;

    POINT cursor = {};
    GetCursorPos(&cursor);
    HWND foreground = GetForegroundWindow();
    HMONITOR monitor = nullptr;
    if (placementMode_ || position_ == KeyPosition::FollowCursor)
    {
        monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    }
    else if (position_ == KeyPosition::Custom)
    {
        POINT anchor = {customX_, customY_};
        monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    }
    else if (foreground && foreground != hwnd_)
    {
        monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    }
    else
    {
        monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    }

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    RECT work = {};
    if (monitor && GetMonitorInfoW(monitor, &info))
    {
        work = info.rcWork;
    }
    else
    {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    }
    if (work.right <= work.left || work.bottom <= work.top)
        return false;

    const UINT dpi = monitor ? DpiForMonitor(monitor) : SystemDpi();
    placement_.work = work;
    placement_.dpi = dpi;
    placement_.scale = static_cast<float>(dpi) / 96.0f;
    placementValid_ = true;
    return true;
}

void KeyOverlay::DrawRow(
    const std::vector<std::wstring>& row,
    const std::vector<float>& widths,
    float left,
    float top,
    const D2D1_COLOR_F& keyTextColor,
    const D2D1_COLOR_F& accent)
{
    const float separator = kPlusGap * 2.0f + kPlusSize;
    float x = left;
    for (size_t i = 0; i < row.size(); ++i)
    {
        const float width = widths[i];
        const D2D1_RECT_F cap = Rect(x, top, x + width, top + kCapHeight);
        const bool modifier =
            row[i] == L"Ctrl" || row[i] == L"Shift" ||
            row[i] == L"Alt" || row[i] == L"Win";

        if (skin_ == KeySkin::Minimal)
        {
            if (backgroundBitmap_)
            {
                renderTarget_->DrawBitmap(
                    backgroundBitmap_.Get(),
                    cap,
                    1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }
            else
            {
                D2D1_COLOR_F fill = UiColor(
                    keyStyleAuto_ ? keyFillRgb_ : keyStyleRgb_);
                fill.a = 0.94f;
                if (modifier)
                {
                    fill.r *= 0.92f;
                    fill.g *= 0.92f;
                    fill.b *= 0.92f;
                }
                FillRounded(renderTarget_.Get(), brush_.Get(), cap, 9.0f, fill);
            }
            StrokeRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                UiColor(keyStyleAuto_ ? keyBorderRgb_ : DerivedKeyBorder(keyStyleRgb_)),
                1.0f);
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                cap,
                keyTextColor);
        }
        else if (skin_ == KeySkin::Neon)
        {
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                Rect(cap.left - 3.0f, cap.top - 3.0f, cap.right + 3.0f, cap.bottom + 3.0f),
                12.0f,
                D2D1::ColorF(accent.r, accent.g, accent.b, 0.18f));
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                Color(0.05f, 0.06f, 0.08f, 0.92f));
            StrokeRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                D2D1::ColorF(accent.r, accent.g, accent.b, 0.95f),
                1.4f);
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                cap,
                keyTextColor);
        }
        else if (skin_ == KeySkin::Paper)
        {
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                Rect(cap.left + 1.0f, cap.top + 2.0f, cap.right + 1.0f, cap.bottom + 2.0f),
                9.0f,
                Color(0.35f, 0.25f, 0.12f, 0.12f));
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                modifier ? Color(0.91f, 0.85f, 0.72f, 0.98f) : Color(0.98f, 0.95f, 0.87f, 0.98f));
            StrokeRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                Color(0.72f, 0.60f, 0.42f, 0.78f),
                1.0f);
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                cap,
                keyTextColor);
        }
        else if (skin_ == KeySkin::Terminal)
        {
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                Rect(cap.left - 2.0f, cap.top - 2.0f, cap.right + 2.0f, cap.bottom + 2.0f),
                11.0f,
                Color(0.20f, 0.95f, 0.42f, 0.12f));
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                Color(0.035f, 0.055f, 0.040f, 0.96f));
            StrokeRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                Color(0.25f, 0.82f, 0.42f, 0.85f),
                1.2f);
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                cap,
                keyTextColor);
        }
        else if (skin_ == KeySkin::Mechanical)
        {
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                Rect(cap.left + 1.0f, cap.top + 3.0f, cap.right + 1.0f, cap.bottom + 3.0f),
                7.0f,
                Color(0.0f, 0.0f, 0.0f, 0.48f));
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                7.0f,
                Color(0.10f, 0.11f, 0.13f, 0.99f));
            const D2D1_RECT_F face = Rect(
                cap.left + 2.0f,
                cap.top + 2.0f,
                cap.right - 2.0f,
                cap.bottom - 4.0f);
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                face,
                5.0f,
                modifier ? Color(0.20f, 0.22f, 0.25f) : Color(0.16f, 0.17f, 0.20f));
            StrokeRounded(
                renderTarget_.Get(),
                brush_.Get(),
                face,
                5.0f,
                Color(0.56f, 0.59f, 0.64f, 0.42f),
                1.0f);
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                Rect(face.left + 3.0f, face.bottom - 2.0f, face.right - 3.0f, face.bottom),
                1.0f,
                Color(0.02f, 0.03f, 0.04f, 0.55f));
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                face,
                keyTextColor);
        }
        else if (skin_ == KeySkin::Win98)
        {
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                0.0f,
                Color(0.76f, 0.76f, 0.76f));
            StrokeRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                0.0f,
                Color(0.20f, 0.20f, 0.20f),
                1.0f);
            brush_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.95f));
            renderTarget_->DrawLine(
                D2D1::Point2F(cap.left + 1.0f, cap.top + 1.0f),
                D2D1::Point2F(cap.right - 1.0f, cap.top + 1.0f),
                brush_.Get(),
                1.0f);
            renderTarget_->DrawLine(
                D2D1::Point2F(cap.left + 1.0f, cap.top + 1.0f),
                D2D1::Point2F(cap.left + 1.0f, cap.bottom - 1.0f),
                brush_.Get(),
                1.0f);
            brush_->SetColor(Color(0.35f, 0.35f, 0.35f, 0.95f));
            renderTarget_->DrawLine(
                D2D1::Point2F(cap.left + 1.0f, cap.bottom - 1.0f),
                D2D1::Point2F(cap.right - 1.0f, cap.bottom - 1.0f),
                brush_.Get(),
                1.0f);
            renderTarget_->DrawLine(
                D2D1::Point2F(cap.right - 1.0f, cap.top + 1.0f),
                D2D1::Point2F(cap.right - 1.0f, cap.bottom - 1.0f),
                brush_.Get(),
                1.0f);
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                cap,
                keyTextColor);
        }
        else
        {
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                Rect(cap.left + 1.0f, cap.top + 2.0f, cap.right + 1.0f, cap.bottom + 2.0f),
                9.0f,
                Color(0.0f, 0.0f, 0.0f, darkMode_ ? 0.26f : 0.16f));
            FillRounded(
                renderTarget_.Get(),
                brush_.Get(),
                cap,
                9.0f,
                modifier ? palette_.control : palette_.surface);
            StrokeRounded(renderTarget_.Get(), brush_.Get(), cap, 9.0f, palette_.border, 1.0f);
            DrawTextBlock(
                renderTarget_.Get(),
                brush_.Get(),
                row[i],
                keyFormat_.Get(),
                cap,
                keyTextColor);
        }

        if (i + 1 < row.size())
        {
            const float plusX = x + width + kPlusGap + kPlusSize / 2.0f;
            const float plusY = top + kCapHeight / 2.0f;
            if (skin_ == KeySkin::Neon)
            {
                brush_->SetColor(keyTextAuto_ ? accent : keyTextColor);
            }
            else if (skin_ == KeySkin::Paper)
            {
                brush_->SetColor(Color(0.95f, 0.89f, 0.77f, 0.95f));
                renderTarget_->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get());
                brush_->SetColor(Color(0.70f, 0.57f, 0.38f, 0.75f));
                renderTarget_->DrawEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get(),
                    0.9f);
                brush_->SetColor(keyTextColor);
            }
            else if (skin_ == KeySkin::Terminal)
            {
                brush_->SetColor(Color(0.035f, 0.055f, 0.040f, 0.95f));
                renderTarget_->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get());
                brush_->SetColor(Color(0.25f, 0.82f, 0.42f, 0.80f));
                renderTarget_->DrawEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get(),
                    0.9f);
                brush_->SetColor(keyTextColor);
            }
            else if (skin_ == KeySkin::Mechanical)
            {
                brush_->SetColor(Color(0.16f, 0.17f, 0.20f, 0.95f));
                renderTarget_->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get());
                brush_->SetColor(Color(0.56f, 0.59f, 0.64f, 0.42f));
                renderTarget_->DrawEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get(),
                    0.9f);
                brush_->SetColor(keyTextColor);
            }
            else if (skin_ == KeySkin::Win98)
            {
                brush_->SetColor(Color(0.04f, 0.04f, 0.04f));
            }
            else
            {
                brush_->SetColor(D2D1::ColorF(
                    palette_.surface.r,
                    palette_.surface.g,
                    palette_.surface.b,
                    0.82f));
                renderTarget_->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get());
                brush_->SetColor(D2D1::ColorF(
                    palette_.border.r,
                    palette_.border.g,
                    palette_.border.b,
                    0.72f));
                renderTarget_->DrawEllipse(
                    D2D1::Ellipse(D2D1::Point2F(plusX, plusY), 9.0f, 9.0f),
                    brush_.Get(),
                    0.9f);
                brush_->SetColor(D2D1::ColorF(
                    keyTextColor.r,
                    keyTextColor.g,
                    keyTextColor.b,
                    0.82f));
            }
            renderTarget_->DrawLine(
                D2D1::Point2F(plusX - kPlusSize / 2.0f, plusY),
                D2D1::Point2F(plusX + kPlusSize / 2.0f, plusY),
                brush_.Get(),
                1.6f);
            renderTarget_->DrawLine(
                D2D1::Point2F(plusX, plusY - kPlusSize / 2.0f),
                D2D1::Point2F(plusX, plusY + kPlusSize / 2.0f),
                brush_.Get(),
                1.6f);
        }

        x += width + separator;
    }
}

void KeyOverlay::Render()
{
    if (!hwnd_ || !keyFormat_ || !dwriteFactory_)
        return;

    const ULONGLONG now = NowMs();
    std::vector<RowView> rows;
    if (placementMode_)
    {
        if (!labels_.empty())
            rows.push_back({&labels_, now, 0});
    }
    else
    {
        for (const HistoryRow& row : history_)
            rows.push_back({&row.labels, row.appearAt, row.exitAt});
        if (!labels_.empty())
            rows.push_back({&labels_, labelsStartedAt_, 0});
    }
    if (rows.empty() || !EnsurePlacement())
        return;

    struct MeasuredRow
    {
        RowView row;
        std::vector<float> widths;
        float width = 0.0f;
        float alpha = 1.0f;
        float offsetY = 0.0f;
    };

    const float scale = placement_.scale;
    const float separator = kPlusGap * 2.0f + kPlusSize;
    std::vector<MeasuredRow> measured;
    measured.reserve(rows.size());
    float maxWidth = 0.0f;
    for (const RowView& view : rows)
    {
        MeasuredRow item;
        item.row = view;
        const ULONGLONG appearAt = view.appearAt ? view.appearAt : now;
        float enter = static_cast<float>(
            (std::min)(1.0, static_cast<double>(now - appearAt) / static_cast<double>(kEnterMs)));
        enter = enter * enter * (3.0f - 2.0f * enter);
        float exit = 0.0f;
        if (view.exitAt != 0)
        {
            exit = static_cast<float>(
                (std::min)(1.0, static_cast<double>(now - view.exitAt) / static_cast<double>(kExitMs)));
            exit = exit * exit * (3.0f - 2.0f * exit);
        }
        item.alpha = (std::max)(0.0f, enter * (1.0f - exit));
        item.offsetY = (1.0f - enter) * 8.0f + exit * 8.0f;
        float rowWidth = 0.0f;
        for (const std::wstring& label : *view.labels)
        {
            Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
            if (FAILED(dwriteFactory_->CreateTextLayout(
                    label.c_str(),
                    static_cast<UINT32>(label.size()),
                    keyFormat_.Get(),
                    1000.0f,
                    kCapHeight,
                    layout.GetAddressOf())) ||
                !layout)
            {
                return;
            }
            DWRITE_TEXT_METRICS metrics = {};
            if (FAILED(layout->GetMetrics(&metrics)))
                return;
            const float width = (std::max)(kCapMinWidth, std::ceil(metrics.width + kCapPaddingX * 2.0f));
            item.widths.push_back(width);
            rowWidth += width;
        }
        if (!item.widths.empty())
            rowWidth += separator * static_cast<float>(item.widths.size() - 1);
        item.width = rowWidth;
        maxWidth = (std::max)(maxWidth, rowWidth);
        measured.push_back(std::move(item));
    }

    const bool placement = placementMode_;
    const float hintHeight = placement ? 18.0f : 0.0f;
    constexpr float kRowGap = 8.0f;
    const float windowWidthDip = maxWidth + kMargin * 2.0f;
    const float windowHeightDip =
        kCapHeight * static_cast<float>(measured.size()) +
        kRowGap * static_cast<float>(measured.size() - 1) +
        kMargin * 2.0f + hintHeight;
    const UINT pixelWidth = (std::max)(1L, std::lround(windowWidthDip * scale));
    const UINT pixelHeight = (std::max)(1L, std::lround(windowHeightDip * scale));
    if (!EnsureSurface(pixelWidth, pixelHeight, placement_.dpi))
        return;

    renderTarget_->SetDpi(static_cast<float>(placement_.dpi), static_cast<float>(placement_.dpi));
    renderTarget_->BeginDraw();
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(Color(0.0f, 0.0f, 0.0f, 0.0f));

    const D2D1_COLOR_F accent = UiColor(accentRgb_);
    D2D1_COLOR_F keyTextColor = palette_.text;
    if (!keyTextAuto_)
    {
        keyTextColor = UiColor(keyTextRgb_);
    }
    else if (skin_ == KeySkin::Neon)
    {
        keyTextColor = Color(0.96f, 0.98f, 1.0f);
    }
    else if (skin_ == KeySkin::Paper)
    {
        keyTextColor = Color(0.24f, 0.17f, 0.10f);
    }
    else if (skin_ == KeySkin::Terminal)
    {
        keyTextColor = Color(0.42f, 1.0f, 0.55f);
    }
    else if (skin_ == KeySkin::Mechanical)
    {
        keyTextColor = Color(0.94f, 0.94f, 0.94f);
    }
    else if (skin_ == KeySkin::Win98)
    {
        keyTextColor = Color(0.04f, 0.04f, 0.04f);
    }

    if (placement && placementHintFormat_)
    {
        DrawTextBlock(
            renderTarget_.Get(),
            brush_.Get(),
            L"单击确认 / Esc 取消",
            placementHintFormat_.Get(),
            Rect(0.0f, 2.0f, windowWidthDip, 18.0f),
            palette_.muted);
    }

    if (!rowLayer_)
        renderTarget_->CreateLayer(rowLayer_.GetAddressOf());

    float top = kMargin + hintHeight;
    for (const MeasuredRow& row : measured)
    {
        if (row.alpha <= 0.01f)
        {
            top += kCapHeight + kRowGap;
            continue;
        }
        const float left = kMargin + (maxWidth - row.width) / 2.0f;
        const float drawTop = top + row.offsetY;
        if (rowLayer_ && row.alpha < 0.995f)
        {
            D2D1_LAYER_PARAMETERS parameters = D2D1::LayerParameters();
            parameters.opacity = row.alpha;
            renderTarget_->PushLayer(parameters, rowLayer_.Get());
            DrawRow(*row.row.labels, row.widths, left, drawTop, keyTextColor, accent);
            renderTarget_->PopLayer();
        }
        else
        {
            DrawRow(*row.row.labels, row.widths, left, drawTop, keyTextColor, accent);
        }
        top += kCapHeight + kRowGap;
    }

    const HRESULT endHr = renderTarget_->EndDraw();
    if (FAILED(endHr))
    {
        DiscardSurface();
        return;
    }

    const int widthPx = static_cast<int>(pixelWidth);
    const int heightPx = static_cast<int>(pixelHeight);
    const int workWidth = placement_.work.right - placement_.work.left;
    const int margin = static_cast<int>(std::lround(kBottomMargin * scale));
    destX_ = placement_.work.left + (workWidth - widthPx) / 2;
    destY_ = placement_.work.bottom - heightPx - margin;
    if (placementMode_)
    {
        POINT cursor = {};
        GetCursorPos(&cursor);
        destX_ = cursor.x - widthPx / 2;
        destY_ = cursor.y - heightPx / 2;
    }
    else if (position_ == KeyPosition::TopCenter)
    {
        destY_ = placement_.work.top + margin;
    }
    else if (position_ == KeyPosition::FollowCursor)
    {
        POINT cursor = {};
        GetCursorPos(&cursor);
        const int offset = static_cast<int>(std::lround(24.0f * scale));
        destX_ = cursor.x - widthPx / 2;
        destY_ = cursor.y + offset;
        if (destY_ + heightPx > placement_.work.bottom - margin)
            destY_ = cursor.y - heightPx - offset;
    }
    else if (position_ == KeyPosition::Custom)
    {
        destX_ = customX_ - widthPx / 2;
        destY_ = customY_ - heightPx / 2;
    }

    const int edge = static_cast<int>(std::lround(8.0f * scale));
    const int maxLeft = placement_.work.right - widthPx - edge;
    if (destX_ > maxLeft)
        destX_ = maxLeft;
    if (destX_ < placement_.work.left + edge)
        destX_ = placement_.work.left + edge;
    const int maxTop = placement_.work.bottom - heightPx - edge;
    if (destY_ > maxTop)
        destY_ = maxTop;
    if (destY_ < placement_.work.top + edge)
        destY_ = placement_.work.top + edge;

    Present(1.0f);
}

bool KeyOverlay::Present(float opacity)
{
    if (!hwnd_ || !bitmap_ || !gdiInterop_ || pixelWidth_ == 0 || pixelHeight_ == 0)
        return false;

    opacity = (std::max)(0.0f, (std::min)(1.0f, opacity));
    renderTarget_->BeginDraw();
    HDC sourceDc = nullptr;
    const HRESULT dcHr = gdiInterop_->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &sourceDc);
    if (FAILED(dcHr) || !sourceDc)
    {
        renderTarget_->EndDraw();
        return false;
    }

    const int exitOffset = opacity < 0.999f
        ? static_cast<int>(std::lround((1.0f - opacity) * 8.0f * placement_.scale))
        : 0;
    POINT destination = {destX_, destY_ + exitOffset};
    POINT source = {0, 0};
    SIZE size = {static_cast<LONG>(pixelWidth_), static_cast<LONG>(pixelHeight_)};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(std::lround(opacity * 255.0f));
    blend.AlphaFormat = AC_SRC_ALPHA;

    const BOOL ok = UpdateLayeredWindow(
        hwnd_,
        nullptr,
        &destination,
        &size,
        sourceDc,
        &source,
        0,
        &blend,
        ULW_ALPHA);
    gdiInterop_->ReleaseDC(nullptr);
    const HRESULT endHr = renderTarget_->EndDraw();

    if (!ok || FAILED(endHr))
        return false;

    if (!visible_)
    {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        visible_ = true;
    }
    opacity_ = opacity;
    return true;
}

void KeyOverlay::OnTimer()
{
    if (placementMode_)
    {
        OnPlacementTimer();
        return;
    }
    if (!visible_)
    {
        StopTimer();
        return;
    }

    PruneReleasedKeys();
    const ULONGLONG now = NowMs();
    const bool historyChanged = PruneHistory(now);
    const auto animating = [&]() {
        for (const HistoryRow& row : history_)
        {
            if (row.exitAt != 0 ||
                now - row.appearAt < static_cast<ULONGLONG>(kEnterMs))
            {
                return true;
            }
        }
        return !labels_.empty() &&
            now - labelsStartedAt_ < static_cast<ULONGLONG>(kEnterMs);
    };
    if (stackKeys_)
    {
        if (!physicalKeys_.empty())
        {
            hideAt_ = 0;
            if (historyChanged || animating())
                Render();
            return;
        }
        bool needsRender = historyChanged || animating();
        if (!labels_.empty())
        {
            PushHistory();
            labels_.clear();
            needsRender = true;
        }
        if (history_.empty() && labels_.empty())
        {
            HideNow();
            return;
        }
        if (needsRender)
            Render();
        StartTimer();
        return;
    }
    if (physicalKeys_.empty())
    {
        if (hideAt_ == 0)
            hideAt_ = now + static_cast<ULONGLONG>(delayMs_);
        if (!labels_.empty() &&
            now - labelsStartedAt_ < static_cast<ULONGLONG>(kEnterMs))
        {
            Render();
            return;
        }
        if (now < hideAt_)
            return;

        const float elapsed = static_cast<float>(now - hideAt_);
        const float opacity = 1.0f - elapsed / kFadeMs;
        if (opacity <= 0.01f)
        {
            HideNow();
            return;
        }
        if (std::fabs(opacity - opacity_) > 0.01f)
            Present(opacity);
        return;
    }

    if (labelsStale_)
    {
        if (hideAt_ == 0)
            hideAt_ = now + static_cast<ULONGLONG>(delayMs_);
        if (now < hideAt_)
            return;
        const float elapsed = static_cast<float>(now - hideAt_);
        const float opacity = 1.0f - elapsed / kFadeMs;
        if (opacity <= 0.01f)
        {
            HideNow();
            return;
        }
        if (std::fabs(opacity - opacity_) > 0.01f)
            Present(opacity);
        return;
    }

    hideAt_ = 0;
    if (!labels_.empty() && now - labelsStartedAt_ < static_cast<ULONGLONG>(kEnterMs))
        Render();
    if (opacity_ < 0.99f)
    {
        opacity_ = 1.0f;
        Present(1.0f);
    }
}

void KeyOverlay::PruneReleasedKeys()
{
    std::vector<UINT> kept;
    kept.reserve(physicalKeys_.size());
    for (UINT vk : physicalKeys_)
    {
        if ((GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0)
            kept.push_back(vk);
    }
    if (kept.size() == physicalKeys_.size())
        return;

    physicalKeys_ = std::move(kept);
    if (!stackKeys_ && physicalKeys_.empty() && !labels_.empty() && hideAt_ == 0)
        hideAt_ = NowMs() + static_cast<ULONGLONG>(delayMs_);
}

void KeyOverlay::HideForSuspend()
{
    StopTimer();
    if (hwnd_ && visible_)
        ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
    placementValid_ = false;
    physicalKeys_.clear();
    latchedKeys_.clear();
    labelsStale_ = false;
    labelsStartedAt_ = NowMs();
    for (HistoryRow& row : history_)
    {
        row.appearAt = labelsStartedAt_;
        row.exitAt = 0;
        row.hideAt = labelsStartedAt_ + static_cast<ULONGLONG>(stackDelayMs_);
    }
    opacity_ = 1.0f;
    hideAt_ = 0;
}

void KeyOverlay::HideNow()
{
    StopTimer();
    if (hwnd_ && visible_)
        ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
    placementValid_ = false;
    physicalKeys_.clear();
    latchedKeys_.clear();
    labels_.clear();
    labelsStartedAt_ = 0;
    labelsStale_ = false;
    history_.clear();
    opacity_ = 1.0f;
    hideAt_ = 0;
}

void KeyOverlay::StartTimer()
{
    if (!timerRunning_ && hwnd_)
    {
        SetTimer(hwnd_, kTimerId, kTimerIntervalMs, nullptr);
        timerRunning_ = true;
    }
}

void KeyOverlay::StopTimer()
{
    if (timerRunning_ && hwnd_)
        KillTimer(hwnd_, kTimerId);
    timerRunning_ = false;
}

bool KeyOverlay::EnsureSurface(UINT pixelWidth, UINT pixelHeight, UINT dpi)
{
    if (!wicFactory_ || !d2dFactory_)
        return false;
    if (pixelWidth == pixelWidth_ && pixelHeight == pixelHeight_ && dpi == dpi_ && renderTarget_)
        return true;
    if (pixelWidth == 0 || pixelHeight == 0)
        return false;

    DiscardSurface();
    HRESULT hr = wicFactory_->CreateBitmap(
        pixelWidth,
        pixelHeight,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        bitmap_.GetAddressOf());
    if (FAILED(hr))
        return false;

    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        static_cast<float>(dpi),
        static_cast<float>(dpi),
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
    hr = d2dFactory_->CreateWicBitmapRenderTarget(
        bitmap_.Get(),
        properties,
        renderTarget_.GetAddressOf());
    if (FAILED(hr))
    {
        DiscardSurface();
        return false;
    }

    hr = renderTarget_.As(&gdiInterop_);
    if (FAILED(hr))
    {
        DiscardSurface();
        return false;
    }
    hr = renderTarget_->CreateSolidColorBrush(palette_.surface, brush_.GetAddressOf());
    if (FAILED(hr))
    {
        DiscardSurface();
        return false;
    }

    renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
    dpi_ = dpi;
    EnsureBackgroundBitmap();
    return true;
}

void KeyOverlay::LoadBackgroundImage()
{
    backgroundSource_.Reset();
    backgroundBitmap_.Reset();
    backgroundPath_ = keyBackgroundImage_;
    if (backgroundPath_.empty() || !wicFactory_)
        return;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wicFactory_->CreateDecoderFromFilename(
            backgroundPath_.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf())) ||
        !decoder)
    {
        return;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())) || !frame)
        return;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wicFactory_->CreateFormatConverter(converter.GetAddressOf())) ||
        !converter ||
        FAILED(converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom)))
    {
        return;
    }

    backgroundSource_ = converter;
    EnsureBackgroundBitmap();
}

void KeyOverlay::EnsureBackgroundBitmap()
{
    if (backgroundBitmap_ || !backgroundSource_ || !renderTarget_)
        return;
    renderTarget_->CreateBitmapFromWicBitmap(
        backgroundSource_.Get(),
        nullptr,
        backgroundBitmap_.GetAddressOf());
}

void KeyOverlay::DiscardSurface()
{
    rowLayer_.Reset();
    backgroundBitmap_.Reset();
    rowLayer_.Reset();
    brush_.Reset();
    gdiInterop_.Reset();
    renderTarget_.Reset();
    bitmap_.Reset();
    pixelWidth_ = 0;
    pixelHeight_ = 0;
    dpi_ = 96;
}

UINT KeyOverlay::CanonicalKey(UINT vk)
{
    switch (vk)
    {
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_SHIFT:
        return VK_SHIFT;
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_CONTROL:
        return VK_CONTROL;
    case VK_LMENU:
    case VK_RMENU:
    case VK_MENU:
        return VK_MENU;
    case VK_LWIN:
    case VK_RWIN:
        return VK_LWIN;
    case VK_PROCESSKEY:
    case VK_PACKET:
        return 0;
    default:
        return vk;
    }
}

bool KeyOverlay::IsModifierKey(UINT canonicalVk)
{
    return canonicalVk == VK_CONTROL || canonicalVk == VK_SHIFT ||
        canonicalVk == VK_MENU || canonicalVk == VK_LWIN;
}

bool KeyOverlay::IsCharacterKey(UINT vk)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return true;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return true;
    switch (vk)
    {
    case VK_SPACE:
    // Backspace and Return are editing/control keys, not ordinary
    // character keys. They must remain visible when letters are hidden,
    // just like Delete, Tab and the arrow keys.
    case VK_OEM_1:
    case VK_OEM_PLUS:
    case VK_OEM_COMMA:
    case VK_OEM_MINUS:
    case VK_OEM_PERIOD:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_8:
    case VK_OEM_102:
    case VK_ADD:
    case VK_SUBTRACT:
    case VK_MULTIPLY:
    case VK_DIVIDE:
    case VK_DECIMAL:
        return true;
    default:
        return false;
    }
}

int KeyOverlay::ModifierRank(UINT canonicalVk)
{
    switch (canonicalVk)
    {
    case VK_CONTROL: return 0;
    case VK_SHIFT: return 1;
    case VK_MENU: return 2;
    case VK_LWIN: return 3;
    default: return 4;
    }
}

std::wstring KeyOverlay::LabelForVk(UINT vk)
{
    if (vk >= 'A' && vk <= 'Z')
        return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= '0' && vk <= '9')
        return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24)
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return L"Num" + std::to_wstring(vk - VK_NUMPAD0);

    switch (vk)
    {
    case VK_CONTROL: return L"Ctrl";
    case VK_SHIFT: return L"Shift";
    case VK_MENU: return L"Alt";
    case VK_LWIN: return L"Win";
    case VK_LEFT: return L"\u2190";
    case VK_RIGHT: return L"\u2192";
    case VK_UP: return L"\u2191";
    case VK_DOWN: return L"\u2193";
    case VK_SPACE: return L"Space";
    case VK_RETURN: return L"Enter";
    case VK_ESCAPE: return L"Esc";
    case VK_TAB: return L"Tab";
    case VK_BACK: return L"Backspace";
    case VK_DELETE: return L"Del";
    case VK_INSERT: return L"Ins";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"PgUp";
    case VK_NEXT: return L"PgDn";
    case VK_CAPITAL: return L"Caps";
    case VK_NUMLOCK: return L"Num";
    case VK_SCROLL: return L"Scroll";
    case VK_SNAPSHOT: return L"PrtSc";
    case VK_PAUSE: return L"Pause";
    case VK_APPS: return L"Menu";
    case VK_ADD: return L"+";
    case VK_SUBTRACT: return L"-";
    case VK_MULTIPLY: return L"*";
    case VK_DIVIDE: return L"/";
    case VK_DECIMAL: return L".";
    case VK_OEM_1: return L";";
    case VK_OEM_PLUS: return L"=";
    case VK_OEM_COMMA: return L",";
    case VK_OEM_MINUS: return L"-";
    case VK_OEM_PERIOD: return L".";
    case VK_OEM_2: return L"/";
    case VK_OEM_3: return L"`";
    case VK_OEM_4: return L"[";
    case VK_OEM_5: return L"\\";
    case VK_OEM_6: return L"]";
    case VK_OEM_7: return L"'";
    default:
        break;
    }

    const UINT scancode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scancode != 0)
    {
        wchar_t name[64] = {};
        if (GetKeyNameTextW(static_cast<LONG>(scancode << 16), name, _countof(name)) > 0)
            return name;
    }
    return L"Key";
}

UINT KeyOverlay::SystemDpi()
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

UINT KeyOverlay::DpiForMonitor(HMONITOR monitor)
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

LRESULT CALLBACK KeyOverlay::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    KeyOverlay* self = reinterpret_cast<KeyOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<KeyOverlay*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message)
    {
    case kKeyMessage:
        self->OnKey(wParam, lParam != 0);
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId)
        {
            self->OnTimer();
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_DISPLAYCHANGE:
        if (self->visible_)
            self->HideNow();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK KeyOverlay::CatcherProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    KeyOverlay* self = reinterpret_cast<KeyOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<KeyOverlay*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_LBUTTONUP:
        if (self->placementMode_ && self->placementArmed_)
            self->ConfirmPlacement();
        return 0;
    case WM_RBUTTONUP:
        if (self->placementMode_)
            self->CancelPlacement();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
