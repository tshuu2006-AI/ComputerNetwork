#pragma once
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include "DXGICapturer.h"

class Stream {
private:
    DXGICapturer m_dxgiCapturer;

    // Camera
    std::atomic<bool> m_isCameraRunning;
    std::thread m_cameraThread;

    // Screen
    std::atomic<bool> m_isScreenRunning;
    std::thread m_screenThread;

    // Recording (FIXED)
    cv::VideoWriter m_camVideoWriter;
    std::atomic<bool> m_isCamRecording;
    std::string m_recordFilename;      // NEW: filename lưu video
    std::mutex m_recordMutex;           // NEW: bảo vệ VideoWriter

    // DXGI capture mutex
    std::mutex m_captureMutex;

public:
    Stream();
    ~Stream();

    // Screenshot
    bool GetScreenShot(std::vector<uchar>& outJpegBuffer);

    // Camera stream
    bool StartCameraStream(int deviceID,
        std::function<bool(const std::vector<uchar>&)> onFrameReady);
    void StopCameraStream();

    // Screen stream
    void StartScreenStream(std::function<bool(const std::vector<uchar>&)> onFrameReady);
    void StopScreenStream();

    // Camera recording
    bool StartCamRecording(const std::string& filename, int fps);
    void StopCamRecording();
};