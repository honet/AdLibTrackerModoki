#include "Direct2DRenderer.h"

#include <algorithm>
#include <cmath>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

Direct2DRenderer::~Direct2DRenderer()
{
    shutdown();
}

bool Direct2DRenderer::init(HWND hwnd)
{
    hwnd_ = hwnd;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf()))) {
        return false;
    }
    return createDevice();
}

void Direct2DRenderer::shutdown()
{
    releaseDevice();
    d2dFactory_.Reset();
}

bool Direct2DRenderer::createDevice()
{
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                   D3D11_SDK_VERSION, &d3dDevice_, nullptr, nullptr);
    if (FAILED(hr)) {
        // GPU が使えない環境ではソフトウェアラスタライザを使う
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                               D3D11_SDK_VERSION, &d3dDevice_, nullptr, nullptr);
        if (FAILED(hr)) return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(d3dDevice_.As(&dxgiDevice))) return false;
    if (FAILED(d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_))) return false;
    if (FAILED(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_))) return false;

    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> dxgiFactory;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) return false;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&dxgiFactory)))) return false;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (FAILED(dxgiFactory->CreateSwapChainForHwnd(d3dDevice_.Get(), hwnd_, &desc, nullptr, nullptr, &swapChain_))) {
        return false;
    }
    // Alt+Enter による全画面切り替えは使わない
    dxgiFactory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);

    frameBitmap_.Reset();
    return createTarget();
}

bool Direct2DRenderer::createTarget()
{
    ComPtr<IDXGISurface> surface;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;

    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    if (FAILED(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &props, &targetBitmap_))) return false;
    d2dContext_->SetTarget(targetBitmap_.Get());
    return true;
}

void Direct2DRenderer::releaseDevice()
{
    if (d2dContext_) d2dContext_->SetTarget(nullptr);
    frameBitmap_.Reset();
    targetBitmap_.Reset();
    d2dContext_.Reset();
    d2dDevice_.Reset();
    swapChain_.Reset();
    d3dDevice_.Reset();
}

void Direct2DRenderer::resize()
{
    if (!swapChain_) return;
    d2dContext_->SetTarget(nullptr);
    targetBitmap_.Reset();
    if (FAILED(swapChain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0)) || !createTarget()) {
        releaseDevice();
        createDevice();
    }
    draw();
}

void Direct2DRenderer::present(const uint32_t* pixels, uint32_t width, uint32_t height)
{
    lastFrame_.assign(pixels, pixels + static_cast<size_t>(width) * height);
    if (width != frameWidth_ || height != frameHeight_) {
        frameBitmap_.Reset();
        frameWidth_ = width;
        frameHeight_ = height;
    }
    draw();
}

void Direct2DRenderer::redraw()
{
    draw();
}

void Direct2DRenderer::draw()
{
    if (!d2dContext_ || !targetBitmap_ || lastFrame_.empty()) return;

    if (!frameBitmap_) {
        const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        if (FAILED(d2dContext_->CreateBitmap(D2D1::SizeU(frameWidth_, frameHeight_), nullptr, 0, &props, &frameBitmap_))) {
            return;
        }
    }
    frameBitmap_->CopyFromMemory(nullptr, lastFrame_.data(), frameWidth_ * 4);

    const D2D1_SIZE_F target = targetBitmap_->GetSize();
    float scale = std::min(target.width / frameWidth_, target.height / frameHeight_);
    D2D1_INTERPOLATION_MODE mode = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    if (scale >= 1.0f) {
        scale = std::floor(scale);
    } else {
        mode = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
    }
    const float w = frameWidth_ * scale;
    const float h = frameHeight_ * scale;
    const float x = std::floor((target.width - w) / 2);
    const float y = std::floor((target.height - h) / 2);

    d2dContext_->BeginDraw();
    d2dContext_->Clear(D2D1::ColorF(0, 0, 0));
    d2dContext_->DrawBitmap(frameBitmap_.Get(), D2D1::RectF(x, y, x + w, y + h), 1.0f, mode);
    HRESULT hr = d2dContext_->EndDraw();
    if (SUCCEEDED(hr)) {
        hr = swapChain_->Present(1, 0);
    }
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        releaseDevice();
        createDevice();
    }
}
