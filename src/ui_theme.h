#pragma once

#include <windows.h>

#include <d2d1.h>

#include <string>

enum class ThemeMode
{
    System,
    Light,
    Dark,
};

struct UiPalette
{
    D2D1_COLOR_F background;
    D2D1_COLOR_F sidebar;
    D2D1_COLOR_F surface;
    D2D1_COLOR_F surfaceAlt;
    D2D1_COLOR_F border;
    D2D1_COLOR_F divider;
    D2D1_COLOR_F text;
    D2D1_COLOR_F muted;
    D2D1_COLOR_F subtle;
    D2D1_COLOR_F hover;
    D2D1_COLOR_F control;
    D2D1_COLOR_F controlHover;
    D2D1_COLOR_F track;
    D2D1_COLOR_F switchOff;
    D2D1_COLOR_F onSurface;
    D2D1_COLOR_F error;
    D2D1_COLOR_F overlay;
    D2D1_COLOR_F shadow;
};

D2D1_COLOR_F UiColor(unsigned rgb, float alpha = 1.0f);
UiPalette LightPalette();
UiPalette DarkPalette();
UiPalette PaletteFor(bool dark);

ThemeMode ParseThemeMode(const std::wstring& text);
std::wstring FormatThemeMode(ThemeMode mode);
bool IsSystemDarkTheme();
bool ResolveDarkTheme(ThemeMode mode);
