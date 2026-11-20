#pragma once

#include "hotkey.h"
#include "key_display.h"
#include "settings.h"
#include "ui_theme.h"

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <vector>

class SettingsWindowHost
{
public:
    virtual ~SettingsWindowHost() = default;
    virtual void OnSettingsEnabledChanged() = 0;
    virtual void OnSettingsStyleChanged() = 0;
    virtual void OnSettingsAutoStartChanged(bool enabled) = 0;
    virtual void OnSettingsClosed() = 0;
    virtual bool BeginHotkeyRecording(std::wstring& error) = 0;
    virtual bool CommitHotkey(const HotkeyBinding& binding, std::wstring& error) = 0;
    virtual void CancelHotkeyRecording() = 0;
    virtual void OnSettingsThemeChanged() = 0;
    virtual void OnSettingsShowKeysChanged(bool enabled) = 0;
    virtual void OnSettingsKeyDisplayChanged() = 0;
    virtual void OnSettingsKeyPositionCustomRequested() = 0;
    virtual void OnSettingsTimerChanged() = 0;
    virtual void OnSettingsTimerResetRequested() = 0;
    virtual void OnSettingsClickChanged() = 0;
    virtual void OnSettingsPreviewClickSound(bool right) = 0;
};

class SettingsWindow
{
public:
    SettingsWindow() = default;
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    bool Create(HINSTANCE instance, AppSettings* settings, SettingsWindowHost* host);
    void Destroy();
    void Show();
    void Hide();
    void Refresh();
    void SetDarkMode(bool dark);
    bool IsVisible() const { return visible_; }

private:
    enum class Page
    {
        Highlight = 0,
        Appearance = 1,
        Keys = 2,
        Stack = 3,
        Timer = 4,
        Click = 5,
        General = 6,
    };

    enum class SliderDrag
    {
        None,
        Radius,
        Opacity,
        KeyDelay,
        KeyScale,
        KeyOpacity,
        StackDelay,
        ClickVolume,
    };

    enum class ColorDrag
    {
        None,
        SaturationValue,
        Hue,
    };

    enum class FocusId
    {
        None,
        NavHighlight,
        NavAppearance,
        NavKeys,
        NavTimer,
        NavClick,
        NavStack,
        NavGeneral,
        Enable,
        HaloStyleRing,
        HaloStyleDouble,
        HaloStyleCrosshair,
        HaloStyleDot,
        Radius,
        Opacity,
        Color0,
        Color1,
        Color2,
        Color3,
        Color4,
        CustomColor,
        AutoStart,
        Hotkey,
        ThemeSystem,
        ThemeLight,
        ThemeDark,
        ShowKeys,
        StackEnabled,
        StackDelay,
        SkinFill,
        SkinBorder,
        SkinText,
        SkinImage,
        SkinImageClear,
        KeySkinClassic,
        KeySkinMinimal,
        KeySkinNeon,
        KeySkinPaper,
        KeySkinTerminal,
        KeySkinMechanical,
        KeySkinWin98,
        KeyPositionBottom,
        KeyPositionTop,
        KeyPositionCursor,
        KeyPositionCustom,
        KeyDelay,
        KeyScale,
        KeyOpacity,
        KeyLetters,
        KeyTextAuto,
        KeyTextColor,
        KeyStyleColor,
        ClickColorLeft,
        ClickColorRight,
        ClickRipple,
        ClickSound,
        ClickVolume,
        ClickPresetDefault,
        ClickPresetSoft,
        ClickPresetCrisp,
        ClickPresetWood,
        ClickPresetSynth,
        ClickImport,
        TimerEnabled,
        TimerAutoStart,
        TimerShowHours,
        TimerResetOnStop,
        TimerReset,
        Close,
    };

    enum class ColorFocus
    {
        SaturationValue,
        Hue,
        Hex,
        Cancel,
        Done,
    };

