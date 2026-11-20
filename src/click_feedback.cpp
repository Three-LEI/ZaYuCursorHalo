#include "click_feedback.h"

#include "app_context.h"
#include "resource.h"
#include "settings.h"

#include <windowsx.h>
#include <mmsystem.h>
#include <objidl.h>
#include <propidl.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using std::max;
using std::min;

#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
constexpr wchar_t kControllerClass[] = L"ZaYuCursorHalo.ClickFeedback.P1";
constexpr wchar_t kRippleClass[] = L"ZaYuCursorHalo.ClickRipple.P1";

Gdiplus::Color AccentColor(unsigned rgb, float alpha)
{
    return Gdiplus::Color(
        static_cast<BYTE>((std::max)(0.0f, (std::min)(1.0f, alpha)) * 255.0f),
        static_cast<BYTE>((rgb >> 16) & 0xFF),
        static_cast<BYTE>((rgb >> 8) & 0xFF),
        static_cast<BYTE>(rgb & 0xFF));
}

Gdiplus::Color MixedColor(unsigned a, unsigned b, float t)
{
    const auto mix = [t](unsigned x, unsigned y) {
        return static_cast<BYTE>(std::lround(
            static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return Gdiplus::Color(
        255,
        mix((a >> 16) & 0xFF, (b >> 16) & 0xFF),
        mix((a >> 8) & 0xFF, (b >> 8) & 0xFF),
        mix(a & 0xFF, b & 0xFF));
}

struct WavView
{
    WAVEFORMATEX format = {};
    const BYTE* data = nullptr;
    DWORD dataSize = 0;
};

bool ParseWav(const std::vector<BYTE>& bytes, WavView& view)
{
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
    {
        return false;
    }

    const BYTE* cursor = bytes.data() + 12;
    const BYTE* end = bytes.data() + bytes.size();
    bool hasFormat = false;
    while (cursor + 8 <= end)
    {
        const DWORD chunkSize =
            static_cast<DWORD>(cursor[4]) |
            (static_cast<DWORD>(cursor[5]) << 8) |
            (static_cast<DWORD>(cursor[6]) << 16) |
            (static_cast<DWORD>(cursor[7]) << 24);
        const BYTE* body = cursor + 8;
        if (body + chunkSize > end)
            return false;

        if (std::memcmp(cursor, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            const size_t copySize = (std::min)(sizeof(WAVEFORMATEX), static_cast<size_t>(chunkSize));
            std::memcpy(&view.format, body, copySize);
            if (copySize < sizeof(WAVEFORMATEX))
                view.format.cbSize = 0;
            hasFormat = true;
        }
        else if (std::memcmp(cursor, "data", 4) == 0)
        {
            view.data = body;
            view.dataSize = chunkSize;
        }

        cursor = body + chunkSize + (chunkSize & 1u);
    }

    if (!hasFormat || !view.data || view.dataSize == 0)
        return false;
    if (view.format.wFormatTag != WAVE_FORMAT_PCM)
        return false;
    if (view.format.nChannels != 1 && view.format.nChannels != 2)
        return false;
    if (view.format.wBitsPerSample != 8 && view.format.wBitsPerSample != 16)
        return false;
    if (view.format.nBlockAlign == 0 || view.format.nSamplesPerSec == 0)
        return false;
    return true;
}

std::mutex g_waveMutex;
std::atomic<unsigned> g_waveGeneration{0};

void PlayPcmAtVolume(
    WAVEFORMATEX format,
    std::vector<BYTE> pcm,
    int volume,
    HINSTANCE instance,
    UINT resourceId,
    std::wstring fallbackPath,
    unsigned generation)
{
    const int clampedVolume = (std::max)(0, (std::min)(100, volume));
    if (clampedVolume <= 0 || pcm.empty())
        return;

    // waveOut/WDM is not reliably re-entrant for rapid previews on every
    // system. Keep one PCM stream open at a time.
    std::lock_guard<std::mutex> waveLock(g_waveMutex);
    if (generation != g_waveGeneration.load())
        return;

    if (format.wBitsPerSample == 16)
    {
        const size_t count = pcm.size() / sizeof(short);
        short* samples = reinterpret_cast<short*>(pcm.data());
        for (size_t i = 0; i < count; ++i)
        {
            const int value = static_cast<int>(samples[i]) * clampedVolume / 100;
            samples[i] = static_cast<short>((std::max)(-32768, (std::min)(32767, value)));
        }
    }
    else
    {
        for (BYTE& sample : pcm)
        {
            const int value = 128 + (static_cast<int>(sample) - 128) * clampedVolume / 100;
            sample = static_cast<BYTE>((std::max)(0, (std::min)(255, value)));
        }
    }

    const double durationMs =
        static_cast<double>(pcm.size()) /
        static_cast<double>((std::max)(1u, static_cast<unsigned>(format.nAvgBytesPerSec))) * 1000.0;
    if (durationMs < 150.0 && format.nBlockAlign > 0 && format.nSamplesPerSec > 0)
    {
        // Very short clicks can lose the first few milliseconds to WDM device
        // spin-up (the 40 ms wood preset is especially prone to this). Pad
        // both ends with silence, but keep the audible part at the same level.
        const size_t padFrames = format.nSamplesPerSec * 60 / 1000;
        const size_t padBytes = padFrames * format.nBlockAlign;
        std::vector<BYTE> padded(
            pcm.size() + padBytes * 2,
            format.wBitsPerSample == 8 ? 0x80 : 0x00);
        std::memcpy(padded.data() + padBytes, pcm.data(), pcm.size());
        pcm.swap(padded);
    }

    HWAVEOUT output = nullptr;
    MMRESULT opened = MMSYSERR_ERROR;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        opened = waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
        if (opened == MMSYSERR_NOERROR)
            break;
        Sleep(3);
    }
    if (opened != MMSYSERR_NOERROR)
    {
        if (resourceId)
        {
            PlaySoundW(
                MAKEINTRESOURCEW(resourceId),
                instance,
                SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
        }
        else if (!fallbackPath.empty())
        {
            PlaySoundW(
                fallbackPath.c_str(),
                nullptr,
                SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
        }
        return;
    }

    bool queued = false;
    WAVEHDR header = {};
    header.lpData = reinterpret_cast<LPSTR>(pcm.data());
    header.dwBufferLength = static_cast<DWORD>(pcm.size());
    if (waveOutPrepareHeader(output, &header, sizeof(header)) == MMSYSERR_NOERROR)
    {
        if (waveOutWrite(output, &header, sizeof(header)) == MMSYSERR_NOERROR)
        {
            queued = true;
            bool done = false;
            for (int i = 0; i < 6000; ++i)
            {
                if ((header.dwFlags & WHDR_DONE) != 0)
                {
                    done = true;
                    break;
                }
                Sleep(5);
            }
            if (!done)
                waveOutReset(output);
        }
        waveOutUnprepareHeader(output, &header, sizeof(header));
    }
    waveOutClose(output);

    if (!queued)
    {
        if (resourceId)
        {
            PlaySoundW(
                MAKEINTRESOURCEW(resourceId),
                instance,
                SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
        }
        else if (!fallbackPath.empty())
        {
            PlaySoundW(
                fallbackPath.c_str(),
                nullptr,
                SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
        }
    }
}

bool PlayWavAtVolume(
    const std::vector<BYTE>& bytes,
    int volume,
    HINSTANCE instance,
    UINT resourceId,
    const std::wstring& fallbackPath)
{
    WavView view;
    if (!ParseWav(bytes, view))
        return false;
    std::vector<BYTE> pcm(view.data, view.data + view.dataSize);
    const unsigned generation = ++g_waveGeneration;
    try
    {
        std::thread(
            PlayPcmAtVolume,
            view.format,
            std::move(pcm),
            volume,
            instance,
            resourceId,
            fallbackPath,
            generation).detach();
    }
    catch (...)
    {
        return false;
    }
    return true;
}

std::vector<BYTE> ReadResourceBytes(HINSTANCE instance, UINT id)
{
    std::vector<BYTE> bytes;
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(id), L"WAVE");
    if (!resource)
        return bytes;
    HGLOBAL loaded = LoadResource(instance, resource);
    const DWORD size = SizeofResource(instance, resource);
    const void* data = loaded ? LockResource(loaded) : nullptr;
    if (!data || size == 0)
        return bytes;
    const BYTE* first = static_cast<const BYTE*>(data);
    bytes.assign(first, first + size);
    return bytes;
}

std::vector<BYTE> ReadFileBytes(const std::wstring& path)
{
    std::vector<BYTE> bytes;
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return bytes;
    LARGE_INTEGER size = {};
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 32 * 1024 * 1024)
    {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
            read != static_cast<DWORD>(bytes.size()))
        {
            bytes.clear();
        }
    }
    CloseHandle(file);
    return bytes;
}
} // namespace

ClickFeedback::~ClickFeedback()
{
    Stop();
}

bool ClickFeedback::Start(AppContext* context)
{
    if (!context)
        return false;

    Stop();
    context_ = context;
    running_.store(true);
    readyEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent_)
    {
        running_.store(false);
        context_ = nullptr;
        return false;
    }

    thread_ = std::thread(&ClickFeedback::Run, this);
    const DWORD wait = WaitForSingleObject(readyEvent_, 3000);
    const bool ok = wait == WAIT_OBJECT_0 && running_.load() && controller_ != nullptr;
    if (!ok)
        Stop();
    return ok;
}

void ClickFeedback::Preview(bool right)
{
    if (controller_)
        PostMessageW(controller_, kPreviewMessage, right ? 1 : 0, 0);
}

void ClickFeedback::Stop()
{
    running_.store(false);
    if (controller_)
        PostMessageW(controller_, WM_CLOSE, 0, 0);
    if (thread_.joinable())
        thread_.join();
    if (readyEvent_)
    {
        CloseHandle(readyEvent_);
        readyEvent_ = nullptr;
    }
    if (context_)
        context_->clickFeedbackWindow.store(nullptr);
    controller_ = nullptr;
    context_ = nullptr;
    slots_.clear();
}

void ClickFeedback::Run()
{
    Gdiplus::GdiplusStartupInput startupInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &startupInput, nullptr) == Gdiplus::Ok)
        gdiplusStarted_ = true;

    WNDCLASSEXW controllerClass = {};
    controllerClass.cbSize = sizeof(controllerClass);
    controllerClass.lpfnWndProc = ControllerProc;
    controllerClass.hInstance = context_ ? context_->instance : nullptr;
    controllerClass.lpszClassName = kControllerClass;
    controllerClassRegistered_ =
        RegisterClassExW(&controllerClass) != 0 ||
        GetLastError() == ERROR_CLASS_ALREADY_EXISTS;

    WNDCLASSEXW rippleClass = {};
    rippleClass.cbSize = sizeof(rippleClass);
    rippleClass.lpfnWndProc = RippleProc;
    rippleClass.hInstance = context_ ? context_->instance : nullptr;
    rippleClass.lpszClassName = kRippleClass;
    rippleClassRegistered_ =
        RegisterClassExW(&rippleClass) != 0 ||
        GetLastError() == ERROR_CLASS_ALREADY_EXISTS;

    const bool created =
        controllerClassRegistered_ && rippleClassRegistered_ &&
        CreateController() && CreateRippleWindows();
    if (!created)
    {
        for (Slot& slot : slots_)
            DestroySlot(slot);
        slots_.clear();
        if (controller_)
        {
            DestroyWindow(controller_);
            controller_ = nullptr;
        }
        running_.store(false);
        if (readyEvent_)
            SetEvent(readyEvent_);
        return;
    }

    context_->clickFeedbackWindow.store(controller_);
    if (readyEvent_)
        SetEvent(readyEvent_);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    KillTimer(controller_, kAnimationTimer);
    CloseMci();
    context_->clickFeedbackWindow.store(nullptr);
    for (Slot& slot : slots_)
        DestroySlot(slot);
    slots_.clear();
    if (controller_)
    {
        DestroyWindow(controller_);
        controller_ = nullptr;
    }
    if (rippleClassRegistered_ && context_)
        UnregisterClassW(kRippleClass, context_->instance);
    if (controllerClassRegistered_ && context_)
        UnregisterClassW(kControllerClass, context_->instance);
    rippleClassRegistered_ = false;
    controllerClassRegistered_ = false;
    if (gdiplusStarted_)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusStarted_ = false;
        gdiplusToken_ = 0;
    }
    running_.store(false);
}

