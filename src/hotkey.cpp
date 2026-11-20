#include "hotkey.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace
{
std::wstring Trim(const std::wstring& value)
{
    size_t begin = 0;
    while (begin < value.size() && iswspace(value[begin]))
        ++begin;
    size_t end = value.size();
    while (end > begin && iswspace(value[end - 1]))
        --end;
    return value.substr(begin, end - begin);
}

std::wstring KeyName(UINT vk)
{
    if (vk >= 'A' && vk <= 'Z')
        return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= '0' && vk <= '9')
        return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24)
    {
        wchar_t buffer[8] = {};
        swprintf_s(buffer, L"F%u", vk - VK_F1 + 1);
        return buffer;
    }
    switch (vk)
    {
    case VK_SPACE: return L"Space";
    case VK_TAB: return L"Tab";
    case VK_RETURN: return L"Enter";
    case VK_BACK: return L"Backspace";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"PgUp";
    case VK_NEXT: return L"PgDn";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    default: return std::wstring();
    }
}

bool ParseKey(const std::wstring& token, UINT& vk)
{
    if (token.size() == 1)
    {
        const wchar_t character = static_cast<wchar_t>(towupper(token[0]));
        if ((character >= L'A' && character <= L'Z') ||
            (character >= L'0' && character <= L'9'))
        {
            vk = character;
            return true;
        }
    }

    if ((token[0] == L'F' || token[0] == L'f') && token.size() >= 2 && token.size() <= 3)
    {
        wchar_t* end = nullptr;
        const long number = wcstol(token.c_str() + 1, &end, 10);
        if (end != token.c_str() + 1 && *end == L'\0' && number >= 1 && number <= 24)
        {
            vk = VK_F1 + static_cast<UINT>(number - 1);
            return true;
        }
    }

    struct KeyAlias
    {
        const wchar_t* name;
        UINT vk;
    };
    static const KeyAlias aliases[] = {
        {L"space", VK_SPACE},
        {L"spacebar", VK_SPACE},
        {L"tab", VK_TAB},
        {L"enter", VK_RETURN},
        {L"return", VK_RETURN},
        {L"backspace", VK_BACK},
        {L"left", VK_LEFT},
        {L"right", VK_RIGHT},
        {L"up", VK_UP},
        {L"down", VK_DOWN},
        {L"home", VK_HOME},
        {L"end", VK_END},
        {L"pgup", VK_PRIOR},
        {L"pageup", VK_PRIOR},
        {L"pgdn", VK_NEXT},
        {L"pagedown", VK_NEXT},
        {L"insert", VK_INSERT},
        {L"ins", VK_INSERT},
        {L"delete", VK_DELETE},
        {L"del", VK_DELETE},
    };
    for (const KeyAlias& alias : aliases)
    {
        if (_wcsicmp(token.c_str(), alias.name) == 0)
        {
            vk = alias.vk;
            return true;
        }
    }
    return false;
}
} // namespace

HotkeyBinding DefaultHotkey()
{
    return HotkeyBinding{MOD_CONTROL | MOD_ALT, 'H'};
}

bool IsModifierKey(UINT vk)
{
    switch (vk)
    {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

bool IsSupportedHotkeyKey(UINT vk)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return true;
    if (vk >= VK_F1 && vk <= VK_F24)
        return vk != VK_F12;
    switch (vk)
    {
    case VK_SPACE:
    case VK_RETURN:
    case VK_BACK:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
        return true;
    default:
        return false;
    }
}

bool IsValidHotkey(const HotkeyBinding& binding)
{
    return binding.modifiers != 0 && IsSupportedHotkeyKey(binding.vk);
}

std::wstring FormatHotkey(const HotkeyBinding& binding)
{
    std::wstring text;
    auto append = [&](const wchar_t* part) {
        if (!text.empty())
            text += L"+";
        text += part;
    };
    if ((binding.modifiers & MOD_CONTROL) != 0)
        append(L"Ctrl");
    if ((binding.modifiers & MOD_ALT) != 0)
        append(L"Alt");
    if ((binding.modifiers & MOD_SHIFT) != 0)
        append(L"Shift");
    if ((binding.modifiers & MOD_WIN) != 0)
        append(L"Win");
    const std::wstring key = KeyName(binding.vk);
    if (!key.empty())
    {
        if (!text.empty())
            text += L"+";
        text += key;
    }
    return text;
}

bool ParseHotkey(const std::wstring& text, HotkeyBinding& binding)
{
    HotkeyBinding parsed;
    parsed.modifiers = 0;
    parsed.vk = 0;

    size_t start = 0;
    bool haveKey = false;
    while (start <= text.size())
    {
        size_t end = text.find(L'+', start);
        if (end == std::wstring::npos)
            end = text.size();
        const std::wstring token = Trim(text.substr(start, end - start));
        if (!token.empty())
        {
            if (_wcsicmp(token.c_str(), L"ctrl") == 0 || _wcsicmp(token.c_str(), L"control") == 0)
            {
                parsed.modifiers |= MOD_CONTROL;
            }
            else if (_wcsicmp(token.c_str(), L"alt") == 0)
            {
                parsed.modifiers |= MOD_ALT;
            }
            else if (_wcsicmp(token.c_str(), L"shift") == 0)
            {
                parsed.modifiers |= MOD_SHIFT;
            }
            else if (_wcsicmp(token.c_str(), L"win") == 0 || _wcsicmp(token.c_str(), L"windows") == 0)
            {
                parsed.modifiers |= MOD_WIN;
            }
            else
            {
                if (haveKey || !ParseKey(token, parsed.vk))
                    return false;
                haveKey = true;
            }
        }
        if (end == text.size())
            break;
        start = end + 1;
    }

    if (!haveKey || !IsValidHotkey(parsed))
        return false;
    binding = parsed;
    return true;
}
