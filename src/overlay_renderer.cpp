#include "overlay_renderer.h"

#include "util.h"

#include <algorithm>
#include <cmath>

namespace
{
D2D1_COLOR_F Color(float r, float g, float b, float a)
{
    return D2D1::ColorF(r, g, b, a);
}
} // namespace

OverlayRenderer::~OverlayRenderer()
{
    Shutdown();
}

bool OverlayRenderer::Initialize()
{
    if (wicFactory_ || d2dFactory_)
        return true;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wicFactory_.GetAddressOf()));
    if (FAILED(hr))
        return false;

    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        d2dFactory_.GetAddressOf());
    if (FAILED(hr))
    {
        Shutdown();
        return false;
    }
    return true;
}

void OverlayRenderer::Shutdown()
{
    DiscardSurface();
    d2dFactory_.Reset();
    wicFactory_.Reset();
}

void OverlayRenderer::DiscardSurface()
{
    brush_.Reset();
    gdiInterop_.Reset();
    renderTarget_.Reset();
    bitmap_.Reset();
    width_ = 0;
    height_ = 0;
}

bool OverlayRenderer::EnsureSurface(UINT width, UINT height)
{
    if (!wicFactory_ || !d2dFactory_)
        return false;
    if (width == width_ && height == height_ && renderTarget_)
        return true;
    if (width == 0 || height == 0)
        return false;

    DiscardSurface();

    HRESULT hr = wicFactory_->CreateBitmap(
        width,
        height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        bitmap_.GetAddressOf());
    if (FAILED(hr))
        return false;

    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f,
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);

    hr = d2dFactory_->CreateWicBitmapRenderTarget(
        bitmap_.Get(),
        properties,
        renderTarget_.GetAddressOf());
    if (FAILED(hr))
    {
        DiscardSurface();
        return false;
    }

    hr = renderTarget_.As(&gdiInterop_);
    if (FAILED(hr))
    {
        DiscardSurface();
        return false;
    }

    hr = renderTarget_->CreateSolidColorBrush(
        Color(1.0f, 1.0f, 1.0f, 1.0f),
        brush_.GetAddressOf());
    if (FAILED(hr))
    {
        DiscardSurface();
        return false;
    }

    renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    width_ = width;
    height_ = height;
    return true;
}

