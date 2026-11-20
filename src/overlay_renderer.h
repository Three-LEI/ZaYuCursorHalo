#pragma once

#include <windows.h>

#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>

struct RenderResult
{
    bool presentOk = false;
    double drawMs = 0.0;
    double presentMs = 0.0;
};

class OverlayRenderer
{
public:
    OverlayRenderer() = default;
    ~OverlayRenderer();

    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    bool Initialize();
    void Shutdown();
    bool EnsureSurface(UINT width, UINT height);
    RenderResult Render(
        HWND hwnd,
        POINT cursor,
        float dpiScale,
        float radiusDip,
        unsigned accentRgb,
        int opacityPercent,
        int haloStyle);
    bool Move(HWND hwnd, POINT cursor, double& moveMs);
    bool HasSurface() const { return width_ != 0 && height_ != 0 && renderTarget_ != nullptr; }
    UINT width() const { return width_; }
    UINT height() const { return height_; }
    bool SavePng(const std::wstring& path) const;

private:
    void DiscardSurface();

    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory_;
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> renderTarget_;
    Microsoft::WRL::ComPtr<ID2D1GdiInteropRenderTarget> gdiInterop_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;

    UINT width_ = 0;
    UINT height_ = 0;
};
