#pragma once
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex> // Thêm mutex để tránh xung đột
#include "DXGICapturer.h"

class Stream {
private:
    DXGICapturer m_dxgiCapturer;

    // Camera Variables
    std::atomic<bool> m_isCameraRunning;
    std::thread m_cameraThread;

    // Screen Stream Variables (MỚI)
    std::atomic<bool> m_isScreenRunning;
    std::thread m_screenThread;

    std::mutex m_captureMutex; // Khóa để tránh chụp ảnh và stream cùng lúc gây lỗi DXGI

public:
    Stream();
    ~Stream();

    // 1. Chụp 1 tấm (Screenshot)
    bool GetScreenShot(std::vector<uchar>& outJpegBuffer);

    // 2. Camera Stream
    void StartCameraStream(int deviceID, std::function<bool(const std::vector<uchar>&)> onFrameReady);
    void StopCameraStream();

    // 3. Screen Stream (MỚI)
    void StartScreenStream(std::function<bool(const std::vector<uchar>&)> onFrameReady);
    void StopScreenStream();
};