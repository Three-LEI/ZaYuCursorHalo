#pragma once

#include <windows.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

struct AppContext;
namespace Gdiplus { class Bitmap; }

// Owns the short-lived click-ripple layered windows and plays the optional
// left/right click sounds. It runs on its own small message-loop thread so a
// click never touches the overlay's render loop.
class ClickFeedback
{
public:
    ClickFeedback() = default;
    ~ClickFeedback();

    ClickFeedback(const ClickFeedback&) = delete;
    ClickFeedback& operator=(const ClickFeedback&) = delete;

    bool Start(AppContext* context);
    void Stop();
    void Preview(bool right);

private:
    struct Slot
    {
        HWND hwnd = nullptr;
        HDC dc = nullptr;
        HBITMAP dib = nullptr;
        HGDIOBJ oldBitmap = nullptr;
        BYTE* bits = nullptr;
        int size = 0;
        float scale = 1.0f;
        bool active = false;
        ULONGLONG startedAt = 0;
        POINT center = {};
        int button = 0;
        Gdiplus::Bitmap* bitmap = nullptr;
    };

    void Run();
    bool CreateController();
    bool CreateRippleWindows();
    bool CreateSlotSurface(Slot& slot, int pixelSize);
    void DestroySlot(Slot& slot);
    void OnClick(int button, POINT point);
    void PlayClickSound(int preset, bool right);
    void PlayCustomSound(const std::wstring& path, int volume);
    bool PlayMciFile(const std::wstring& path);
    void CloseMci();
    void OnTick();
    void RenderSlot(Slot& slot, ULONGLONG now);
    void HideSlot(Slot& slot);
    static LRESULT CALLBACK ControllerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK RippleProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static UINT SystemDpi();

    static constexpr UINT kClickMessage = WM_APP + 70;
    static constexpr UINT kPreviewMessage = WM_APP + 71;
    static constexpr UINT_PTR kAnimationTimer = 1;
    static constexpr int kRippleWindowDip = 132;
    static constexpr int kSlotCount = 4;
    static constexpr ULONGLONG kRippleLifetimeMs = 420;

    AppContext* context_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    HANDLE readyEvent_ = nullptr;
    HWND controller_ = nullptr;
    std::vector<Slot> slots_;
    ULONG_PTR gdiplusToken_ = 0;
    bool gdiplusStarted_ = false;
    bool controllerClassRegistered_ = false;
    bool rippleClassRegistered_ = false;
    int windowSize_ = 0;
    std::wstring mciPath_;
};
