#pragma once

#include <cwchar>

#include <string>

enum class KeySkin
{
    Classic,
    Minimal,
    Neon,
    Paper,
    Terminal,
    Mechanical,
    Win98,
};

enum class KeyPosition
{
    BottomCenter,
    TopCenter,
    FollowCursor,
    Custom,
};

inline unsigned ScaleRgb(unsigned rgb, float factor)
{
    const auto scale = [factor](unsigned channel) {
        const float value = static_cast<float>(channel) * factor;
        return static_cast<unsigned>((value < 0.0f) ? 0.0f : ((value > 255.0f) ? 255.0f : value));
    };
    return (scale((rgb >> 16) & 0xFF) << 16) |
        (scale((rgb >> 8) & 0xFF) << 8) |
        scale(rgb & 0xFF);
}

inline float RgbLuminanceValue(unsigned rgb)
{
    return (0.2126f * static_cast<float>((rgb >> 16) & 0xFF) +
        0.7152f * static_cast<float>((rgb >> 8) & 0xFF) +
        0.0722f * static_cast<float>(rgb & 0xFF)) / 255.0f;
}

inline unsigned DerivedKeyBorder(unsigned rgb)
{
    return RgbLuminanceValue(rgb) > 0.55f ? ScaleRgb(rgb, 0.55f) : ScaleRgb(rgb, 1.55f);
}

constexpr int kDefaultKeyDelayMs = 650;
constexpr int kMinKeyDelayMs = 0;
constexpr int kMaxKeyDelayMs = 2000;

inline KeySkin ParseKeySkin(const std::wstring& text)
{
    if (_wcsicmp(text.c_str(), L"minimal") == 0)
        return KeySkin::Minimal;
    if (_wcsicmp(text.c_str(), L"mechanical") == 0)
        return KeySkin::Mechanical;
    if (_wcsicmp(text.c_str(), L"win98") == 0)
        return KeySkin::Win98;
    if (_wcsicmp(text.c_str(), L"neon") == 0 ||
        _wcsicmp(text.c_str(), L"paper") == 0 ||
        _wcsicmp(text.c_str(), L"terminal") == 0)
    {
        return KeySkin::Minimal;
    }
    return KeySkin::Classic;
}

inline std::wstring FormatKeySkin(KeySkin skin)
{
    switch (skin)
    {
    case KeySkin::Minimal:
        return L"minimal";
    case KeySkin::Neon:
        return L"neon";
    case KeySkin::Paper:
        return L"paper";
    case KeySkin::Terminal:
        return L"terminal";
    case KeySkin::Mechanical:
        return L"mechanical";
    case KeySkin::Win98:
        return L"win98";
    default:
        return L"classic";
    }
}

inline KeyPosition ParseKeyPosition(const std::wstring& text)
{
    if (_wcsicmp(text.c_str(), L"top") == 0 || _wcsicmp(text.c_str(), L"top_center") == 0)
        return KeyPosition::TopCenter;
    if (_wcsicmp(text.c_str(), L"cursor") == 0 || _wcsicmp(text.c_str(), L"follow_cursor") == 0)
        return KeyPosition::FollowCursor;
    if (_wcsicmp(text.c_str(), L"custom") == 0)
        return KeyPosition::Custom;
    return KeyPosition::BottomCenter;
}

inline std::wstring FormatKeyPosition(KeyPosition position)
{
    switch (position)
    {
    case KeyPosition::TopCenter:
        return L"top";
    case KeyPosition::FollowCursor:
        return L"cursor";
    case KeyPosition::Custom:
        return L"custom";
    default:
        return L"bottom";
    }
}

inline int ClampKeyDelay(int value)
{
    if (value < kMinKeyDelayMs)
        return kMinKeyDelayMs;
    if (value > kMaxKeyDelayMs)
        return kMaxKeyDelayMs;
    return value;
}
