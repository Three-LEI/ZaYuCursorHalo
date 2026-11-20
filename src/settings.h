#pragma once

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

inline std::wstring FormatRgbHex(unsigned rgb)
{
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"%06X", rgb & 0xFFFFFFu);
    return buffer;
}

inline bool ParseRgbHex(const wchar_t* text, unsigned& value, unsigned fallback)
{
    if (!text)
    {
        value = fallback;
        return false;
    }
    wchar_t* end = nullptr;
    const unsigned long parsed = wcstoul(text, &end, 16);
    if (end == text || parsed > 0xFFFFFFul)
    {
        value = fallback;
        return false;
    }
    value = static_cast<unsigned>(parsed);
    return true;
}

enum class HaloStyle
{
    Ring,
    Double,
    Crosshair,
    Dot,
};

inline HaloStyle ParseHaloStyle(const std::wstring& text)
{
    if (_wcsicmp(text.c_str(), L"double") == 0)
        return HaloStyle::Double;
    if (_wcsicmp(text.c_str(), L"crosshair") == 0)
        return HaloStyle::Crosshair;
    if (_wcsicmp(text.c_str(), L"dot") == 0)
        return HaloStyle::Dot;
    return HaloStyle::Ring;
}

inline std::wstring FormatHaloStyle(HaloStyle style)
{
    switch (style)
    {
    case HaloStyle::Double:
        return L"double";
    case HaloStyle::Crosshair:
        return L"crosshair";
    case HaloStyle::Dot:
        return L"dot";
    default:
        return L"ring";
    }
}

enum class ClickSoundPreset
{
    Default,
    Soft,
    Crisp,
    Wood,
    Synth,
    Custom,
};

inline ClickSoundPreset ParseClickSoundPreset(const std::wstring& text)
{
    if (_wcsicmp(text.c_str(), L"soft") == 0)
        return ClickSoundPreset::Soft;
    if (_wcsicmp(text.c_str(), L"crisp") == 0)
        return ClickSoundPreset::Crisp;
    if (_wcsicmp(text.c_str(), L"wood") == 0)
        return ClickSoundPreset::Wood;
    if (_wcsicmp(text.c_str(), L"synth") == 0)
        return ClickSoundPreset::Synth;
    if (_wcsicmp(text.c_str(), L"custom") == 0)
        return ClickSoundPreset::Custom;
    return ClickSoundPreset::Default;
}

inline std::wstring FormatClickSoundPreset(ClickSoundPreset preset)
{
    switch (preset)
    {
    case ClickSoundPreset::Soft:
        return L"soft";
    case ClickSoundPreset::Crisp:
        return L"crisp";
    case ClickSoundPreset::Wood:
        return L"wood";
    case ClickSoundPreset::Synth:
        return L"synth";
    case ClickSoundPreset::Custom:
        return L"custom";
    default:
        return L"default";
    }
}

enum class TimerMode
{
    CountUp,
    CountDown,
};

inline TimerMode ParseTimerMode(const std::wstring& text)
{
    return _wcsicmp(text.c_str(), L"down") == 0 ? TimerMode::CountDown : TimerMode::CountUp;
}

inline std::wstring FormatTimerMode(TimerMode mode)
{
    return mode == TimerMode::CountDown ? L"down" : L"up";
}

constexpr int kMinCountdownMinutes = 1;
constexpr int kMaxCountdownMinutes = 99;

inline int ClampCountdownMinutes(int value)
{
    return (std::max)(kMinCountdownMinutes, (std::min)(kMaxCountdownMinutes, value));
}

struct AppSettings
{
    bool enabled = true;
    bool startWithWindows = false;
    int radiusDip = 36;
    int opacityPercent = 95;
    unsigned accentRgb = 0x5B8DFF;
    std::wstring haloStyle = L"ring";
    std::wstring hotkey = L"Ctrl+Alt+H";
    std::wstring theme = L"system";
    bool showKeys = true;
    std::wstring keySkin = L"classic";
    std::wstring keyPosition = L"bottom";
    int keyDelayMs = 650;
    bool showLetterKeys = false;
    int keyScalePercent = 100;
    int keyOpacityPercent = 100;
    bool keyStack = false;
    int keyStackDelayMs = 650;
    unsigned keyFillRgb = 0xFFFFFF;
    unsigned keyBorderRgb = 0xC9CDD6;
    bool keyStyleAuto = true;
    unsigned keyStyleRgb = 0xFFFFFF;
    std::wstring keyBackgroundImage;
    bool keyTextAuto = true;
    unsigned keyTextRgb = 0xFFFFFF;
    int keyCustomX = 0;
    int keyCustomY = 0;
    bool keyCustomSet = false;
    bool clickRippleEnabled = false;
    bool clickSoundEnabled = false;
    unsigned clickLeftRgb = 0x5B8DFF;
    unsigned clickRightRgb = 0xFF6B6B;
    std::wstring clickSoundPreset = L"default";
    std::wstring clickSoundFile;

    bool timerEnabled = true;
    bool timerAutoStart = false;
    bool timerShowHours = true;
    bool timerResetOnStop = false;
    std::wstring timerMode = L"up";
    int timerCountdownMinutes = 5;
    int timerX = 0;
    int timerY = 0;
    bool timerHasPosition = false;
    bool trailEnabled = false;
    bool firstRun = true;
};

class SettingsStore
{
public:
    static std::wstring DefaultPath();
    static AppSettings Load(const std::wstring& path);
    static bool Save(const std::wstring& path, const AppSettings& settings);
};

std::wstring GetExecutablePath();
bool ApplyAutoStart(bool enabled);
bool AutoStartEntryExists();
