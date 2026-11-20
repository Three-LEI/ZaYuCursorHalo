#include "app_context.h"
#include "click_feedback.h"
#include "hotkey.h"
#include "mouse_hook.h"
#include "overlay.h"
#include "resource.h"
#include "settings.h"
#include "key_display.h"
#include "key_overlay.h"
#include "trail_overlay.h"
#include "settings_window.h"
#include "timer_widget.h"
#include "tray_panel.h"
#include "ui_theme.h"
#include "util.h"

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>

namespace
{
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT kHotkeyId = 1;
constexpr UINT kBenchmarkTimerId = 1;
constexpr UINT kSaveTimerId = 2;
constexpr UINT kFirstRunMessage = WM_APP + 41;
constexpr UINT kKeyPlacementMessage = WM_APP + 42;
constexpr wchar_t kMainClass[] = L"ZaYuCursorHalo.Main.P0";
constexpr wchar_t kMutexName[] = L"Local\\ZaYuCursorHalo.P0.Singleton";
constexpr wchar_t kAppTitle[] = L"ZaYuCursorHalo";
constexpr wchar_t kAppVersion[] = L"0.3.0-p1";

UINT g_taskbarCreatedMessage = 0;

struct Options
{
    PacerMode pacer = PacerMode::Dwm;
    int radiusDip = 36;
    bool radiusSpecified = false;
    int benchmarkSeconds = 0;
    bool writeStats = false;
    bool openSettings = false;
    bool openTrayPanel = false;
    std::wstring statsPath;
    std::wstring dumpFramePath;
};

bool StartsWithNoCase(const wchar_t* text, const wchar_t* prefix)
{
    return _wcsnicmp(text, prefix, wcslen(prefix)) == 0;
}

int ParseInt(const wchar_t* text, int fallback)
{
    wchar_t* end = nullptr;
    const long value = wcstol(text, &end, 10);
    if (end == text)
        return fallback;
    return static_cast<int>(value);
}

std::wstring ArgumentValue(int argc, wchar_t** argv, int& index, const wchar_t* prefix)
{
    const std::wstring arg = argv[index];
    const size_t prefixLength = wcslen(prefix);
    if (arg.size() > prefixLength && arg[prefixLength] == L'=')
        return arg.substr(prefixLength + 1);
    if (arg.size() == prefixLength && index + 1 < argc)
        return argv[++index];
    return std::wstring();
}

Options ParseOptions()
{
    Options options;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return options;

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (StartsWithNoCase(arg.c_str(), L"--benchmark"))
        {
            const std::wstring value = ArgumentValue(argc, argv, i, L"--benchmark");
            if (!value.empty())
                options.benchmarkSeconds = (std::max)(1, (std::min)(3600, ParseInt(value.c_str(), 0)));
            continue;
        }
        if (StartsWithNoCase(arg.c_str(), L"--pacer"))
        {
            const std::wstring value = ArgumentValue(argc, argv, i, L"--pacer");
            if (_wcsicmp(value.c_str(), L"timer") == 0)
                options.pacer = PacerMode::Timer;
            else
                options.pacer = PacerMode::Dwm;
            continue;
        }
        if (StartsWithNoCase(arg.c_str(), L"--radius"))
        {
            const std::wstring value = ArgumentValue(argc, argv, i, L"--radius");
            if (!value.empty())
            {
                options.radiusDip = (std::max)(16, (std::min)(120, ParseInt(value.c_str(), 36)));
                options.radiusSpecified = true;
            }
            continue;
        }
        if (StartsWithNoCase(arg.c_str(), L"--settings"))
        {
            options.openSettings = true;
            continue;
        }
        if (StartsWithNoCase(arg.c_str(), L"--tray-panel"))
        {
            options.openTrayPanel = true;
            continue;
        }
        if (StartsWithNoCase(arg.c_str(), L"--stats"))
        {
            const std::wstring value = ArgumentValue(argc, argv, i, L"--stats");
            if (!value.empty())
            {
                options.statsPath = value;
                options.writeStats = true;
            }
            continue;
        }
        if (StartsWithNoCase(arg.c_str(), L"--dump-frame"))
        {
            const std::wstring value = ArgumentValue(argc, argv, i, L"--dump-frame");
            if (!value.empty())
                options.dumpFramePath = value;
            continue;
        }
    }

    LocalFree(argv);
    if (options.benchmarkSeconds > 0)
        options.writeStats = true;
    return options;
}

std::wstring TrayTooltip(bool enabled, const std::wstring& hotkey)
{
    return std::wstring(L"ZaYuCursorHalo · ") + (enabled ? L"高亮已开启" : L"高亮已暂停") + L" · " + hotkey;
}

