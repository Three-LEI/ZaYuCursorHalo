#include "ui_theme.h"

#include <algorithm>
#include <cwchar>

D2D1_COLOR_F UiColor(unsigned rgb, float alpha)
{
    return D2D1::ColorF(
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha);
}

UiPalette LightPalette()
{
    UiPalette palette;
    palette.background = UiColor(0xFAFBFC);
    palette.sidebar = UiColor(0xF3F4F6);
    palette.surface = UiColor(0xFFFFFF);
    palette.surfaceAlt = UiColor(0xF7F8FB);
    palette.border = UiColor(0xE2E4E9);
    palette.divider = UiColor(0xECEEF2);
    palette.text = UiColor(0x1B1C1F);
    palette.muted = UiColor(0x73777F);
    palette.subtle = UiColor(0x969AA2);
    palette.hover = UiColor(0xF0F2F5);
    palette.control = UiColor(0xF2F3F5);
    palette.controlHover = UiColor(0xECEEF3);
    palette.track = UiColor(0xE1E4E9);
    palette.switchOff = UiColor(0xD5D8DE);
    palette.onSurface = UiColor(0xFFFFFF);
    palette.error = UiColor(0xB4543C);
    palette.overlay = UiColor(0x101114, 0.20f);
    palette.shadow = UiColor(0x101114, 0.08f);
    return palette;
}

UiPalette DarkPalette()
{
    UiPalette palette;
    palette.background = UiColor(0x17181B);
    palette.sidebar = UiColor(0x1E2024);
    palette.surface = UiColor(0x24262B);
    palette.surfaceAlt = UiColor(0x1E2024);
    palette.border = UiColor(0x34373E);
    palette.divider = UiColor(0x2C2F35);
    palette.text = UiColor(0xF0F1F3);
    palette.muted = UiColor(0xA2A7B0);
    palette.subtle = UiColor(0x7E848D);
    palette.hover = UiColor(0x2A2D33);
    palette.control = UiColor(0x2B2E34);
    palette.controlHover = UiColor(0x33373E);
    palette.track = UiColor(0x3C4048);
    palette.switchOff = UiColor(0x454951);
    palette.onSurface = UiColor(0xF7F8FA);
    palette.error = UiColor(0xE08B75);
    palette.overlay = UiColor(0x000000, 0.48f);
    palette.shadow = UiColor(0x000000, 0.30f);
    return palette;
}

UiPalette PaletteFor(bool dark)
{
    return dark ? DarkPalette() : LightPalette();
}

ThemeMode ParseThemeMode(const std::wstring& text)
{
    if (_wcsicmp(text.c_str(), L"dark") == 0)
        return ThemeMode::Dark;
    if (_wcsicmp(text.c_str(), L"light") == 0)
        return ThemeMode::Light;
    return ThemeMode::System;
}

std::wstring FormatThemeMode(ThemeMode mode)
{
    switch (mode)
    {
    case ThemeMode::Dark:
        return L"dark";
    case ThemeMode::Light:
        return L"light";
    default:
        return L"system";
    }
}

bool IsSystemDarkTheme()
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS result = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return result == ERROR_SUCCESS && value == 0;
}

bool ResolveDarkTheme(ThemeMode mode)
{
    if (mode == ThemeMode::Dark)
        return true;
    if (mode == ThemeMode::Light)
        return false;
    return IsSystemDarkTheme();
}
