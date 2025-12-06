#include "Stream.h" // File header của bạn
#include <vector>
#include <windows.h>
#include <d3d11.h>        // DirectX 11
#include <dxgi1_2.h>      // DXGI 1.2
#include <wrl/client.h>   // Smart Pointer
#include <opencv2/opencv.hpp>
#include <gdiplus.h>      // GDI+ để lưu ảnh
#include <iostream>
#include <thread>
#include <chrono>

// Link thư viện
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using Microsoft::WRL::ComPtr;

// ==========================================
// HÀM HỖ TRỢ GDI+ (TÌM MÃ ENCODER JPEG/PNG)
// ==========================================
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// ==========================================
// CLASS XỬ LÝ DXGI (THUẦN C++)
// ==========================================
class DXGICapturer {
private:
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

    void Release() {
        m_stagingTexture.Reset();
        m_duplication.Reset();
        m_context.Reset();
        m_device.Reset();
        m_initialized = false;
    }

    bool Initialize() {
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
    bool CaptureFrame(std::vector<uint8_t>& outBuffer, int& outWidth, int& outHeight) {
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
        return false;
    }
};

// ==========================================
// HÀM CAPTURE SCREEN (KHÔNG DÙNG OPENCV)
// ==========================================
void capture_screen(const std::string& filename) {
    // Đảm bảo GDI+ đã được start (thường đặt ở main, nhưng đặt đây để an toàn cho ví dụ)
    // Lưu ý: Nếu main() đã có GdiplusStartup rồi thì không cần gọi lại.
    /*
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    */

    static DXGICapturer capturer;
    std::vector<uint8_t> pixelData;
    int width = 0, height = 0;

    bool captured = false;
    for (int i = 0; i < 5; i++) {
        if (capturer.CaptureFrame(pixelData, width, height)) {
            captured = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (captured) {
        // Tạo Bitmap từ dữ liệu raw (Format DXGI trả về là BGRA, tương ứng PixelFormat32bppARGB trong GDI+)
        // Lưu ý: Stride = width * 4
        Bitmap bmp(width, height, width * 4, PixelFormat32bppARGB, pixelData.data());

        // Tìm Encoder cho file ảnh (ở đây ví dụ là JPEG)
        CLSID clsid;
        // Chuyển string filename sang wstring
        std::wstring wname(filename.begin(), filename.end());

        // Kiểm tra đuôi file để chọn format (đơn giản)
        if (filename.find(".png") != std::string::npos)
            GetEncoderClsid(L"image/png", &clsid);
        else
            GetEncoderClsid(L"image/jpeg", &clsid);

        bmp.Save(wname.c_str(), &clsid, NULL);
        // std::cout << "Saved via DXGI + GDI+: " << filename << std::endl;
    }
    else {
        std::cerr << "Loi: Khong chup duoc man hinh qua DXGI." << std::endl;
    }
}

// ==========================================
// PHẦN RECORD WEBCAM
// ==========================================
bool record_webcam(const std::string& output_filename, int duration_seconds) {
    // 1. Mở Webcam (0 là camera mặc định)
    // CAP_DSHOW: Giúp mở camera nhanh hơn trên Windows (tránh bị delay lúc khởi động)
    cv::VideoCapture cap(0, cv::CAP_DSHOW);

    if (!cap.isOpened()) {
        std::cerr << "[WEBCAM] Loi: Khong tim thay Webcam hoac Webcam dang ban." << std::endl;
        return false;
    }

    // 2. Lấy thông số từ Camera
    int width = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = 30.0; // Giả định quay 30 FPS

    // 3. Cấu hình file Video đầu ra
    cv::VideoWriter writer;

    // Chọn Codec nén video.
    // 'M','J','P','G' (.avi) là an toàn nhất, máy nào cũng chạy được mà không cần cài thêm DLL lạ.
    int codec = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');

    // Nếu bạn muốn file mp4 nhỏ gọn (nhưng cần openh264.dll), dùng: cv::VideoWriter::fourcc('H', '2', '6', '4')

    writer.open(output_filename, codec, fps, cv::Size(width, height), true);

    if (!writer.isOpened()) {
        std::cerr << "[WEBCAM] Loi: Khong the tao file video de ghi." << std::endl;
        cap.release();
        return false;
    }

    std::cout << "[WEBCAM] Dang ghi hinh trong " << duration_seconds << " giay..." << std::endl;

    // 4. Vòng lặp ghi hình
    int total_frames = duration_seconds * (int)fps;
    cv::Mat frame;

    for (int i = 0; i < total_frames; ++i) {
        // Đọc 1 khung hình từ cam
        cap >> frame;

        if (frame.empty()) {
            std::cerr << "[WEBCAM] Loi: Mat tin hieu." << std::endl;
            break;
        }

        // Ghi vào file
        writer.write(frame);

        // Ngủ 1 chút để giữ nhịp FPS (1000ms / 30fps ≈ 33ms)
        // Nếu không sleep, vòng lặp sẽ chạy quá nhanh gây ngốn CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // 5. Giải phóng tài nguyên
    cap.release();
    writer.release();
    std::cout << "[WEBCAM] Ghi hinh hoan tat: " << output_filename << std::endl;
    return true;
}