    enum class ColorTarget
    {
        Accent,
        KeyText,
        KeyStyle,
        KeyFill,
        KeyBorder,
        ClickLeft,
        ClickRight,
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK HotkeyHookProc(int code, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();
    bool StartKeyboardCaptureHook();
    void StopKeyboardCaptureHook();
    bool CreateTextFormats();
    bool EnsureRenderTarget();
    void DiscardRenderTarget();
    void Draw();
    void DrawSidebar(const D2D1_COLOR_F& accent);
    void DrawHeader(const D2D1_COLOR_F& accent);
    void DrawPreview(const D2D1_COLOR_F& accent);
    void DrawHighlightPage(const D2D1_COLOR_F& accent);
    void DrawAppearancePage(const D2D1_COLOR_F& accent);
    void DrawKeysPage(const D2D1_COLOR_F& accent);
    void DrawTimerPage(const D2D1_COLOR_F& accent);
    void DrawClickPage(const D2D1_COLOR_F& accent);
    void DrawPreviewSlider(
        const wchar_t* label,
        const std::wstring& value,
        float centerY,
        float fraction,
        const D2D1_RECT_F& track,
        const D2D1_COLOR_F& accent);
    void DrawStackPage(const D2D1_COLOR_F& accent);
    void DrawSkinPage(const D2D1_COLOR_F& accent);
    void DrawGeneralPage(const D2D1_COLOR_F& accent);
    void DrawKeyPreviewCaps(const D2D1_COLOR_F& accent);
    void DrawKeycapPreview(
        const D2D1_RECT_F& rect,
        const wchar_t* label,
        bool modifier,
        KeySkin skin,
        const D2D1_COLOR_F& accent);
    void DrawCloseButton();
    void DrawColorPicker();
    void DrawHexInput(const D2D1_COLOR_F& accent);
    void DrawLineIcon(int icon, D2D1_POINT_2F center, const D2D1_COLOR_F& color);
    void DrawSlider(const wchar_t* label, const std::wstring& value, float labelY, float trackY, float fraction, const D2D1_COLOR_F& accent);
    void DrawToggle(const D2D1_RECT_F& rect, bool on, const D2D1_COLOR_F& accent);
    void DrawTextBlock(const wchar_t* text, IDWriteTextFormat* format, const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);

    void FillRect(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);
    void FillRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color);
    void StrokeRounded(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, float width = 1.0f);
    void StrokeLine(float x1, float y1, float x2, float y2, const D2D1_COLOR_F& color, float width = 1.0f);

    void OnLeftDown(float x, float y);
    void OnLeftUp();
    void OnMouseMove(float x, float y);
    void OnMouseLeave();
    void OnCaptureChanged();
    void UpdateSliderValue(float x, bool radius);
    void CloseRequested();
    void OpenColorPicker(ColorTarget target = ColorTarget::Accent);
    void CancelColorPicker();
    void CommitColorPicker();
    void ApplyColorDraft(unsigned rgb, bool notify);
    void UpdateColorFromSaturationValue(float x, float y);
    void UpdateColorFromHue(float x);
    void SyncHexFromColor();
    void ApplyHexInput();
    void FocusHexInput(bool selectAll);
    bool HandleHexChar(wchar_t character);
    bool HandleHexKey(WPARAM key);
    void NormalizeHexState();
    void PasteHexFromClipboard();
    void StartHotkeyCapture();
    void CancelHotkeyCapture();
    void HandleHotkeyKey(WPARAM key);
    void HandleCapturedHotkey(UINT key, UINT modifiers);
    void SetThemeMode(ThemeMode mode);
    void SetKeySkin(KeySkin skin);
    void SetKeyPosition(KeyPosition position);
    void SetKeyDelay(int delayMs);
    void SetKeyLetters(bool enabled);
    void SetKeyScale(int percent);
    void SetKeyOpacity(int percent);
    void SetClickVolume(int percent);
    void UpdateKeyDelayValue(float x);
    void UpdateClickVolumeValue(float x);
    void UpdateKeyScaleValue(float x);
    void UpdateKeyOpacityValue(float x);
    void UpdateStackDelayValue(float x);
    std::vector<FocusId> BuildFocusOrder() const;
    void SetFocusId(FocusId id);
    void AdvanceFocus(bool backwards);
    bool HandleFocusKey(WPARAM key);
    void ActivateFocus();
    void DrawFocusRings();
    void DrawFocusRingFor(FocusId id);
    void AdvanceColorFocus(bool backwards);
    bool HandleColorPickerKey(WPARAM key);
    void DrawColorFocusRings();
    HCURSOR CursorForPoint(float x, float y) const;