void WriteStartupError(const std::wstring& path, const wchar_t* message)
{
    if (path.empty())
        return;
    if (!EnsureParentDirectoryExists(path))
        return;
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file)
    {
        std::fprintf(file, "ZaYuCursorHalo P0 startup error\nmessage=%ls\n", message);
        std::fclose(file);
    }
}
} // namespace

class App : private SettingsWindowHost, private TrayPanelHost, private TimerWidgetHost
{
public:
    ~App()
    {
        Shutdown();
    }

    int Run(HINSTANCE instance, const Options& options)
    {
        if (!Initialize(instance, options))
        {
            Shutdown();
            return 1;
        }

        MSG message = {};
        int exitCode = 0;
        for (;;)
        {
            const BOOL result = GetMessageW(&message, nullptr, 0, 0);
            if (result == 0)
                break;
            if (result == -1)
            {
                exitCode = 1;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        Shutdown();
        return exitCode;
    }

private:
    bool Initialize(HINSTANCE instance, const Options& options)
    {
        instance_ = instance;
        benchSeconds_ = options.benchmarkSeconds;
        context_.instance = instance;
        context_.pacerMode = options.pacer;
        context_.benchmark.store(benchSeconds_ > 0);
        context_.writeStats = options.writeStats;
        context_.statsPath = options.statsPath;
        context_.dumpFramePath = options.dumpFramePath;
        if (context_.writeStats && context_.statsPath.empty())
            context_.statsPath = GetDefaultStatsPath();

        settingsPath_ = SettingsStore::DefaultPath();
        settings_ = SettingsStore::Load(settingsPath_);
        if (options.radiusSpecified)
            settings_.radiusDip = options.radiusDip;
        if (benchSeconds_ > 0)
            settings_.enabled = true;
        if (!ParseHotkey(settings_.hotkey, hotkeyBinding_))
            hotkeyBinding_ = DefaultHotkey();
        settings_.hotkey = FormatHotkey(hotkeyBinding_);
        themeMode_ = ParseThemeMode(settings_.theme);
        settings_.theme = FormatThemeMode(themeMode_);
        darkMode_ = ResolveDarkTheme(themeMode_);
        context_.enabled.store(settings_.enabled);
        context_.radiusDip.store(settings_.radiusDip);
        context_.opacityPercent.store(settings_.opacityPercent);
        context_.accentRgb.store(settings_.accentRgb);
        context_.haloStyle.store(static_cast<int>(ParseHaloStyle(settings_.haloStyle)));
        context_.styleVersion.store(1);

        context_.mouseWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        context_.stateWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!context_.mouseWake || !context_.stateWake)
        {
            ReportError(L"无法创建内部唤醒事件。");
            return false;
        }

        g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &App::WndProc;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = kMainClass;
        if (!RegisterClassExW(&windowClass))
        {
            ReportError(L"无法注册主窗口类。");
            return false;
        }
        mainClassRegistered_ = true;

        hwnd_ = CreateWindowExW(
            0,
            kMainClass,
            kAppTitle,
            WS_OVERLAPPED,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr,
            instance_,
            this);
        if (!hwnd_)
        {
            ReportError(L"无法创建主消息窗口。");
            return false;
        }

        if (!LoadTrayIcons())
            return false;

        if (!settingsWindow_.Create(instance_, &settings_, this))
        {
            ReportError(L"无法创建设置窗口。");
            return false;
        }
        if (!trayPanel_.Create(instance_, this))
        {
            ReportError(L"无法创建托盘面板。");
            return false;
        }
        settingsWindow_.SetDarkMode(darkMode_);
        trayPanel_.SetDarkMode(darkMode_);
        if (!keyOverlay_.Create(instance_))
            keyOverlay_.Destroy();
        keyOverlay_.SetDarkMode(darkMode_);
        keyOverlay_.Configure(
            ParseKeySkin(settings_.keySkin),
            ParseKeyPosition(settings_.keyPosition),
            settings_.keyDelayMs,
            settings_.showLetterKeys,
            settings_.keyStack,
            settings_.keyStackDelayMs,
            settings_.keyScalePercent,
            settings_.keyOpacityPercent,
            settings_.keyTextAuto,
            settings_.keyTextRgb,
            settings_.keyStyleAuto,
            settings_.keyStyleRgb,
            settings_.keyFillRgb,
            settings_.keyBorderRgb,
            settings_.keyBackgroundImage,
            settings_.accentRgb,
            settings_.keyCustomX,
            settings_.keyCustomY);
        if (settings_.showKeys && benchSeconds_ == 0)
            keyOverlay_.SetEnabled(true);

        if (!timerWidget_.Create(instance_))
        {
            timerWidget_.Destroy();
        }
        else
        {
            timerWidget_.SetHost(this);
        }
        if (benchSeconds_ == 0)
        {
            timerWidget_.ApplySettings(
                settings_.timerEnabled,
                settings_.timerShowHours,
                settings_.timerResetOnStop,
                ParseTimerMode(settings_.timerMode),
                settings_.timerCountdownMinutes,
                settings_.accentRgb,
                darkMode_,
                settings_.timerX,
                settings_.timerY,
                settings_.timerHasPosition);
            if (settings_.timerEnabled && settings_.timerAutoStart)
                timerWidget_.Start();
        }

        ApplyAutoStart(settings_.startWithWindows);
        SettingsStore::Save(settingsPath_, settings_);
        AddTrayIcon();

        if (TryRegisterHotkey(hotkeyBinding_))
        {
            hotkeyRegistered_ = true;
        }
        else
        {
            const std::wstring message =
                L"无法注册快捷键 " + settings_.hotkey +
                L"，可能已被其他程序占用。仍可通过托盘图标开关高亮，或到设置中修改。";
            ReportWarning(message.c_str());
        }

        if (!mouseHook_.Start(&context_))
        {
            ReportError(L"无法安装鼠标跟随钩子。");
            return false;
        }

        context_.clickRippleEnabled.store(settings_.clickRippleEnabled && benchSeconds_ == 0);
        context_.clickSoundEnabled.store(settings_.clickSoundEnabled && benchSeconds_ == 0);
        context_.clickLeftRgb.store(settings_.clickLeftRgb);
        context_.clickRightRgb.store(settings_.clickRightRgb);
        context_.clickSoundPreset.store(static_cast<int>(ParseClickSoundPreset(settings_.clickSoundPreset)));
        {
            std::lock_guard<std::mutex> lock(context_.clickSoundMutex);
            context_.clickSoundFile = settings_.clickSoundFile;
        }
        if (benchSeconds_ == 0 && !clickFeedback_.Start(&context_))
            ReportWarning(L"点击波纹/音效初始化失败，其他功能不受影响。");

        if (!overlay_.Start(instance_, &context_))
        {
            ReportError(L"无法创建透明叠加层。");
            return false;
        }

        if (settings_.trailEnabled && benchSeconds_ == 0)
            trailOverlay_.Start(&context_);

        if (benchSeconds_ > 0)
        {
            benchTimer_ = SetTimer(
                hwnd_,
                kBenchmarkTimerId,
                static_cast<UINT>(benchSeconds_ * 1000),
                nullptr);
        }

        if (options.openSettings)
            ShowSettings();
        else if (options.openTrayPanel)
            ShowTrayPanel();
        else if (settings_.firstRun && benchSeconds_ == 0)
            PostMessageW(hwnd_, kFirstRunMessage, 0, 0);

        return true;
    }

    void Shutdown()
    {
        if (shutdown_)
            return;
        shutdown_ = true;
        trailOverlay_.Stop();
        timerWidget_.Destroy();
        keyOverlay_.Destroy();

        if (benchTimer_ && hwnd_)
        {
            KillTimer(hwnd_, kBenchmarkTimerId);
            benchTimer_ = 0;
        }
        if (saveTimer_ && hwnd_)
        {
            KillTimer(hwnd_, kSaveTimerId);
            saveTimer_ = 0;
        }

        context_.running.store(false);
        if (context_.stateWake)
            SetEvent(context_.stateWake);
        if (context_.mouseWake)
            SetEvent(context_.mouseWake);

        clickFeedback_.Stop();
        mouseHook_.Stop();
        overlay_.Stop();
        trayPanel_.Destroy();
        context_.trayPanelOpen.store(false);
        settingsWindow_.Destroy();
        SaveSettingsNow();

        RemoveTrayIcon();

        if (hotkeyRegistered_ && hwnd_)
        {
            UnregisterHotKey(hwnd_, kHotkeyId);
            hotkeyRegistered_ = false;
        }

        if (context_.writeStats && !context_.statsPath.empty())
        {
            if (!context_.stats.WriteSummary(context_.statsPath))
                WriteStartupError(context_.statsPath, L"no frame samples were recorded");
        }

        if (hwnd_)
        {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (mainClassRegistered_)
        {
            UnregisterClassW(kMainClass, instance_);
            mainClassRegistered_ = false;
        }

        if (activeIcon_)
        {
            DestroyIcon(activeIcon_);
            activeIcon_ = nullptr;
        }
        if (pausedIcon_)
        {
            DestroyIcon(pausedIcon_);
            pausedIcon_ = nullptr;
        }

        if (context_.mouseWake)
        {
            CloseHandle(context_.mouseWake);
            context_.mouseWake = nullptr;
        }
        if (context_.stateWake)
        {
            CloseHandle(context_.stateWake);
            context_.stateWake = nullptr;
        }
    }

    bool LoadTrayIcons()
    {
        const int cx = GetSystemMetrics(SM_CXSMICON);
        const int cy = GetSystemMetrics(SM_CYSMICON);
        activeIcon_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_TRAY_ACTIVE), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
        pausedIcon_ = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_TRAY_PAUSED), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
        if (!activeIcon_ || !pausedIcon_)
        {
            ReportError(L"无法加载托盘图标。");
            return false;
        }
        return true;
    }