RenderResult OverlayRenderer::Render(
    HWND hwnd,
    POINT cursor,
    float dpiScale,
    float radiusDip,
    unsigned accentRgb,
    int opacityPercent,
    int haloStyle)
{
    RenderResult result;
    if (!renderTarget_ || !gdiInterop_ || !brush_ || width_ == 0 || height_ == 0)
        return result;

    if (dpiScale < 0.5f)
        dpiScale = 1.0f;
    const float scale = dpiScale;
    const float radius = radiusDip * scale;
    const float center = static_cast<float>(width_) / 2.0f;

    const double drawStart = QpcNowSeconds();
    renderTarget_->BeginDraw();

    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(Color(0.0f, 0.0f, 0.0f, 0.0f));

    const float opacity = (std::max)(0.30f, (std::min)(1.0f, static_cast<float>(opacityPercent) / 100.0f));
    const float accentR = static_cast<float>((accentRgb >> 16) & 0xFF) / 255.0f;
    const float accentG = static_cast<float>((accentRgb >> 8) & 0xFF) / 255.0f;
    const float accentB = static_cast<float>(accentRgb & 0xFF) / 255.0f;
    auto drawRing = [&](float offsetDip, float thicknessDip, float alpha) {
        brush_->SetColor(Color(accentR, accentG, accentB, alpha * opacity));
        const float r = radius + offsetDip * scale;
        const float thickness = thicknessDip * scale;
        renderTarget_->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(center, center), r, r),
            brush_.Get(),
            thickness);
    };

    auto drawTick = [&](float x1, float y1, float x2, float y2, float thickness, float alpha) {
        brush_->SetColor(Color(accentR, accentG, accentB, alpha * opacity));
        renderTarget_->DrawLine(
            D2D1::Point2F(center + x1 * scale, center + y1 * scale),
            D2D1::Point2F(center + x2 * scale, center + y2 * scale),
            brush_.Get(),
            thickness * scale);
    };

    if (haloStyle == 3)
    {
        // Soft light spot with a thin outer edge.
        brush_->SetColor(Color(accentR, accentG, accentB, 0.10f * opacity));
        renderTarget_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(center, center), radius + 9.0f * scale, radius + 9.0f * scale),
            brush_.Get());
        drawRing(0.0f, 1.4f, 0.78f);
        brush_->SetColor(Color(accentR, accentG, accentB, 0.92f * opacity));
        renderTarget_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(center, center), 3.8f * scale, 3.8f * scale),
            brush_.Get());
        brush_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.62f * opacity));
        renderTarget_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(center, center), 1.5f * scale, 1.5f * scale),
            brush_.Get());
    }
    else
    {
        // Soft halo.
        drawRing(8.0f, 14.0f, 0.045f);
        drawRing(3.0f, 6.0f, 0.10f);

        // Dark edge for contrast on bright backgrounds.
        brush_->SetColor(Color(0.0f, 0.0f, 0.0f, 0.35f * opacity));
        const float darkRadius = radius + 1.2f * scale;
        renderTarget_->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(center, center), darkRadius, darkRadius),
            brush_.Get(),
            1.4f * scale);

        // Main accent ring.
        drawRing(0.0f, 2.0f, 0.95f);

        if (haloStyle == 1)
        {
            drawRing(-10.0f, 1.6f, 0.56f);
            drawRing(-17.0f, 1.0f, 0.22f);
        }
        else if (haloStyle == 2)
        {
            const float inner = radius / scale + 5.0f;
            const float outer = radius / scale + 16.0f;
            drawTick(0.0f, -inner, 0.0f, -outer, 2.2f, 0.82f);
            drawTick(0.0f, inner, 0.0f, outer, 2.2f, 0.82f);
            drawTick(-inner, 0.0f, -outer, 0.0f, 2.2f, 0.82f);
            drawTick(inner, 0.0f, outer, 0.0f, 2.2f, 0.82f);
            brush_->SetColor(Color(accentR, accentG, accentB, 0.9f * opacity));
            renderTarget_->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(center, center), 2.4f * scale, 2.4f * scale),
                brush_.Get());
        }

        // Small inner highlight for dark backgrounds.
        brush_->SetColor(Color(1.0f, 1.0f, 1.0f, 0.38f * opacity));
        const float innerRadius = radius - 2.4f * scale;
        if (innerRadius > 1.0f)
        {
            renderTarget_->DrawEllipse(
                D2D1::Ellipse(D2D1::Point2F(center, center), innerRadius, innerRadius),
                brush_.Get(),
                1.0f * scale);
        }
    }

    HDC sourceDc = nullptr;
    HRESULT dcHr = gdiInterop_->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &sourceDc);
    const double drawEnd = QpcNowSeconds();
    result.drawMs = (drawEnd - drawStart) * 1000.0;

    if (SUCCEEDED(dcHr) && sourceDc)
    {
        POINT destination = {
            cursor.x - static_cast<LONG>(width_ / 2),
            cursor.y - static_cast<LONG>(height_ / 2),
        };
        POINT source = {0, 0};
        SIZE size = {static_cast<LONG>(width_), static_cast<LONG>(height_)};
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        const double presentStart = QpcNowSeconds();
        result.presentOk = UpdateLayeredWindow(
            hwnd,
            nullptr,
            &destination,
            &size,
            sourceDc,
            &source,
            0,
            &blend,
            ULW_ALPHA) != FALSE;
        const double presentEnd = QpcNowSeconds();
        result.presentMs = (presentEnd - presentStart) * 1000.0;

        gdiInterop_->ReleaseDC(nullptr);
    }
    else
    {
        // Ensure the render target is still balanced if we could not get a DC.
    }

    const HRESULT endHr = renderTarget_->EndDraw();
    if (FAILED(endHr) || FAILED(dcHr))
    {
        result.presentOk = false;
        if (endHr == D2DERR_RECREATE_TARGET || FAILED(endHr))
            DiscardSurface();
    }
    return result;
}

bool OverlayRenderer::Move(HWND hwnd, POINT cursor, double& moveMs)
{
    moveMs = 0.0;
    if (!hwnd || width_ == 0 || height_ == 0)
        return false;
    const int x = cursor.x - static_cast<int>(width_ / 2);
    const int y = cursor.y - static_cast<int>(height_ / 2);

    const double start = QpcNowSeconds();
    const BOOL ok = SetWindowPos(
        hwnd,
        nullptr,
        x,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    const double end = QpcNowSeconds();
    moveMs = (end - start) * 1000.0;
    return ok != FALSE;
}

bool OverlayRenderer::SavePng(const std::wstring& path) const
{
    if (!wicFactory_ || !bitmap_ || width_ == 0 || height_ == 0)
        return false;
    if (!EnsureParentDirectoryExists(path))
        return false;

    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> properties;

    HRESULT hr = wicFactory_->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return false;
    hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(hr))
        return false;
    hr = wicFactory_->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
    if (FAILED(hr))
        return false;
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return false;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf());
    if (FAILED(hr))
        return false;
    hr = frame->Initialize(properties.Get());
    if (FAILED(hr))
        return false;
    hr = frame->SetSize(width_, height_);
    if (FAILED(hr))
        return false;

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr))
        return false;
    hr = frame->WriteSource(bitmap_.Get(), nullptr);
    if (FAILED(hr))
        return false;
    hr = frame->Commit();
    if (FAILED(hr))
        return false;
    return SUCCEEDED(encoder->Commit());
}
