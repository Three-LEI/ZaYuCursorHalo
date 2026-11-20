#include "settings.h"

#include "hotkey.h"
#include "key_display.h"
#include "ui_theme.h"
#include "util.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>

namespace
{
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"ZaYuCursorHalo";
constexpr wchar_t kLegacyRunValue[] = L"CursorHalo";
constexpr wchar_t kAppDataFolder[] = L"ZaYuCursorHalo";
constexpr wchar_t kLegacyAppDataFolder[] = L"CursorHalo";

int ClampInt(int value, int minimum, int maximum)
{
    return (std::max)(minimum, (std::min)(maximum, value));
}

std::wstring BuildIni(const AppSettings& settings)
{
    wchar_t color[16] = {};
    swprintf_s(color, L"%06X", settings.accentRgb & 0xFFFFFFu);

    std::wstring text;
    text += L'\uFEFF';
    text += L"[general]\r\n";
    text += L"enabled=";
    text += settings.enabled ? L"1\r\n" : L"0\r\n";
    text += L"start_with_windows=";
    text += settings.startWithWindows ? L"1\r\n" : L"0\r\n";
    text += L"hotkey=";
    text += settings.hotkey;
    text += L"\r\ntheme=";
    text += settings.theme;
    text += L"\r\nshow_keys=";
    text += settings.showKeys ? L"1" : L"0";
    text += L"\r\nfirst_run=";
    text += settings.firstRun ? L"1" : L"0";
    text += L"\r\n\r\n[highlight]\r\n";
    text += L"radius=";
    text += std::to_wstring(settings.radiusDip);
    text += L"\r\nopacity=";
    text += std::to_wstring(settings.opacityPercent);
    text += L"\r\ncolor=";
    text += color;
    text += L"\r\nstyle=";
    text += settings.haloStyle;
    text += L"\r\n\r\n[keys]\r\nskin=";
    text += settings.keySkin;
    text += L"\r\nposition=";
    text += settings.keyPosition;
    text += L"\r\ndelay_ms=";
    text += std::to_wstring(settings.keyDelayMs);
    text += L"\r\nshow_letter_keys=";
    text += settings.showLetterKeys ? L"1" : L"0";
    text += L"\r\nstack=";
    text += settings.keyStack ? L"1" : L"0";
    text += L"\r\nstack_delay_ms=";
    text += std::to_wstring(settings.keyStackDelayMs);
    text += L"\r\nscale_percent=";
    text += std::to_wstring(settings.keyScalePercent);
    text += L"\r\nopacity_percent=";
    text += std::to_wstring(settings.keyOpacityPercent);
    text += L"\r\nstyle_color=";
    text += settings.keyStyleAuto ? L"auto" : FormatRgbHex(settings.keyStyleRgb);
    text += L"\r\nfill_color=";
    text += FormatRgbHex(settings.keyFillRgb);
    text += L"\r\nborder_color=";
    text += FormatRgbHex(settings.keyBorderRgb);
    text += L"\r\nbackground_image=";
    text += settings.keyBackgroundImage;
    text += L"\r\ntext_color=";
    if (settings.keyTextAuto)
    {
        text += L"auto";
    }
    else
    {
        wchar_t keyTextColor[16] = {};
        swprintf_s(keyTextColor, L"%06X", settings.keyTextRgb & 0xFFFFFFu);
        text += keyTextColor;
    }
    text += L"\r\ncustom_x=";
    text += std::to_wstring(settings.keyCustomX);
    text += L"\r\ncustom_y=";
    text += std::to_wstring(settings.keyCustomY);
    text += L"\r\ncustom_set=";
    text += settings.keyCustomSet ? L"1" : L"0";
    text += L"\r\n\r\n[effects]\r\nclick_ripple=";
    text += settings.clickRippleEnabled ? L"1" : L"0";
    text += L"\r\nclick_sound=";
    text += settings.clickSoundEnabled ? L"1" : L"0";
    text += L"\r\nleft_color=";
    text += FormatRgbHex(settings.clickLeftRgb);
    text += L"\r\nright_color=";
    text += FormatRgbHex(settings.clickRightRgb);
    text += L"\r\nsound_preset=";
    text += settings.clickSoundPreset;
    text += L"\r\nsound_file=";
    text += settings.clickSoundFile;
    text += L"\r\n\r\n[timer]\r\nenabled=";
    text += settings.timerEnabled ? L"1" : L"0";
    text += L"\r\nauto_start=";
    text += settings.timerAutoStart ? L"1" : L"0";
    text += L"\r\nshow_hours=";
    text += settings.timerShowHours ? L"1" : L"0";
    text += L"\r\nreset_on_stop=";
    text += settings.timerResetOnStop ? L"1" : L"0";
    text += L"\r\nmode=";
    text += settings.timerMode;
    text += L"\r\ncountdown_minutes=";
    text += std::to_wstring(settings.timerCountdownMinutes);
    text += L"\r\nx=";
    text += std::to_wstring(settings.timerX);
    text += L"\r\ny=";
    text += std::to_wstring(settings.timerY);
    text += L"\r\nhas_position=";
    text += settings.timerHasPosition ? L"1" : L"0";
    text += L"\r\n\r\n[trail]\r\nenabled=";
    text += settings.trailEnabled ? L"1" : L"0";
    text += L"\r\n";
    return text;
}

std::wstring GetRunCommand()
{
    const std::wstring exe = GetExecutablePath();
    if (exe.empty())
        return std::wstring();
    return L"\"" + exe + L"\"";
}
} // namespace