    void AddTrayIcon()
    {
        if (!hwnd_)
            return;

        trayData_.cbSize = sizeof(trayData_);
        trayData_.hWnd = hwnd_;
        trayData_.uID = kTrayIconId;
        trayData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        trayData_.uCallbackMessage = kTrayMessage;
        trayData_.hIcon = context_.enabled.load() ? activeIcon_ : pausedIcon_;
        wcsncpy_s(trayData_.szTip, TrayTooltip(context_.enabled.load(), settings_.hotkey).c_str(), _TRUNCATE);

        if (!trayAdded_)
        {
            trayAdded_ = Shell_NotifyIconW(NIM_ADD, &trayData_) != FALSE;
            if (trayAdded_)
            {
                NOTIFYICONDATAW versionData = trayData_;
                versionData.uVersion = NOTIFYICON_VERSION_4;
                trayVersion4_ = Shell_NotifyIconW(NIM_SETVERSION, &versionData) != FALSE;
            }
        }
        else
        {
            Shell_NotifyIconW(NIM_MODIFY, &trayData_);
        }
    }

    void UpdateTrayIcon()
    {
        if (!trayAdded_)
            return;
        trayData_.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        trayData_.hIcon = context_.enabled.load() ? activeIcon_ : pausedIcon_;
        if (trayTipSuppressed_)
            trayData_.szTip[0] = L'\0';
        else
            wcsncpy_s(trayData_.szTip, TrayTooltip(context_.enabled.load(), settings_.hotkey).c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &trayData_);
    }