bool ClickFeedback::CreateController()
{
    controller_ = CreateWindowExW(
        0,
        kControllerClass,
        L"ZaYuCursorHalo click feedback",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        context_->instance,
        nullptr);
    if (controller_)
        SetWindowLongPtrW(controller_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return controller_ != nullptr;
}

bool ClickFeedback::CreateRippleWindows()
{
    const float scale = static_cast<float>(SystemDpi()) / 96.0f;
    windowSize_ = (std::max)(48, static_cast<int>(std::lround(kRippleWindowDip * scale)));
    slots_.resize(kSlotCount);
    for (Slot& slot : slots_)
    {
        slot.scale = scale;
        slot.size = windowSize_;
        slot.hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW |
                WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kRippleClass,
            L"ZaYuCursorHalo click ripple",
            WS_POPUP,
            -10000,
            -10000,
            windowSize_,
            windowSize_,
            nullptr,
            nullptr,
            context_->instance,
            nullptr);
        if (!slot.hwnd || !CreateSlotSurface(slot, windowSize_))
            return false;
    }
    return true;
}

bool ClickFeedback::CreateSlotSurface(Slot& slot, int pixelSize)
{
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = pixelSize;
    info.bmiHeader.biHeight = -pixelSize;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    if (!screen)
        return false;
    slot.dib = CreateDIBSection(
        screen,
        &info,
        DIB_RGB_COLORS,
        reinterpret_cast<void**>(&slot.bits),
        nullptr,
        0);
    slot.dc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!slot.dc || !slot.dib || !slot.bits)
        return false;

    slot.oldBitmap = SelectObject(slot.dc, slot.dib);
    slot.bitmap = new Gdiplus::Bitmap(
        pixelSize,
        pixelSize,
        pixelSize * 4,
        PixelFormat32bppPARGB,
        slot.bits);
    return slot.bitmap && slot.bitmap->GetLastStatus() == Gdiplus::Ok;
}