std::wstring SettingsStore::DefaultPath()
{
    static std::wstring cached;
    if (!cached.empty())
        return cached;

    const std::wstring roaming = GetRoamingAppDataDirectory();
    if (roaming.empty())
        return std::wstring();

    const std::wstring directory = roaming + L"\\" + kAppDataFolder;
    const std::wstring path = directory + L"\\config.ini";
    CreateDirectoryW(directory.c_str(), nullptr);

    // One-time migration from the pre-rename CursorHalo profile. Keep the old
    // folder intact; custom sound paths in the copied ini remain valid.
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        const std::wstring legacyPath =
            roaming + L"\\" + kLegacyAppDataFolder + L"\\config.ini";
        if (GetFileAttributesW(legacyPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            CopyFileW(legacyPath.c_str(), path.c_str(), FALSE);
    }

    cached = path;
    return cached;
}

AppSettings SettingsStore::Load(const std::wstring& path)
{
    AppSettings settings;
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return settings;

    settings.enabled = GetPrivateProfileIntW(L"general", L"enabled", 1, path.c_str()) != 0;
    settings.startWithWindows = GetPrivateProfileIntW(L"general", L"start_with_windows", 0, path.c_str()) != 0;

    wchar_t hotkey[128] = {};
    GetPrivateProfileStringW(L"general", L"hotkey", L"Ctrl+Alt+H", hotkey, _countof(hotkey), path.c_str());
    HotkeyBinding hotkeyBinding;
    if (ParseHotkey(hotkey, hotkeyBinding))
        settings.hotkey = FormatHotkey(hotkeyBinding);
    else
        settings.hotkey = FormatHotkey(DefaultHotkey());

    wchar_t theme[32] = {};
    GetPrivateProfileStringW(L"general", L"theme", L"system", theme, _countof(theme), path.c_str());
    settings.theme = FormatThemeMode(ParseThemeMode(theme));
    settings.showKeys = GetPrivateProfileIntW(L"general", L"show_keys", 1, path.c_str()) != 0;
    settings.firstRun = GetPrivateProfileIntW(L"general", L"first_run", 1, path.c_str()) != 0;

    settings.radiusDip = ClampInt(GetPrivateProfileIntW(L"highlight", L"radius", 36, path.c_str()), 20, 80);
    settings.opacityPercent = ClampInt(GetPrivateProfileIntW(L"highlight", L"opacity", 95, path.c_str()), 30, 100);

    wchar_t color[32] = {};
    GetPrivateProfileStringW(L"highlight", L"color", L"5B8DFF", color, _countof(color), path.c_str());
    wchar_t* end = nullptr;
    const unsigned long parsed = wcstoul(color, &end, 16);
    if (end != color && parsed <= 0xFFFFFFul)
        settings.accentRgb = static_cast<unsigned>(parsed);

    wchar_t haloStyle[32] = {};
    GetPrivateProfileStringW(L"highlight", L"style", L"ring", haloStyle, _countof(haloStyle), path.c_str());
    settings.haloStyle = FormatHaloStyle(ParseHaloStyle(haloStyle));

    wchar_t keySkin[32] = {};
    GetPrivateProfileStringW(L"keys", L"skin", L"classic", keySkin, _countof(keySkin), path.c_str());
    settings.keySkin = FormatKeySkin(ParseKeySkin(keySkin));

    wchar_t keyPosition[32] = {};
    GetPrivateProfileStringW(L"keys", L"position", L"bottom", keyPosition, _countof(keyPosition), path.c_str());
    settings.keyPosition = FormatKeyPosition(ParseKeyPosition(keyPosition));

    settings.keyDelayMs = ClampKeyDelay(
        GetPrivateProfileIntW(L"keys", L"delay_ms", kDefaultKeyDelayMs, path.c_str()));
    settings.showLetterKeys = GetPrivateProfileIntW(L"keys", L"show_letter_keys", 0, path.c_str()) != 0;
    settings.keyStack = GetPrivateProfileIntW(L"keys", L"stack", 0, path.c_str()) != 0;
    const int stackDelay = static_cast<int>(
        GetPrivateProfileIntW(L"keys", L"stack_delay_ms", 650, path.c_str()));
    settings.keyStackDelayMs = (std::max)(0, (std::min)(3000, stackDelay));
    settings.keyScalePercent = (std::max)(70, (std::min)(
        150, static_cast<int>(GetPrivateProfileIntW(L"keys", L"scale_percent", 100, path.c_str()))));
    settings.keyOpacityPercent = (std::max)(20, (std::min)(
        100, static_cast<int>(GetPrivateProfileIntW(L"keys", L"opacity_percent", 100, path.c_str()))));
    wchar_t keyColor[32] = {};
    GetPrivateProfileStringW(L"keys", L"fill_color", L"FFFFFF", keyColor, _countof(keyColor), path.c_str());
    ParseRgbHex(keyColor, settings.keyFillRgb, 0xFFFFFF);
    GetPrivateProfileStringW(L"keys", L"border_color", L"C9CDD6", keyColor, _countof(keyColor), path.c_str());
    ParseRgbHex(keyColor, settings.keyBorderRgb, 0xC9CDD6);
    GetPrivateProfileStringW(L"keys", L"style_color", L"auto", keyColor, _countof(keyColor), path.c_str());
    settings.keyStyleAuto = _wcsicmp(keyColor, L"auto") == 0;
    if (!settings.keyStyleAuto)
        ParseRgbHex(keyColor, settings.keyStyleRgb, 0xFFFFFF);
    wchar_t keyBackground[1024] = {};
    GetPrivateProfileStringW(L"keys", L"background_image", L"", keyBackground, _countof(keyBackground), path.c_str());
    settings.keyBackgroundImage.clear();

    wchar_t keyTextColor[32] = {};
    GetPrivateProfileStringW(L"keys", L"text_color", L"auto", keyTextColor, _countof(keyTextColor), path.c_str());
    if (_wcsicmp(keyTextColor, L"auto") == 0)
    {
        settings.keyTextAuto = true;
    }
    else
    {
        wchar_t* keyTextEnd = nullptr;
        const unsigned long parsedKeyText = wcstoul(keyTextColor, &keyTextEnd, 16);
        if (keyTextEnd != keyTextColor && parsedKeyText <= 0xFFFFFFul)
        {
            settings.keyTextRgb = static_cast<unsigned>(parsedKeyText);
            settings.keyTextAuto = false;
        }
    }

    settings.keyCustomX = GetPrivateProfileIntW(L"keys", L"custom_x", 0, path.c_str());
    settings.keyCustomY = GetPrivateProfileIntW(L"keys", L"custom_y", 0, path.c_str());
    settings.keyCustomSet = GetPrivateProfileIntW(L"keys", L"custom_set", 0, path.c_str()) != 0;
    if (ParseKeyPosition(settings.keyPosition) == KeyPosition::Custom && !settings.keyCustomSet)
        settings.keyPosition = L"bottom";
    settings.clickRippleEnabled = GetPrivateProfileIntW(L"effects", L"click_ripple", 0, path.c_str()) != 0;
    settings.clickSoundEnabled = GetPrivateProfileIntW(L"effects", L"click_sound", 0, path.c_str()) != 0;
    wchar_t effectColor[32] = {};
    GetPrivateProfileStringW(L"effects", L"left_color", L"5B8DFF", effectColor, _countof(effectColor), path.c_str());
    ParseRgbHex(effectColor, settings.clickLeftRgb, 0x5B8DFF);
    GetPrivateProfileStringW(L"effects", L"right_color", L"FF6B6B", effectColor, _countof(effectColor), path.c_str());
    ParseRgbHex(effectColor, settings.clickRightRgb, 0xFF6B6B);
    wchar_t soundPreset[32] = {};
    GetPrivateProfileStringW(L"effects", L"sound_preset", L"default", soundPreset, _countof(soundPreset), path.c_str());
    settings.clickSoundPreset = FormatClickSoundPreset(ParseClickSoundPreset(soundPreset));
    wchar_t soundFile[1024] = {};
    GetPrivateProfileStringW(L"effects", L"sound_file", L"", soundFile, _countof(soundFile), path.c_str());
    settings.clickSoundFile = soundFile;
    settings.timerEnabled = GetPrivateProfileIntW(L"timer", L"enabled", 1, path.c_str()) != 0;
    settings.timerAutoStart = GetPrivateProfileIntW(L"timer", L"auto_start", 0, path.c_str()) != 0;
    settings.timerShowHours = GetPrivateProfileIntW(L"timer", L"show_hours", 1, path.c_str()) != 0;
    settings.timerResetOnStop = GetPrivateProfileIntW(L"timer", L"reset_on_stop", 0, path.c_str()) != 0;
    wchar_t timerMode[16] = {};
    GetPrivateProfileStringW(L"timer", L"mode", L"up", timerMode, _countof(timerMode), path.c_str());
    settings.timerMode = FormatTimerMode(ParseTimerMode(timerMode));
    settings.timerCountdownMinutes = ClampCountdownMinutes(
        GetPrivateProfileIntW(L"timer", L"countdown_minutes", 5, path.c_str()));
    settings.timerX = GetPrivateProfileIntW(L"timer", L"x", 0, path.c_str());
    settings.timerY = GetPrivateProfileIntW(L"timer", L"y", 0, path.c_str());
    settings.timerHasPosition = GetPrivateProfileIntW(L"timer", L"has_position", 0, path.c_str()) != 0;
    settings.trailEnabled = GetPrivateProfileIntW(L"trail", L"enabled", 0, path.c_str()) != 0;
    return settings;
}

bool SettingsStore::Save(const std::wstring& path, const AppSettings& settings)
{
    if (!EnsureParentDirectoryExists(path))
        return false;

    const std::wstring text = BuildIni(settings);
    const std::wstring tempPath = path + L".tmp";

    HANDLE file = CreateFileW(
        tempPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    const DWORD bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    DWORD written = 0;
    const BOOL writeOk = WriteFile(file, text.c_str(), bytes, &written, nullptr) &&
        written == bytes;
    const BOOL flushOk = writeOk ? FlushFileBuffers(file) : FALSE;
    CloseHandle(file);

    if (!writeOk || !flushOk)
    {
        DeleteFileW(tempPath.c_str());
        return false;
    }

    if (!MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tempPath.c_str());
        return false;
    }
    return true;
}

std::wstring GetExecutablePath()
{
    std::wstring buffer(512, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return std::wstring();
        if (length < buffer.size() - 1)
        {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool ApplyAutoStart(bool enabled)
{
    HKEY key = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKey,
        0,
        nullptr,
        0,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        nullptr,
        &key,
        nullptr);
    if (openResult != ERROR_SUCCESS)
        return false;

    LONG result = ERROR_SUCCESS;
    if (enabled)
    {
        const std::wstring command = GetRunCommand();
        if (command.empty())
            result = ERROR_FILE_NOT_FOUND;
        else
        {
            result = RegSetValueExW(
                key,
                kRunValue,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        }
    }
    else
    {
        result = RegDeleteValueW(key, kRunValue);
        if (result == ERROR_FILE_NOT_FOUND)
            result = ERROR_SUCCESS;
    }

    // Remove the old auto-start entry so Windows 7 does not keep trying to
    // launch the pre-rename executable.
    RegDeleteValueW(key, kLegacyRunValue);

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool AutoStartEntryExists()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;
    const LONG result = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