    void RemoveTrayIcon()
    {
        if (trayAdded_)
        {
            Shell_NotifyIconW(NIM_DELETE, &trayData_);
            trayAdded_ = false;
        }
    }

    void Toggle()
    {
        SetEnabled(!context_.enabled.load());
    }

    bool TryRegisterHotkey(const HotkeyBinding& binding)
    {
        return RegisterHotKey(
            hwnd_,
            kHotkeyId,
            binding.modifiers | MOD_NOREPEAT,
            binding.vk) != FALSE;
    }

    bool BeginHotkeyRecording(std::wstring& error) override
    {
        if (hotkeyRecordingActive_)
            return true;
        hotkeyBeforeRecording_ = hotkeyBinding_;
        hotkeyWasRegisteredBeforeRecording_ = hotkeyRegistered_;
        if (hotkeyRegistered_)
        {
            if (!UnregisterHotKey(hwnd_, kHotkeyId))
            {
                error = L"暂时无法暂停当前快捷键，请重试。";
                return false;
            }
            hotkeyRegistered_ = false;
        }
        hotkeyRecordingActive_ = true;
        error.clear();
        return true;
    }

    bool CommitHotkey(const HotkeyBinding& binding, std::wstring& error) override
    {
        if (!hotkeyRecordingActive_)
            return false;
        if (!IsValidHotkey(binding))
        {
            error = L"请同时按住 Ctrl、Alt、Shift 或 Win，再按一个字母、数字或功能键。";
            return false;
        }
        if (!TryRegisterHotkey(binding))
        {
            const DWORD errorCode = GetLastError();
            if (hotkeyWasRegisteredBeforeRecording_ && TryRegisterHotkey(hotkeyBeforeRecording_))
                hotkeyRegistered_ = true;
            hotkeyRecordingActive_ = false;
            error = errorCode == ERROR_HOTKEY_ALREADY_REGISTERED
                ? L"该组合键已被其他程序占用，请换一个。"
                : L"无法注册该快捷键，请换一个组合。";
            return false;
        }

        hotkeyRegistered_ = true;
        hotkeyRecordingActive_ = false;
        hotkeyBinding_ = binding;
        settings_.hotkey = FormatHotkey(binding);
        UpdateTrayIcon();
        if (trayPanel_.IsVisible())
            trayPanel_.SetHotkey(settings_.hotkey);
        QueueSettingsSave();
        return true;
    }

    void CancelHotkeyRecording() override
    {
        if (!hotkeyRecordingActive_)
            return;
        hotkeyRecordingActive_ = false;
        if (hotkeyWasRegisteredBeforeRecording_ && TryRegisterHotkey(hotkeyBeforeRecording_))
            hotkeyRegistered_ = true;
    }

