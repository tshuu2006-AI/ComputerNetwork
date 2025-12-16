#pragma once
// Link thư viện
#pragma once
#include <vector>
#include <d3d11.h>        // DirectX 11
#include <dxgi1_2.h>      // DXGI 1.2
#include <wrl/client.h>   // Smart Pointer
#include <gdiplus.h>      // GDI+ để lưu ảnh


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using Microsoft::WRL::ComPtr;


class DXGICapturer {
protected:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIOutputDuplication> m_duplication;
    ComPtr<ID3D11Texture2D> m_stagingTexture;
    bool m_initialized = false;
    int m_width = 0;
    int m_height = 0;

public:
    DXGICapturer() {}
    ~DXGICapturer() { Release(); }
    void Release();
    bool Initialize();
    bool CaptureFrame(std::vector<uint8_t>& outBuffer, int& outWidth, int& outHeight);
};