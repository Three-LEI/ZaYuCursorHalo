#pragma once

#include <windows.h>

#include <string>

struct HotkeyBinding
{
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT vk = 'H';
};

HotkeyBinding DefaultHotkey();
bool IsModifierKey(UINT vk);
bool IsSupportedHotkeyKey(UINT vk);
bool IsValidHotkey(const HotkeyBinding& binding);
std::wstring FormatHotkey(const HotkeyBinding& binding);
bool ParseHotkey(const std::wstring& text, HotkeyBinding& binding);