    void SetEnabled(bool enabled)
    {
        context_.enabled.store(enabled);
        settings_.enabled = enabled;
        UpdateTrayIcon();
        if (context_.stateWake)
            SetEvent(context_.stateWake);
        if (trayPanel_.IsVisible())
            trayPanel_.SetState(enabled, settings_.accentRgb);
        keyOverlay_.Configure(
            ParseKeySkin(settings_.keySkin),
            ParseKeyPosition(settings_.keyPosition),
            settings_.keyDelayMs,
            settings_.showLetterKeys,
            settings_.keyStack,
            settings_.keyStackDelayMs,
            settings_.keyScalePercent,
            settings_.keyOpacityPercent,
            settings_.keyTextAuto,
            settings_.keyTextRgb,
            settings_.keyStyleAuto,
            settings_.keyStyleRgb,
            settings_.keyFillRgb,
            settings_.keyBorderRgb,
            settings_.keyBackgroundImage,
            settings_.accentRgb,
            settings_.keyCustomX,
            settings_.keyCustomY);
        QueueSettingsSave();
        if (settingsWindow_.IsVisible())
            settingsWindow_.Refresh();
    }

    void ShowSettings()
    {
        keyOverlay_.SetSuspended(true);
        if (!settingsWindow_.IsVisible())
        {
            context_.settingsOpen.store(true);
            if (context_.stateWake)
                SetEvent(context_.stateWake);
        }
        settingsWindow_.Show();
    }

    void SetTrayTipSuppressed(bool suppressed)
    {
        trayTipSuppressed_ = suppressed;
        if (!trayAdded_)
            return;
        NOTIFYICONDATAW data = trayData_;
        data.uFlags = NIF_TIP | (trayVersion4_ ? NIF_SHOWTIP : 0);
        if (suppressed)
            data.szTip[0] = L'\0';
        else
            wcsncpy_s(data.szTip, TrayTooltip(context_.enabled.load(), settings_.hotkey).c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &data);
    }

    bool ShowTrayPanel(bool firstRunHint = false)
    {
        if (!hwnd_)
            return false;
        if (trayPanel_.IsVisible())
        {
            trayPanel_.Hide();
            return true;
        }

        NOTIFYICONIDENTIFIER identifier = {};
        identifier.cbSize = sizeof(identifier);
        identifier.hWnd = hwnd_;
        identifier.uID = kTrayIconId;
        RECT iconRect = {};
        if (FAILED(Shell_NotifyIconGetRect(&identifier, &iconRect)))
        {
            POINT cursor = {};
            GetCursorPos(&cursor);
            iconRect = {cursor.x - 16, cursor.y - 16, cursor.x + 16, cursor.y + 16};
        }

        trayPanel_.SetFirstRunHint(firstRunHint);
        context_.trayPanelOpen.store(true);
        if (context_.stateWake)
            SetEvent(context_.stateWake);
        if (!trayPanel_.ShowAt(
                iconRect,
                context_.enabled.load(),
                settings_.accentRgb,
                settings_.hotkey,
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled))
        {
            trayPanel_.SetFirstRunHint(false);
            context_.trayPanelOpen.store(false);
            if (context_.stateWake)
                SetEvent(context_.stateWake);
            return false;
        }
        // The tray tooltip can otherwise float above the panel for several
        // seconds because the pointer remains over the icon.
        SetTrayTipSuppressed(true);
        return true;
    }

    void QueueSettingsSave()
    {
        settingsDirty_ = true;
        if (!hwnd_)
            return;
        if (saveTimer_)
            KillTimer(hwnd_, kSaveTimerId);
        saveTimer_ = SetTimer(hwnd_, kSaveTimerId, 500, nullptr);
    }

    void SaveSettingsNow()
    {
        if (saveTimer_ && hwnd_)
        {
            KillTimer(hwnd_, kSaveTimerId);
            saveTimer_ = 0;
        }
        if (!settingsDirty_ || settingsPath_.empty())
            return;
        if (SettingsStore::Save(settingsPath_, settings_))
        {
            settingsDirty_ = false;
            saveWarningShown_ = false;
            return;
        }
        if (!saveWarningShown_)
        {
            saveWarningShown_ = true;
            ReportWarning(L"设置暂时无法保存，请检查用户配置目录是否可写。");
        }
    }

    void OnSettingsEnabledChanged() override
    {
        SetEnabled(settings_.enabled);
    }

    void OnSettingsStyleChanged() override
    {
        context_.radiusDip.store(settings_.radiusDip);
        context_.opacityPercent.store(settings_.opacityPercent);
        context_.accentRgb.store(settings_.accentRgb);
        context_.haloStyle.store(static_cast<int>(ParseHaloStyle(settings_.haloStyle)));
        context_.styleVersion.fetch_add(1);
        if (context_.stateWake)
            SetEvent(context_.stateWake);
        if (trayPanel_.IsVisible())
            trayPanel_.SetState(context_.enabled.load(), settings_.accentRgb);
        timerWidget_.ApplySettings(
            settings_.timerEnabled,
            settings_.timerShowHours,
            settings_.timerResetOnStop,
            ParseTimerMode(settings_.timerMode),
            settings_.timerCountdownMinutes,
            settings_.accentRgb,
            darkMode_,
            settings_.timerX,
            settings_.timerY,
            settings_.timerHasPosition);
        QueueSettingsSave();
    }

