#pragma once

#include "key_display.h"
#include "keyboard_hook.h"
#include "ui_theme.h"

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>
#include <vector>

class KeyOverlay
{
public:
    KeyOverlay() = default;
    ~KeyOverlay();

    KeyOverlay(const KeyOverlay&) = delete;
    KeyOverlay& operator=(const KeyOverlay&) = delete;

    bool Create(HINSTANCE instance);
    void Destroy();
    bool SetEnabled(bool enabled);
    void SetSuspended(bool suspended);
    void SetDarkMode(bool dark);
    void Configure(
        KeySkin skin,
        KeyPosition position,
        int delayMs,
        bool showLetterKeys,
        bool keyStack,
        int keyStackDelayMs,
        int keyScalePercent,
        int keyOpacityPercent,
        bool keyTextAuto,
        unsigned keyTextRgb,
        bool keyStyleAuto,
        unsigned keyStyleRgb,
        unsigned keyFillRgb,
        unsigned keyBorderRgb,
        const std::wstring& keyBackgroundImage,
        unsigned accentRgb,
        int customX,
        int customY);
    bool BeginPlacement(HWND target, UINT message);
    void CancelPlacement();
    bool IsPlacementActive() const { return placementMode_; }
    bool IsEnabled() const { return enabled_; }
    bool IsVisible() const { return visible_; }

private:
    struct DisplayKey
    {
        UINT vk = 0;
        bool modifier = false;
        int modifierRank = 0;
        std::wstring label;
    };

    struct Placement
    {
        RECT work = {};
        UINT dpi = 96;
        float scale = 1.0f;
    };

    struct HistoryRow
    {
        std::vector<std::wstring> labels;
        ULONGLONG hideAt = 0;
        ULONGLONG appearAt = 0;
        ULONGLONG exitAt = 0;
    };

    struct RowView
    {
        const std::vector<std::wstring>* labels = nullptr;
        ULONGLONG appearAt = 0;
        ULONGLONG exitAt = 0;
    };

    static constexpr UINT kKeyMessage = WM_APP + 51;
    static constexpr UINT_PTR kTimerId = 1;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CatcherProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnKey(WPARAM vk, bool down);
    void OnTimer();
    void OnPlacementTimer();
    void UpdatePlacementPosition();
    void ConfirmPlacement();
    void EndPlacement(bool confirm);
    bool CreateCatcher();
    void DestroyCatcher();
    bool BuildDisplay(std::vector<std::wstring>& labels) const;
    void PushHistory();
    bool PruneHistory(ULONGLONG now);
    void DrawRow(
        const std::vector<std::wstring>& row,
        const std::vector<float>& widths,
        float left,
        float top,
        const D2D1_COLOR_F& keyTextColor,
        const D2D1_COLOR_F& accent);
    void Render();
    bool Present(float opacity);
    void HideNow();
    void HideForSuspend();
    void StartTimer();
    void StopTimer();
    void PruneReleasedKeys();
    bool EnsurePlacement();
    bool EnsureSurface(UINT pixelWidth, UINT pixelHeight, UINT dpi);
    void DiscardSurface();
    void LoadBackgroundImage();
    void EnsureBackgroundBitmap();
    bool CreateTextFormat();

    static UINT CanonicalKey(UINT vk);
    static bool IsModifierKey(UINT canonicalVk);
    static bool IsCharacterKey(UINT canonicalVk);
    static int ModifierRank(UINT canonicalVk);
    static std::wstring LabelForVk(UINT canonicalVk);
    static UINT SystemDpi();
    static UINT DpiForMonitor(HMONITOR monitor);

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    bool classRegistered_ = false;
    bool catcherClassRegistered_ = false;
    bool enabled_ = false;
    bool suspended_ = false;
    bool darkMode_ = false;
    bool placementMode_ = false;
    bool placementArmed_ = false;
    ULONGLONG placementStarted_ = 0;
    bool visible_ = false;
    bool labelsStale_ = false;
    bool placementValid_ = false;
    bool timerRunning_ = false;
    bool showLetterKeys_ = false;
    bool stackKeys_ = false;
    int stackDelayMs_ = 650;
    float keyScale_ = 1.0f;
    float keyOpacity_ = 1.0f;
    bool keyTextAuto_ = true;
    unsigned keyTextRgb_ = 0xFFFFFF;
    bool keyStyleAuto_ = true;
    unsigned keyStyleRgb_ = 0xFFFFFF;
    unsigned keyFillRgb_ = 0xFFFFFF;
    unsigned keyBorderRgb_ = 0xC9CDD6;
    std::wstring keyBackgroundImage_;
    KeySkin skin_ = KeySkin::Classic;
    KeyPosition position_ = KeyPosition::BottomCenter;
    int delayMs_ = kDefaultKeyDelayMs;
    unsigned accentRgb_ = 0x5B8DFF;
    int customX_ = 0;
    int customY_ = 0;
    HWND placementTarget_ = nullptr;
    UINT placementMessage_ = 0;
    HWND catcher_ = nullptr;
    float opacity_ = 1.0f;
    ULONGLONG hideAt_ = 0;
    Placement placement_ = {};
    int destX_ = 0;
    int destY_ = 0;
    UINT pixelWidth_ = 0;
    UINT pixelHeight_ = 0;
    UINT dpi_ = 96;
    std::vector<UINT> physicalKeys_;
    std::vector<UINT> latchedKeys_;
    std::vector<std::wstring> labels_;
    ULONGLONG labelsStartedAt_ = 0;
    std::vector<HistoryRow> history_;
    UiPalette palette_ = LightPalette();
    KeyboardHook hook_;

    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory_;
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1GdiInteropRenderTarget> gdiInterop_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<ID2D1Layer> rowLayer_;
    Microsoft::WRL::ComPtr<IWICBitmapSource> backgroundSource_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> backgroundBitmap_;
    std::wstring backgroundPath_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> keyFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> placementHintFormat_;
};
