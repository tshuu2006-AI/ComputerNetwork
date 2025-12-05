#include "Stream.h"
#include <vector>
#include <windows.h>
#include <d3d11.h>        // Thư viện DirectX 11
#include <dxgi1_2.h>      // Thư viện DXGI 1.2
#include <wrl/client.h>   // Smart Pointer (ComPtr)
#include <opencv2/opencv.hpp> // Bao gồm toàn bộ OpenCV cần thiết
#include <iostream>
#include <chrono>
#include <thread>

// Link thư viện DirectX
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

// ==========================================
// CLASS XỬ LÝ DXGI (GPU CAPTURE)
// ==========================================
class DXGICapturer {
private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIOutputDuplication> m_duplication;
    ComPtr<ID3D11Texture2D> m_stagingTexture; // Texture nằm ở RAM để CPU đọc được
    DXGI_OUTPUT_DESC m_outputDesc;
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

    // Khởi tạo DirectX Device và Output Duplication
    bool Initialize() {
        if (m_initialized) return true;

        HRESULT hr;

        // 1. Tạo D3D11 Device
        D3D_FEATURE_LEVEL featureLevel;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
        if (FAILED(hr)) { std::cerr << "Loi tao D3D11 Device" << std::endl; return false; }

        // 2. Lấy DXGI Device -> Adapter -> Output
        ComPtr<IDXGIDevice> dxgiDevice;
        m_device.As(&dxgiDevice);
        ComPtr<IDXGIAdapter> dxgiAdapter;
        dxgiDevice->GetAdapter(&dxgiAdapter);
        ComPtr<IDXGIOutput> dxgiOutput;
        // Lấy màn hình chính (Index 0). Nếu muốn màn phụ thì đổi số 0 thành 1
        dxgiAdapter->EnumOutputs(0, &dxgiOutput);

        if (!dxgiOutput) { std::cerr << "Khong tim thay man hinh!" << std::endl; return false; }

        dxgiOutput->GetDesc(&m_outputDesc);
        m_width = m_outputDesc.DesktopCoordinates.right - m_outputDesc.DesktopCoordinates.left;
        m_height = m_outputDesc.DesktopCoordinates.bottom - m_outputDesc.DesktopCoordinates.top;

        // 3. Tạo Output Duplication
        ComPtr<IDXGIOutput1> dxgiOutput1;
        dxgiOutput.As(&dxgiOutput1);
        hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_duplication);

        if (FAILED(hr)) {
            std::cerr << "Loi DuplicateOutput (Co the do chua cai Driver VGA hoac dang chay Fullscreen game doc quyen)" << std::endl;
            return false;
        }

        m_initialized = true;
        return true;
    }

    // Hàm chụp màn hình trả về cv::Mat (BGRA)
    bool CaptureFrame(cv::Mat& outputFrame) {
        if (!m_initialized) {
            if (!Initialize()) return false;
        }

        HRESULT hr;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        ComPtr<IDXGIResource> desktopResource;

        // 1. Lấy khung hình mới nhất (Timeout 100ms)
        // Lưu ý: Nếu màn hình KHÔNG CÓ GÌ THAY ĐỔI, hàm này sẽ chờ hết timeout rồi báo lỗi DXGI_ERROR_WAIT_TIMEOUT.
        // Điều này tốt để tiết kiệm CPU, nhưng cần xử lý logic nếu muốn stream liên tục.
        hr = m_duplication->AcquireNextFrame(100, &frameInfo, &desktopResource);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // Không có khung hình mới -> Trả về false hoặc giữ nguyên ảnh cũ
            return false;
        }
        if (FAILED(hr)) {
            // Có thể bị mất thiết bị (đổi độ phân giải, UAC...), cần Init lại
            Release();
            return false;
        }

        // 2. Lấy Texture từ GPU
        ComPtr<ID3D11Texture2D> gpuTexture;
        desktopResource.As(&gpuTexture);

        // 3. Tạo Staging Texture (nếu chưa có hoặc kích thước đổi)
        D3D11_TEXTURE2D_DESC desc;
        gpuTexture->GetDesc(&desc);

        if (!m_stagingTexture || desc.Width != m_width || desc.Height != m_height) {
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.BindFlags = 0;
            desc.MiscFlags = 0;
            m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
            m_width = desc.Width;
            m_height = desc.Height;
        }

        // 4. Copy từ GPU Texture sang RAM Texture
        m_context->CopyResource(m_stagingTexture.Get(), gpuTexture.Get());

        // 5. Giải phóng frame trên GPU để Windows tiếp tục vẽ frame mới
        m_duplication->ReleaseFrame();

        // 6. Map texture để đọc dữ liệu raw
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
        if (SUCCEEDED(hr)) {
            // Tạo cv::Mat từ dữ liệu raw
            // Format của DXGI thường là BGRA (tương thích tốt với OpenCV)
            if (outputFrame.empty() || outputFrame.size() != cv::Size(m_width, m_height)) {
                outputFrame.create(m_height, m_width, CV_8UC4);
            }

            // Copy từng dòng vì Pitch (độ rộng dòng trong bộ nhớ) có thể khác độ rộng ảnh thực tế
            uchar* dest = outputFrame.data;
            uchar* src = static_cast<uchar*>(mappedResource.pData);
            for (int h = 0; h < m_height; ++h) {
                memcpy(dest, src, m_width * 4); // 4 bytes per pixel (BGRA)
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
// HÀM CAPTURE SCREEN (Thay thế hàm cũ)
// ==========================================
void capture_screen(const std::string& filename) {
    // Dùng static để giữ kết nối DXGI qua các lần gọi hàm (tránh khởi tạo lại gây lag)
    static DXGICapturer capturer;

    cv::Mat frame;

    // Thử chụp màn hình
    // Vòng lặp nhỏ để đảm bảo lấy được hình nếu lần đầu bị timeout
    for (int i = 0; i < 5; i++) {
        if (capturer.CaptureFrame(frame)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!frame.empty()) {
        // Lưu ảnh bằng OpenCV (ngắn gọn hơn GDI+ rất nhiều)
        // DXGI trả về BGRA, OpenCV lưu tốt nhất là BGR hoặc BGRA
        cv::imwrite(filename, frame);
        // std::cout << "Saved: " << filename << std::endl;
    }
    else {
        std::cerr << "Loi: Khong chup duoc man hinh qua DXGI" << std::endl;
    }
}


// ==========================================
// PHẦN RECORD WEBCAM (GIỮ NGUYÊN)
// ==========================================
// Code record_webcam của bạn bên dưới vẫn dùng tốt, không cần sửa gì cả.
bool record_webcam(const std::string& output_filename, int duration_seconds) {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        std::cerr << "LOI: Khong the mo webcam mac dinh." << std::endl;
        return false;
    }

    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = 30.0;
    int delay_ms = static_cast<int>(1000 / fps);

    cv::VideoWriter writer(
        output_filename,
        cv::VideoWriter::fourcc('V', 'P', '8', '0'),
        fps,
        cv::Size(frame_width, frame_height)
    );

    if (!writer.isOpened()) {
        std::cerr << "LOI: Khong the tao file video output." << std::endl;
        return false;
    }

    int num_frames = static_cast<int>(duration_seconds * fps);
    for (int i = 0; i < num_frames; ++i) {
        auto start_time = std::chrono::steady_clock::now();
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) continue;
        writer.write(frame);
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
        int sleep_time = delay_ms - static_cast<int>(elapsed_ms);
        if (sleep_time > 0) std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }
    return true;
}