void ClickFeedback::DestroySlot(Slot& slot)
{
    if (slot.bitmap)
    {
        delete slot.bitmap;
        slot.bitmap = nullptr;
    }
    if (slot.hwnd)
    {
        DestroyWindow(slot.hwnd);
        slot.hwnd = nullptr;
    }
    if (slot.dc)
    {
        if (slot.oldBitmap)
            SelectObject(slot.dc, slot.oldBitmap);
        DeleteDC(slot.dc);
        slot.dc = nullptr;
    }
    if (slot.dib)
    {
        DeleteObject(slot.dib);
        slot.dib = nullptr;
    }
    slot.oldBitmap = nullptr;
    slot.bits = nullptr;
    slot.active = false;
}

void ClickFeedback::OnClick(int button, POINT point)
{
    if (!context_)
        return;

    const bool ripple = context_->clickRippleEnabled.load();
    const bool sound = context_->clickSoundEnabled.load();
    if (!ripple && !sound)
        return;

    if (sound)
        PlayClickSound(context_->clickSoundPreset.load(), button == WM_RBUTTONDOWN);

    if (!ripple || slots_.empty())
        return;

    Slot* chosen = nullptr;
    for (Slot& slot : slots_)
    {
        if (!slot.active)
        {
            chosen = &slot;
            break;
        }
    }
    if (!chosen)
    {
        chosen = &slots_[0];
        for (Slot& slot : slots_)
        {
            if (slot.startedAt < chosen->startedAt)
                chosen = &slot;
        }
    }

    chosen->active = true;
    chosen->startedAt = GetTickCount64();
    chosen->center = point;
    chosen->button = button;
    RenderSlot(*chosen, chosen->startedAt);
    SetTimer(controller_, kAnimationTimer, 16, nullptr);
}

