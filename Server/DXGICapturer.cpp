#include "DXGICapturer.h"



void DXGICapturer::Release() {
    m_stagingTexture.Reset();
    m_duplication.Reset();
    m_context.Reset();
    m_device.Reset();
    m_initialized = false;
}

bool DXGICapturer::Initialize() {
    if (m_initialized) return true;
    HRESULT hr;

    // 1. Tạo Device
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
    if (FAILED(hr)) return false;

    // 2. Lấy Output Duplication
    ComPtr<IDXGIDevice> dxgiDevice;
    m_device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    ComPtr<IDXGIOutput> dxgiOutput;
    dxgiAdapter->EnumOutputs(0, &dxgiOutput); // Màn hình 0

    if (!dxgiOutput) return false;

    DXGI_OUTPUT_DESC outputDesc;
    dxgiOutput->GetDesc(&outputDesc);
    m_width = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
    m_height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;

    ComPtr<IDXGIOutput1> dxgiOutput1;
    dxgiOutput.As(&dxgiOutput1);
    hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_duplication);

    if (FAILED(hr)) return false;

    m_initialized = true;
    return true;
}

// Thay đổi: Trả về vector byte thay vì cv::Mat
// Output: Mảng pixel định dạng BGRA (Blue-Green-Red-Alpha)
bool DXGICapturer::CaptureFrame(std::vector<uint8_t>& outBuffer, int& outWidth, int& outHeight) {
    if (!m_initialized) {
        if (!Initialize()) return false;
    }

    HRESULT hr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> desktopResource;

    hr = m_duplication->AcquireNextFrame(100, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false; // Không có khung hình mới
    if (FAILED(hr)) { Release(); return false; }

    ComPtr<ID3D11Texture2D> gpuTexture;
    desktopResource.As(&gpuTexture);

    D3D11_TEXTURE2D_DESC desc;
    gpuTexture->GetDesc(&desc);

    // Tạo Staging Texture nếu cần
    if (!m_stagingTexture || desc.Width != m_width || desc.Height != m_height) {
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
        m_width = desc.Width;
        m_height = desc.Height;
    }

    m_context->CopyResource(m_stagingTexture.Get(), gpuTexture.Get());
    m_duplication->ReleaseFrame();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);

    if (SUCCEEDED(hr)) {
        outWidth = m_width;
        outHeight = m_height;

        // Cấp phát bộ nhớ: Width * Height * 4 bytes (BGRA)
        size_t dataSize = m_width * m_height * 4;
        if (outBuffer.size() != dataSize) outBuffer.resize(dataSize);

        // Copy từng dòng (xử lý Pitch/Stride)
        uint8_t* dest = outBuffer.data();
        uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);

        for (int h = 0; h < m_height; ++h) {
            memcpy(dest, src, m_width * 4);
            dest += m_width * 4;
            src += mappedResource.RowPitch;
        }

        m_context->Unmap(m_stagingTexture.Get(), 0);
        return true;
    }
}

