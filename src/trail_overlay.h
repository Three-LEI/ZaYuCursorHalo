#pragma once

#include <windows.h>

#include <atomic>
#include <deque>
#include <thread>

struct AppContext;

namespace Gdiplus
{
class Bitmap;
}

class TrailOverlay
{
public:
    TrailOverlay() = default;
    ~TrailOverlay();

    TrailOverlay(const TrailOverlay&) = delete;
    TrailOverlay& operator=(const TrailOverlay&) = delete;

    bool Start(AppContext* context);
    void Stop();
    bool IsRunning() const { return running_.load(); }

private:
    struct Sample
    {
        POINT point = {};
        ULONGLONG time = 0;
    };

    void Run();
    bool CreateTrailWindow();
    void Update();
    void Render();
    void Prune(ULONGLONG now);
    void RebuildSurface(int width, int height);
    void Hide();
    void Cleanup();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    AppContext* context_ = nullptr;
    std::thread thread_;
    HANDLE readyEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> installed_{false};

    HWND hwnd_ = nullptr;
    HDC dc_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    Gdiplus::Bitmap* bitmap_ = nullptr;
    int surfaceWidth_ = 0;
    int surfaceHeight_ = 0;
    int windowX_ = 0;
    int windowY_ = 0;
    bool visible_ = false;
    bool classRegistered_ = false;
    bool gdiplusStarted_ = false;
    ULONG_PTR gdiplusToken_ = 0;

    std::deque<Sample> samples_;
    POINT lastCursor_ = {};
    bool hasLastCursor_ = false;
};
