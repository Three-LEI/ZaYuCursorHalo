#pragma once

#include "frame_pacer.h"
#include "overlay_renderer.h"

#include <windows.h>

#include <atomic>
#include <thread>

struct AppContext;

class Overlay
{
public:
    Overlay() = default;
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool Start(HINSTANCE instance, AppContext* context);
    void Stop();

private:
    void Run();
    bool RegisterWindowClass();
    bool CreateOverlayWindow();
    void PumpMessages();
    bool UpdateOverlay(POINT cursor, float scale);
    void ShowOverlay();
    void HideOverlay();
    void EnsureTopmost();
    void Cleanup();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static UINT DpiForPoint(POINT point);

    HINSTANCE instance_ = nullptr;
    AppContext* context_ = nullptr;
    std::thread thread_;
    HANDLE readyEvent_ = nullptr;
    std::atomic<bool> readyOk_{false};

    HWND hwnd_ = nullptr;
    bool classRegistered_ = false;
    bool visible_ = false;
    bool idle_ = false;
    bool hasLastPos_ = false;
    POINT lastPos_ = {};
    float lastScale_ = 0.0f;
    double lastMoveSeconds_ = 0.0;
    double lastPresentSeconds_ = 0.0;
    bool hasPresented_ = false;
    bool dumpedFrame_ = false;
    bool benchmarkMotionStarted_ = false;
    float renderedScale_ = 0.0f;
    int renderedRadiusDip_ = 0;
    unsigned renderedStyleVersion_ = 0;
    bool refreshDirty_ = false;

    OverlayRenderer renderer_;
    FramePacer pacer_;
};