bool ClickFeedback::PlayMciFile(const std::wstring& path)
{
    if (path.empty())
        return false;

    if (mciPath_ != path)
    {
        CloseMci();
        std::wstring open = L"open \"";
        open += path;
        open += L"\" type waveaudio alias ZaYuCursorHaloClickSound";
        if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) != 0)
        {
            mciPath_.clear();
            return false;
        }
        mciPath_ = path;
    }

    mciSendStringW(L"seek ZaYuCursorHaloClickSound to start", nullptr, 0, nullptr);
    if (mciSendStringW(L"play ZaYuCursorHaloClickSound", nullptr, 0, nullptr) != 0)
    {
        CloseMci();
        return false;
    }
    return true;
}

void ClickFeedback::PlayClickSound(int preset, bool right)
{
    static const UINT leftIds[] = {
        IDR_CLICK_LEFT,
        IDR_CLICK_SOFT_LEFT,
        IDR_CLICK_CRISP_LEFT,
        IDR_CLICK_WOOD_LEFT,
        IDR_CLICK_SYNTH_LEFT,
    };
    static const UINT rightIds[] = {
        IDR_CLICK_RIGHT,
        IDR_CLICK_SOFT_RIGHT,
        IDR_CLICK_CRISP_RIGHT,
        IDR_CLICK_WOOD_RIGHT,
        IDR_CLICK_SYNTH_RIGHT,
    };

    const int volume = context_
        ? (std::max)(0, (std::min)(100, context_->clickVolumePercent.load()))
        : 100;
    if (volume <= 0)
        return;

    if (preset == static_cast<int>(ClickSoundPreset::Custom) && context_)
    {
        std::wstring path;
        {
            std::lock_guard<std::mutex> lock(context_->clickSoundMutex);
            path = context_->clickSoundFile;
        }
        if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            PlayCustomSound(path, volume);
            return;
        }
        preset = static_cast<int>(ClickSoundPreset::Default);
    }

    const int index = (std::max)(0, (std::min)(4, preset));
    if (volume < 100)
    {
        const UINT resourceId = right ? rightIds[index] : leftIds[index];
        const std::vector<BYTE> bytes = ReadResourceBytes(
            context_->instance,
            resourceId);
        if (PlayWavAtVolume(bytes, volume, context_->instance, resourceId, L""))
            return;
    }

    PlaySoundW(
        MAKEINTRESOURCEW(right ? rightIds[index] : leftIds[index]),
        context_->instance,
        SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
}

