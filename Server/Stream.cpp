#include "Stream.h"
#include <iostream>

Stream::Stream() : m_isCameraRunning(false), m_isScreenRunning(false) {}

Stream::~Stream() {
    StopCameraStream();
    StopScreenStream();
}

// Chụp 1 tấm
bool Stream::GetScreenShot(std::vector<uchar>& outJpegBuffer) {
    std::lock_guard<std::mutex> lock(m_captureMutex); // Khóa an toàn

    std::vector<uint8_t> rawPixels;
    int width = 0, height = 0;

    for (int i = 0; i < 5; i++) {
        if (m_dxgiCapturer.CaptureFrame(rawPixels, width, height)) {
            cv::Mat img(height, width, CV_8UC4, rawPixels.data());
            std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 70 };
            cv::imencode(".jpg", img, outJpegBuffer, params);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// --- CAMERA STREAM (Giữ nguyên) ---
void Stream::StartCameraStream(int deviceID, std::function<bool(const std::vector<uchar>&)> onFrameReady) {
    if (m_isCameraRunning || m_isScreenRunning) return; // Chỉ cho phép chạy 1 loại stream tại 1 thời điểm

    m_isCameraRunning = true;
    m_cameraThread = std::thread([this, deviceID, onFrameReady]() {
        cv::VideoCapture cap(deviceID);
        if (!cap.isOpened()) {
            m_isCameraRunning = false; return;
        }

        cv::Mat frame;
        std::vector<uchar> buffer;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 50 }; // Giảm chất lượng chút cho mượt

        while (m_isCameraRunning) {
            cap >> frame;
            if (frame.empty()) break;

            // Resize nếu cần để giảm băng thông (ví dụ giảm 50%)
            // cv::resize(frame, frame, cv::Size(), 0.5, 0.5);

            cv::imencode(".jpg", frame, buffer, params);
            if (!onFrameReady(buffer)) break; // Gửi thất bại -> dừng

            std::this_thread::sleep_for(std::chrono::milliseconds(30)); // ~30 FPS
        }
        cap.release();
        m_isCameraRunning = false;
        });
    m_cameraThread.detach();
}

void Stream::StopCameraStream() {
    m_isCameraRunning = false;
}

// --- SCREEN STREAM (MỚI) ---
void Stream::StartScreenStream(std::function<bool(const std::vector<uchar>&)> onFrameReady) {
    if (m_isCameraRunning || m_isScreenRunning) return; // Không chạy đè lên nhau

    m_isScreenRunning = true;
    m_screenThread = std::thread([this, onFrameReady]() {
        std::cout << "--> Screen Stream Started.\n";

        std::vector<uint8_t> rawPixels;
        std::vector<uchar> buffer;
        int width = 0, height = 0;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 40 }; // Chất lượng 40 để truyền nhanh

        while (m_isScreenRunning) {
            {
                std::lock_guard<std::mutex> lock(m_captureMutex); // Khóa tài nguyên DXGI

                // Capture Frame
                if (m_dxgiCapturer.CaptureFrame(rawPixels, width, height)) {
                    // Convert sang OpenCV Mat
                    cv::Mat img(height, width, CV_8UC4, rawPixels.data());

                    // Resize xuống độ phân giải thấp hơn để stream mượt qua mạng (VD: 1280x720)
                    // Nếu mạng LAN nhanh thì có thể bỏ dòng này
                    cv::Mat resizedImg;
                    float scale = 0.6f; // Giảm xuống 60% kích thước gốc
                    cv::resize(img, resizedImg, cv::Size(), scale, scale);

                    // Nén JPEG
                    cv::imencode(".jpg", resizedImg, buffer, params);

                    // Gửi qua callback
                    if (!onFrameReady(buffer)) {
                        std::cout << "Client ngat ket noi Screen Stream.\n";
                        break;
                    }
                }
            }
            // Giới hạn FPS khoảng 15-20 FPS cho màn hình (đỡ lag máy)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        m_isScreenRunning = false;
        std::cout << "--> Screen Stream Stopped.\n";
        });
    m_screenThread.detach();
}

void Stream::StopScreenStream() {
    m_isScreenRunning = false;
}