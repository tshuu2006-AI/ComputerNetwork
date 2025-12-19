#include "Stream.h"
#include <iostream>

Stream::Stream()
    : m_isCameraRunning(false),
    m_isScreenRunning(false),
    m_isCamRecording(false) {
}

Stream::~Stream() {
    StopCameraStream();
    StopScreenStream();
}

// ---------------- Screenshot ----------------
bool Stream::GetScreenShot(std::vector<uchar>& outJpegBuffer) {
    std::lock_guard<std::mutex> lock(m_captureMutex);

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

// ---------------- Camera Stream ----------------
bool Stream::StartCameraStream(
    int deviceID,
    std::function<bool(const std::vector<uchar>&)> onFrameReady
) {
    if (m_isCameraRunning || m_isScreenRunning)
        return false;

    m_isCameraRunning = true;

    m_cameraThread = std::thread([this, deviceID, onFrameReady]() {
        cv::VideoCapture cap;

        bool opened = false;
        for (int retry = 0; retry < 3; retry++) {
            if (cap.open(deviceID, cv::CAP_DSHOW)) {
                opened = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (!opened) {
            std::cout << "[ERROR] Camera device not responding.\n";
            m_isCameraRunning = false;
            return;
        }

        cap.set(cv::CAP_PROP_FPS, 30);

        cv::Mat frame;
        std::vector<uchar> buffer;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 50 };

        while (m_isCameraRunning) {
            if (!cap.read(frame) || frame.empty())
                break;

            // ---- RECORD FIX (CORE CHANGE) ----
            if (m_isCamRecording) {
                std::lock_guard<std::mutex> lock(m_recordMutex);

                if (!m_camVideoWriter.isOpened()) {
                    int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
                    cv::Size sz(frame.cols, frame.rows);
                    m_camVideoWriter.open(m_recordFilename, fourcc, 30, sz);
                }

                if (m_camVideoWriter.isOpened()) {
                    m_camVideoWriter.write(frame);
                }
            }
            // ---------------------------------

            cv::imencode(".jpg", frame, buffer, params);
            if (!onFrameReady(buffer))
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        cap.release();
        m_isCameraRunning = false;
        });

    return true;
}

void Stream::StopCameraStream() {
    if (!m_isCameraRunning) return;

    m_isCameraRunning = false;
    if (m_cameraThread.joinable())
        m_cameraThread.join();
}

// ---------------- Screen Stream (unchanged) ----------------
void Stream::StartScreenStream(std::function<bool(const std::vector<uchar>&)> onFrameReady) {
    if (m_isCameraRunning || m_isScreenRunning) return;

    m_isScreenRunning = true;
    m_screenThread = std::thread([this, onFrameReady]() {
        std::vector<uint8_t> rawPixels;
        std::vector<uchar> buffer;
        int width = 0, height = 0;
        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 40 };

        while (m_isScreenRunning) {
            {
                std::lock_guard<std::mutex> lock(m_captureMutex);
                if (m_dxgiCapturer.CaptureFrame(rawPixels, width, height)) {
                    cv::Mat img(height, width, CV_8UC4, rawPixels.data());
                    cv::Mat resizedImg;
                    cv::resize(img, resizedImg, cv::Size(), 0.6, 0.6);
                    cv::imencode(".jpg", resizedImg, buffer, params);
                    if (!onFrameReady(buffer)) break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        m_isScreenRunning = false;
        });
}

void Stream::StopScreenStream() {
    if (!m_isScreenRunning) return;

    m_isScreenRunning = false;
    if (m_screenThread.joinable())
        m_screenThread.join();
}

// ---------------- Recording API ----------------
bool Stream::StartCamRecording(const std::string& filename, int /*fps*/) {
    if (m_isCamRecording) return false;

    std::lock_guard<std::mutex> lock(m_recordMutex);
    m_recordFilename = filename;
    m_isCamRecording = true;
    return true;
}

void Stream::StopCamRecording() {
    std::lock_guard<std::mutex> lock(m_recordMutex);

    m_isCamRecording = false;
    if (m_camVideoWriter.isOpened()) {
        m_camVideoWriter.release();
    }
}
