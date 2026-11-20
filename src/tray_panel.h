#pragma once

#include "ui_theme.h"

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>

class TrayPanelHost
{
public:
    virtual ~TrayPanelHost() = default;
    virtual void OnTrayPanelToggle() = 0;
    virtual void OnTrayPanelShowKeysChanged(bool enabled) = 0;
    virtual void OnTrayPanelTimerChanged(bool enabled) = 0;
    virtual void OnTrayPanelTrailChanged(bool enabled) = 0;
    virtual void OnTrayPanelClickRippleChanged(bool enabled) = 0;
    virtual void OnTrayPanelClickSoundChanged(bool enabled) = 0;
    virtual void OnTrayPanelSettings() = 0;
    virtual void OnTrayPanelExit() = 0;
    virtual void OnTrayPanelClosed() = 0;
};

class TrayPanel
{
public:
    TrayPanel() = default;
    ~TrayPanel();

    TrayPanel(const TrayPanel&) = delete;
    TrayPanel& operator=(const TrayPanel&) = delete;

    bool Create(HINSTANCE instance, TrayPanelHost* host);
    void Destroy();
    bool ShowAt(
        const RECT& iconRect,
        bool enabled,
        unsigned accentRgb,
        const std::wstring& hotkeyText,
        bool showKeys,
        bool timerEnabled,
        bool trailEnabled,
        bool clickRipple,
        bool clickSound);
    void Hide();
    void SetState(bool enabled, unsigned accentRgb);
    void SetToggles(bool showKeys, bool timerEnabled, bool trailEnabled, bool clickRipple, bool clickSound);
    void SetHotkey(const std::wstring& hotkeyText);
    void SetDarkMode(bool dark);
    void SetFirstRunHint(bool enabled);
    bool IsVisible() const { return visible_; }

private:
    enum class Row
    {
        None,
        Toggle,
        ShowKeys,
        Timer,
        Trail,
        ClickRipple,
        ClickSound,
        Settings,
        Exit,
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();
    bool CreateTextFormats();
    bool EnsureRenderTarget();
    void DiscardRenderTarget();
    void Draw();
    void DrawTextBlock(const wchar_t* text, IDWriteTextFormat* format, const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);
    void FillRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color);
    void StrokeRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, float width = 1.0f);
    void DrawSwitch(const D2D1_RECT_F& rect, bool on);
    void DrawSettingsIcon(D2D1_POINT_2F center, const D2D1_COLOR_F& color);
    void DrawPowerIcon(D2D1_POINT_2F center, const D2D1_COLOR_F& color);
    void DrawChevron(D2D1_POINT_2F center, const D2D1_COLOR_F& color);
    Row RowAt(float x, float y) const;
    void ActivateRow(Row row);
    void OnMouseMove(float x, float y);
    void OnMouseLeave();
    void MoveFocus(bool backwards);

    HINSTANCE instance_ = nullptr;
    TrayPanelHost* host_ = nullptr;
    HWND hwnd_ = nullptr;
    bool classRegistered_ = false;
    bool visible_ = false;
    bool hasFocus_ = false;
    bool enabled_ = true;
    bool showKeys_ = true;
    bool timerEnabled_ = true;
    bool trailEnabled_ = false;
    bool clickRipple_ = false;
    bool clickSound_ = false;
    bool darkMode_ = false;
    bool firstRunHint_ = false;
    unsigned accentRgb_ = 0x5B8DFF;
    std::wstring hotkeyText_ = L"Ctrl+Alt+H";
    UiPalette palette_ = LightPalette();
    float scale_ = 1.0f;
    Row hoverRow_ = Row::None;
    Row focusRow_ = Row::Toggle;

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> brandFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rowFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rowStrongFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> captionFormat_;

    UINT targetWidth_ = 0;
    UINT targetHeight_ = 0;
};
