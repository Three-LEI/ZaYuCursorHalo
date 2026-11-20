#pragma once

#include "frame_stats.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>

enum class PacerMode
{
    Dwm,
    Timer,
};

struct AppContext
{
    HINSTANCE instance = nullptr;

    std::atomic<bool> enabled{true};
    std::atomic<bool> running{true};
    std::atomic<bool> benchmark{false};
    std::atomic<bool> settingsOpen{false};
    std::atomic<bool> trayPanelOpen{false};

    HANDLE mouseWake = nullptr;
    HANDLE stateWake = nullptr;
    std::atomic<HWND> clickFeedbackWindow{nullptr};
    std::atomic<bool> clickRippleEnabled{false};
    std::atomic<bool> clickSoundEnabled{false};
    std::atomic<unsigned> clickLeftRgb{0x5B8DFF};
    std::atomic<unsigned> clickRightRgb{0xFF6B6B};
    std::atomic<int> clickSoundPreset{0};
    std::mutex clickSoundMutex;
    std::wstring clickSoundFile;

    PacerMode pacerMode = PacerMode::Dwm;
    std::atomic<int> radiusDip{36};
    std::atomic<int> opacityPercent{95};
    std::atomic<unsigned> accentRgb{0x5B8DFF};
    std::atomic<int> haloStyle{0};
    std::atomic<unsigned> styleVersion{0};

    std::wstring statsPath;
    bool writeStats = false;
    std::wstring dumpFramePath;

    FrameStats stats;
};