void ClickFeedback::PlayCustomSound(const std::wstring& path, int volume)
{
    if (volume < 100)
    {
        const std::vector<BYTE> bytes = ReadFileBytes(path);
        if (PlayWavAtVolume(bytes, volume, nullptr, 0, path))
            return;
    }
    else if (PlaySoundW(
                 path.c_str(),
                 nullptr,
                 SND_FILENAME | SND_ASYNC | SND_NODEFAULT))
    {
        return;
    }

    // MCI's waveaudio driver is a useful Win7-native fallback for WAV
    // encodings PlaySound rejects. It has no reliable per-play volume, so
    // this path is only used when PCM scaling was not possible.
    if (PlayMciFile(path))
        return;

    PlaySoundW(
        path.c_str(),
        nullptr,
        SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void ClickFeedback::CloseMci()
{
    if (mciPath_.empty())
        return;
    mciSendStringW(L"close ZaYuCursorHaloClickSound", nullptr, 0, nullptr);
    mciPath_.clear();
}

void ClickFeedback::OnTick()
{
    const ULONGLONG now = GetTickCount64();
    bool anyActive = false;
    for (Slot& slot : slots_)
    {
        if (!slot.active)
            continue;
        if (now - slot.startedAt >= kRippleLifetimeMs)
        {
            HideSlot(slot);
            continue;
        }
        anyActive = true;
        RenderSlot(slot, now);
    }
    if (!anyActive)
        KillTimer(controller_, kAnimationTimer);
}

void ClickFeedback::RenderSlot(Slot& slot, ULONGLONG now)
{
    if (!slot.bitmap || !slot.hwnd)
        return;

    const double elapsed = static_cast<double>(now - slot.startedAt);
    const float progress = static_cast<float>(
        (std::max)(0.0, (std::min)(1.0, elapsed / static_cast<double>(kRippleLifetimeMs))));
    const float fade = std::pow(1.0f - progress, 1.25f);

    unsigned rgb = context_ ? context_->clickLeftRgb.load() : 0x5B8DFF;
    if (slot.button == WM_RBUTTONDOWN)
        rgb = context_ ? context_->clickRightRgb.load() : 0xFF6B6B;
    else if (slot.button == WM_MBUTTONDOWN)
        rgb = 0x58D6A8;

    Gdiplus::Graphics graphics(slot.bitmap);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    const float scale = slot.scale;
    const float center = static_cast<float>(slot.size) / 2.0f;
    const float radius = (10.0f + 46.0f * progress) * scale;
    const float glowWidth = (10.0f - 6.0f * progress) * scale;
    const float mainWidth = (1.0f + 3.4f * (1.0f - progress)) * scale;

    Gdiplus::Pen glow(AccentColor(rgb, 0.10f * fade), glowWidth);
    graphics.DrawEllipse(&glow, center - radius, center - radius, radius * 2.0f, radius * 2.0f);

    Gdiplus::Pen main(AccentColor(rgb, 0.95f * fade), mainWidth);
    graphics.DrawEllipse(&main, center - radius, center - radius, radius * 2.0f, radius * 2.0f);

    if (progress < 0.55f)
    {
        const float dotAlpha = 0.55f * (1.0f - progress / 0.55f);
        const Gdiplus::Color mixed = MixedColor(rgb, 0xFFFFFF, 0.35f);
        Gdiplus::Color dotColor(
            static_cast<BYTE>(dotAlpha * 255.0f),
            mixed.GetR(),
            mixed.GetG(),
            mixed.GetB());
        Gdiplus::SolidBrush dot(dotColor);
        const float dotRadius = (3.2f + 4.0f * progress) * scale;
        graphics.FillEllipse(
            &dot,
            center - dotRadius,
            center - dotRadius,
            dotRadius * 2.0f,
            dotRadius * 2.0f);
    }
    graphics.Flush();

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT destination = {
        slot.center.x - slot.size / 2,
        slot.center.y - slot.size / 2,
    };
    POINT source = {0, 0};
    SIZE size = {slot.size, slot.size};
    UpdateLayeredWindow(
        slot.hwnd,
        nullptr,
        &destination,
        &size,
        slot.dc,
        &source,
        0,
        &blend,
        ULW_ALPHA);
    SetWindowPos(
        slot.hwnd,
        HWND_TOPMOST,
        destination.x,
        destination.y,
        0,
        0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ClickFeedback::HideSlot(Slot& slot)
{
    if (slot.hwnd)
        ShowWindow(slot.hwnd, SW_HIDE);
    slot.active = false;
}

LRESULT CALLBACK ClickFeedback::ControllerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case kClickMessage:
    {
        // The controller is owned by this thread; recover the instance from
        // the thread-local pointer stored at creation time.
        ClickFeedback* self = reinterpret_cast<ClickFeedback*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
            self->OnClick(static_cast<int>(wParam), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    }
    case kPreviewMessage:
    {
        ClickFeedback* self = reinterpret_cast<ClickFeedback*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && self->context_)
            self->PlayClickSound(
                self->context_->clickSoundPreset.load(),
                wParam != 0);
        return 0;
    }
    case WM_TIMER:
        if (wParam == kAnimationTimer)
        {
            ClickFeedback* self = reinterpret_cast<ClickFeedback*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (self)
                self->OnTick();
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK ClickFeedback::RippleProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

UINT ClickFeedback::SystemDpi()
{
    HDC dc = GetDC(nullptr);
    if (!dc)
        return 96;
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi >= 72 && dpi <= 480 ? static_cast<UINT>(dpi) : 96;
}
