#pragma once

#include <cstdint>
#include <vector>

#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// Direct2D (D3D11 デバイス + DXGI スワップチェーン) による画面表示
//
// CPU で作ったフレーム (BGRA 32bit) をウィンドウに合わせて拡大表示する。
// 拡大率が 1 倍以上なら整数倍・最近傍補間でドットをくっきり表示し、縦横比を保って中央に置く。
class Direct2DRenderer
{
public:
    Direct2DRenderer() = default;
    ~Direct2DRenderer();

    bool init(HWND hwnd);
    void shutdown();

    // ウィンドウサイズ変更時に呼ぶ
    void resize();

    // フレームを更新して表示する (pixels は width*height の BGRA)
    void present(const uint32_t* pixels, uint32_t width, uint32_t height);

    // 直前のフレームを再表示する (WM_PAINT など)
    void redraw();

private:
    bool createDevice();
    bool createTarget();
    void releaseDevice();
    void draw();

    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> frameBitmap_;
    uint32_t frameWidth_ = 0;
    uint32_t frameHeight_ = 0;
    std::vector<uint32_t> lastFrame_;
};