    D2D1_RECT_F NavRect(int index) const;
    D2D1_RECT_F CloseRect() const;
    D2D1_RECT_F EnableRowRect() const;
    D2D1_RECT_F HaloStyleChipRect(int index) const;
    D2D1_RECT_F RadiusTrackRect() const;
    D2D1_RECT_F OpacityTrackRect() const;
    D2D1_RECT_F AutoStartRowRect() const;
    D2D1_RECT_F CustomColorRect() const;
    D2D1_RECT_F ColorCardRect() const;
    D2D1_RECT_F SaturationValueRect() const;
    D2D1_RECT_F HueRect() const;
    D2D1_RECT_F ColorCancelRect() const;
    D2D1_RECT_F ColorDoneRect() const;
    D2D1_RECT_F HexInputRect() const;
    D2D1_RECT_F HotkeyPillRect() const;
    D2D1_RECT_F ThemeSegmentRect() const;
    D2D1_RECT_F ThemeChipRect(int index) const;
    D2D1_RECT_F ShowKeysRowRect() const;
    D2D1_RECT_F ShowKeysToggleRect() const;
    D2D1_RECT_F StackToggleRect() const;
    D2D1_RECT_F StackDelayTrackRect() const;
    D2D1_RECT_F SkinSwatchRect(int index) const;
    D2D1_RECT_F SkinImageButtonRect() const;
    D2D1_RECT_F SkinImageClearRect() const;
    void ImportKeyBackground();
    D2D1_RECT_F KeySkinChipRect(int index) const;
    D2D1_RECT_F KeyPositionChipRect(int index) const;
    D2D1_RECT_F KeyDelayTrackRect() const;
    D2D1_RECT_F KeyScaleTrackRect() const;
    D2D1_RECT_F KeyOpacityTrackRect() const;
    D2D1_RECT_F KeyLettersRowRect() const;
    D2D1_RECT_F KeyLettersToggleRect() const;
    D2D1_RECT_F KeyTextAutoRect() const;
    D2D1_RECT_F KeyTextColorRect() const;
    D2D1_RECT_F KeyStyleColorRect() const;
    D2D1_RECT_F TimerToggleRect(int index) const;
    D2D1_RECT_F TimerResetButtonRect() const;
    D2D1_RECT_F ClickColorRect(int index) const;
    D2D1_RECT_F ClickToggleRect(int index) const;
    D2D1_RECT_F ClickPresetChipRect(int index) const;
    D2D1_RECT_F ClickImportRect() const;
    D2D1_RECT_F ClickVolumeTrackRect() const;
    void ImportClickSound();
    bool HitTestInteractive(float x, float y) const;
    void SwitchPage(Page page);

    HINSTANCE instance_ = nullptr;
    AppSettings* settings_ = nullptr;
    SettingsWindowHost* host_ = nullptr;
    HWND hwnd_ = nullptr;
    bool classRegistered_ = false;
    bool visible_ = false;
    Page page_ = Page::Highlight;

    float scale_ = 1.0f;
    D2D1_POINT_2F mouse_ = D2D1::Point2F(-1.0f, -1.0f);
    bool trackingLeave_ = false;
    int hoverNav_ = -1;
    bool hoverClose_ = false;

    bool draggingWindow_ = false;
    POINT dragStartCursor_ = {};
    RECT dragStartWindow_ = {};
    SliderDrag sliderDrag_ = SliderDrag::None;

    bool colorPickerOpen_ = false;
    ColorDrag colorDrag_ = ColorDrag::None;
    ColorTarget colorTarget_ = ColorTarget::Accent;
    unsigned colorDraftRgb_ = 0x5B8DFF;
    unsigned colorBeforePicker_ = 0;
    bool colorBeforeKeyTextAuto_ = true;
    bool colorBeforeKeyStyleAuto_ = true;
    unsigned colorBeforeKeyTextRgb_ = 0xFFFFFF;
    float colorHue_ = 0.0f;
    float colorSaturation_ = 0.0f;
    float colorValue_ = 1.0f;
    bool hoverColorCancel_ = false;
    bool hoverColorDone_ = false;
    bool hexFocused_ = false;
    bool hexSelectAll_ = false;
    wchar_t hexSkipChar_ = L'\0';
    int hexCaret_ = 0;
    std::wstring hexEdit_;

    bool hotkeyRecording_ = false;
    bool hotkeyPillHover_ = false;
    std::wstring hotkeyError_;
    bool darkMode_ = false;
    int hoverThemeChip_ = -1;
    FocusId focus_ = FocusId::None;
    ColorFocus colorFocus_ = ColorFocus::Hex;
    UiPalette palette_ = LightPalette();
    static HHOOK hotkeyHook_;
    static HWND hotkeyHookTarget_;

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<ID2D1Layer> previewLayer_;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> cursorGeometry_;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> brandFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> subtitleFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> bodyFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> bodyStrongFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> captionFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> captionCenterFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> navFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> valueRightFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> centerFormat_;

    UINT targetWidth_ = 0;
    UINT targetHeight_ = 0;
};