    void OnSettingsAutoStartChanged(bool enabled) override
    {
        if (!ApplyAutoStart(enabled))
        {
            settings_.startWithWindows = !enabled;
            settingsWindow_.Refresh();
            ReportWarning(L"无法更新开机启动设置，请检查当前用户注册表权限。");
            return;
        }
        QueueSettingsSave();
    }

    void OnSettingsClosed() override
    {
        if (hotkeyRecordingActive_)
            CancelHotkeyRecording();
        context_.settingsOpen.store(false);
        if (context_.stateWake)
            SetEvent(context_.stateWake);
        keyOverlay_.SetSuspended(false);
        SaveSettingsNow();
    }

    void OnTrayPanelToggle() override
    {
        Toggle();
    }

    void OnTrayPanelShowKeysChanged(bool enabled) override
    {
        OnSettingsShowKeysChanged(enabled);
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
    }

    void OnTrayPanelTimerChanged(bool enabled) override
    {
        settings_.timerEnabled = enabled;
        timerWidget_.ApplySettings(
            settings_.timerEnabled,
            settings_.timerShowHours,
            settings_.timerResetOnStop,
            ParseTimerMode(settings_.timerMode),
            settings_.timerCountdownMinutes,
            settings_.accentRgb,
            darkMode_,
            settings_.timerX,
            settings_.timerY,
            settings_.timerHasPosition);
        if (settingsWindow_.IsVisible())
            settingsWindow_.Refresh();
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnTrayPanelTrailChanged(bool enabled) override
    {
        settings_.trailEnabled = enabled;
        if (enabled)
            trailOverlay_.Start(&context_);
        else
            trailOverlay_.Stop();
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnTrayPanelClickRippleChanged(bool enabled) override
    {
        settings_.clickRippleEnabled = enabled;
        context_.clickRippleEnabled.store(enabled);
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnTrayPanelClickSoundChanged(bool enabled) override
    {
        settings_.clickSoundEnabled = enabled;
        context_.clickSoundEnabled.store(enabled);
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnTrayPanelSettings() override
    {
        ShowSettings();
    }

    void OnTrayPanelExit() override
    {
        RequestExit();
    }

    void OnTrayPanelClosed() override
    {
        SetTrayTipSuppressed(false);
        context_.trayPanelOpen.store(false);
        if (context_.stateWake)
            SetEvent(context_.stateWake);
    }

    void OnSettingsThemeChanged() override
    {
        themeMode_ = ParseThemeMode(settings_.theme);
        settings_.theme = FormatThemeMode(themeMode_);
        darkMode_ = ResolveDarkTheme(themeMode_);
        settingsWindow_.SetDarkMode(darkMode_);
        trayPanel_.SetDarkMode(darkMode_);
        keyOverlay_.SetDarkMode(darkMode_);
        timerWidget_.ApplySettings(
            settings_.timerEnabled,
            settings_.timerShowHours,
            settings_.timerResetOnStop,
            ParseTimerMode(settings_.timerMode),
            settings_.timerCountdownMinutes,
            settings_.accentRgb,
            darkMode_,
            settings_.timerX,
            settings_.timerY,
            settings_.timerHasPosition);
        if (trayPanel_.IsVisible())
            trayPanel_.SetState(context_.enabled.load(), settings_.accentRgb);
        QueueSettingsSave();
    }

    void OnSettingsShowKeysChanged(bool enabled) override
    {
        settings_.showKeys = enabled;
        if (!keyOverlay_.SetEnabled(enabled))
            settings_.showKeys = false;
        settingsWindow_.Refresh();
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnSettingsKeyDisplayChanged() override
    {
        keyOverlay_.Configure(
            ParseKeySkin(settings_.keySkin),
            ParseKeyPosition(settings_.keyPosition),
            settings_.keyDelayMs,
            settings_.showLetterKeys,
            settings_.keyStack,
            settings_.keyStackDelayMs,
            settings_.keyScalePercent,
            settings_.keyOpacityPercent,
            settings_.keyTextAuto,
            settings_.keyTextRgb,
            settings_.keyStyleAuto,
            settings_.keyStyleRgb,
            settings_.keyFillRgb,
            settings_.keyBorderRgb,
            settings_.keyBackgroundImage,
            settings_.accentRgb,
            settings_.keyCustomX,
            settings_.keyCustomY);
        QueueSettingsSave();
    }

    void OnSettingsTimerChanged() override
    {
        timerWidget_.ApplySettings(
            settings_.timerEnabled,
            settings_.timerShowHours,
            settings_.timerResetOnStop,
            ParseTimerMode(settings_.timerMode),
            settings_.timerCountdownMinutes,
            settings_.accentRgb,
            darkMode_,
            settings_.timerX,
            settings_.timerY,
            settings_.timerHasPosition);
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnSettingsTimerResetRequested() override
    {
        timerWidget_.Reset();
    }

    void OnSettingsPreviewClickSound(bool right) override
    {
        clickFeedback_.Preview(right);
    }

    void OnSettingsClickChanged() override
    {
        context_.clickRippleEnabled.store(settings_.clickRippleEnabled);
        context_.clickSoundEnabled.store(settings_.clickSoundEnabled);
        context_.clickLeftRgb.store(settings_.clickLeftRgb);
        context_.clickRightRgb.store(settings_.clickRightRgb);
        context_.clickSoundPreset.store(static_cast<int>(ParseClickSoundPreset(settings_.clickSoundPreset)));
        {
            std::lock_guard<std::mutex> lock(context_.clickSoundMutex);
            context_.clickSoundFile = settings_.clickSoundFile;
        }
        if (trayPanel_.IsVisible())
            trayPanel_.SetToggles(
                settings_.showKeys,
                settings_.timerEnabled,
                settings_.trailEnabled,
                settings_.clickRippleEnabled,
                settings_.clickSoundEnabled);
        QueueSettingsSave();
    }

    void OnTimerSettingsChanged(TimerMode mode, int countdownMinutes) override
    {
        settings_.timerMode = FormatTimerMode(mode);
        settings_.timerCountdownMinutes = ClampCountdownMinutes(countdownMinutes);
        if (settingsWindow_.IsVisible())
            settingsWindow_.Refresh();
        QueueSettingsSave();
    }

    void OnTimerPositionChanged(int x, int y) override
    {
        settings_.timerX = x;
        settings_.timerY = y;
        settings_.timerHasPosition = true;
        QueueSettingsSave();
    }

    void OnSettingsKeyPositionCustomRequested() override
    {
        if (keyOverlay_.IsPlacementActive())
            return;
        settingsWindow_.Hide();
        if (!keyOverlay_.BeginPlacement(hwnd_, kKeyPlacementMessage))
            settingsWindow_.Show();
    }

    void RequestExit()
    {
        if (exitRequested_)
            return;
        exitRequested_ = true;
        PostQuitMessage(0);
    }

    void ReportError(const wchar_t* message) const
    {
        if (context_.benchmark.load() && !context_.statsPath.empty())
            WriteStartupError(context_.statsPath, message);
        else
            MessageBoxW(nullptr, message, L"ZaYuCursorHalo", MB_OK | MB_ICONERROR);
    }

    void ReportWarning(const wchar_t* message) const
    {
        if (context_.benchmark.load() && !context_.statsPath.empty())
            return;
        MessageBoxW(nullptr, message, L"ZaYuCursorHalo", MB_OK | MB_ICONWARNING);
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<App*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }

        if (!self)
            return DefWindowProcW(hwnd, message, wParam, lParam);

        if (g_taskbarCreatedMessage != 0 && message == g_taskbarCreatedMessage)
        {
            self->trayAdded_ = false;
            self->AddTrayIcon();
            if (!self->timerWidget_.IsCreated())
            {
                if (self->timerWidget_.Create(self->instance_))
                {
                    self->timerWidget_.ApplySettings(
                        self->settings_.timerEnabled,
                        self->settings_.timerShowHours,
                        self->settings_.timerResetOnStop,
                        ParseTimerMode(self->settings_.timerMode),
                        self->settings_.timerCountdownMinutes,
                        self->settings_.accentRgb,
                        self->darkMode_,
                        self->settings_.timerX,
                        self->settings_.timerY,
                        self->settings_.timerHasPosition);
                }
            }
            else
            {
                self->timerWidget_.Refresh();
            }
            return 0;
        }

        switch (message)
        {
        case WM_HOTKEY:
            if (wParam == kHotkeyId)
            {
                self->Toggle();
                return 0;
            }
            break;
        case WM_TIMER:
            if (wParam == kBenchmarkTimerId)
            {
                KillTimer(hwnd, kBenchmarkTimerId);
                self->benchTimer_ = 0;
                self->RequestExit();
                return 0;
            }
            if (wParam == kSaveTimerId)
            {
                KillTimer(hwnd, kSaveTimerId);
                self->saveTimer_ = 0;
                self->SaveSettingsNow();
                return 0;
            }
            break;
        case kFirstRunMessage:
            if (self->settings_.firstRun && !self->trayPanel_.IsVisible())
            {
                if (self->ShowTrayPanel(true))
                {
                    self->settings_.firstRun = false;
                    SettingsStore::Save(self->settingsPath_, self->settings_);
                }
            }
            return 0;
        case kKeyPlacementMessage:
            if (wParam != 0)
            {
                self->settings_.keyPosition = L"custom";
                self->settings_.keyCustomX = GET_X_LPARAM(lParam);
                self->settings_.keyCustomY = GET_Y_LPARAM(lParam);
                self->settings_.keyCustomSet = true;
                self->keyOverlay_.Configure(
                    ParseKeySkin(self->settings_.keySkin),
                    KeyPosition::Custom,
                    self->settings_.keyDelayMs,
                    self->settings_.showLetterKeys,
                    self->settings_.keyStack,
                    self->settings_.keyStackDelayMs,
                    self->settings_.keyScalePercent,
                    self->settings_.keyOpacityPercent,
                    self->settings_.keyTextAuto,
                    self->settings_.keyTextRgb,
                    self->settings_.keyStyleAuto,
                    self->settings_.keyStyleRgb,
                    self->settings_.keyFillRgb,
                    self->settings_.keyBorderRgb,
                    self->settings_.keyBackgroundImage,
                    self->settings_.accentRgb,
                    self->settings_.keyCustomX,
                    self->settings_.keyCustomY);
                self->QueueSettingsSave();
            }
            self->settingsWindow_.Refresh();
            self->settingsWindow_.Show();
            return 0;
        case kTrayMessage:
        {
            // With NOTIFYICON_VERSION_4 the event is in LOWORD(lParam) and
            // the icon coordinates are packed into wParam. Older versions put
            // the mouse message directly in lParam.
            const UINT event = self->trayVersion4_
                ? LOWORD(lParam)
                : static_cast<UINT>(lParam);
            if (event == NIN_SELECT || event == NIN_KEYSELECT || event == WM_LBUTTONUP)
            {
                self->Toggle();
                return 0;
            }
            // Version 4 sends both WM_RBUTTONUP and WM_CONTEXTMENU. Acting on
            // both would open the panel and immediately close it again, so use
            // only WM_CONTEXTMENU as the authoritative right-click event.
            const bool rightClick = self->trayVersion4_
                ? (event == WM_CONTEXTMENU)
                : (event == WM_CONTEXTMENU || event == WM_RBUTTONUP);
            if (rightClick)
            {
                self->ShowTrayPanel();
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            self->RequestExit();
            return 0;
        case WM_DISPLAYCHANGE:
            self->timerWidget_.Refresh();
            return 0;
        case WM_SETTINGCHANGE:
            self->timerWidget_.Refresh();
            if (self->themeMode_ == ThemeMode::System && lParam)
            {
                const wchar_t* area = reinterpret_cast<const wchar_t*>(lParam);
                if (area && _wcsicmp(area, L"ImmersiveColorSet") == 0)
                {
                    const bool dark = ResolveDarkTheme(ThemeMode::System);
                    if (dark != self->darkMode_)
                    {
                        self->darkMode_ = dark;
                        self->settingsWindow_.SetDarkMode(dark);
                        self->trayPanel_.SetDarkMode(dark);
                        self->keyOverlay_.SetDarkMode(dark);
                    }
                }
            }
            break;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam)
                self->RequestExit();
            return 0;
        case WM_DESTROY:
            if (!self->exitRequested_)
                PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    AppContext context_;
    AppSettings settings_;
    SettingsWindow settingsWindow_;
    TrayPanel trayPanel_;
    KeyOverlay keyOverlay_;
    TimerWidget timerWidget_;
    TrailOverlay trailOverlay_;
    MouseHook mouseHook_;
    ClickFeedback clickFeedback_;
    Overlay overlay_;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HICON activeIcon_ = nullptr;
    HICON pausedIcon_ = nullptr;
    NOTIFYICONDATAW trayData_ = {};
    std::wstring settingsPath_;
    bool trayAdded_ = false;
    bool trayVersion4_ = false;
    bool trayTipSuppressed_ = false;
    bool hotkeyRegistered_ = false;
    bool hotkeyRecordingActive_ = false;
    ThemeMode themeMode_ = ThemeMode::System;
    bool darkMode_ = false;
    bool hotkeyWasRegisteredBeforeRecording_ = false;
    HotkeyBinding hotkeyBinding_;
    HotkeyBinding hotkeyBeforeRecording_;
    bool mainClassRegistered_ = false;
    bool exitRequested_ = false;
    bool shutdown_ = false;
    bool settingsDirty_ = false;
    bool saveWarningShown_ = false;
    UINT_PTR benchTimer_ = 0;
    UINT_PTR saveTimer_ = 0;
    int benchSeconds_ = 0;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"ZaYuCursorHalo 已在运行。", L"ZaYuCursorHalo", MB_OK | MB_ICONINFORMATION);
        CloseHandle(instanceMutex);
        return 0;
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);
    const Options options = ParseOptions();

    App app;
    const int result = app.Run(instance, options);

    if (comInitialized)
        CoUninitialize();
    if (instanceMutex)
        CloseHandle(instanceMutex);
    return result;
}
