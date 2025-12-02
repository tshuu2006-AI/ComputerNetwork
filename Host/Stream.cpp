#include "Stream.h"
#include <vector>
#include <windows.h>
#include <gdiplus.h> // Đã được khởi động ở main.cpp
#include <opencv2/videoio.hpp>  // Cần cho VideoCapture, VideoWriter
#include <opencv2/imgproc.hpp>  // Cần cho cv::Mat
#include <opencv2/highgui.hpp>  // Cần cho fourcc
#include <iostream>
#include <chrono>   // Cần cho thời gian
#include <thread>   // Cần cho sleep

#pragma comment(lib, "gdiplus.lib") 

using namespace Gdiplus;

// Hàm tìm CLSID cho encoder (ví dụ: "image/jpeg")
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

void capture_screen(const std::string& filename) {
    // Không cần GdiplusStartup/Shutdown ở đây nữa (đã chuyển ra main.cpp)

    // Sử dụng "Virtual Screen" để lấy TẤT CẢ các màn hình
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);

    // Copy từ (x, y) của màn hình ảo vào (0, 0) của bitmap
    BitBlt(hDC, 0, 0, width, height, hScreen, x, y, SRCCOPY);

    // Chuyển HBITMAP sang Gdiplus::Bitmap
    Bitmap bmp(hBitmap, NULL);
    CLSID clsid;
    GetEncoderClsid(L"image/jpeg", &clsid);

    std::wstring wname(filename.begin(), filename.end());
    bmp.Save(wname.c_str(), &clsid, NULL); // Lưu file

    // Dọn dẹp tài nguyên GDI
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
}


#include <opencv2/videoio.hpp>  // Cần cho VideoCapture, VideoWriter
#include <opencv2/imgproc.hpp>  // Cần cho cv::Mat
#include <opencv2/highgui.hpp>  // Cần cho fourcc
#include <iostream>
#include <chrono>   // Cần cho thời gian
#include <thread>   // Cần cho sleep

bool record_webcam(const std::string& output_filename, int duration_seconds) {
    // 1. Mở webcam mặc định (thiết bị 0)
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        std::cerr << "LOI: Khong the mo webcam mac dinh." << std::endl;
        return false;
    }

    // 2. Lấy thông số của webcam
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    // Đặt FPS cố định là 30.0
    double fps = 30.0;
    // -------------------------

    // Tính toán thời gian nghỉ giữa mỗi frame (ms) để đạt đúng FPS này
    // 1000ms / 30fps = ~33ms
    int delay_ms = static_cast<int>(1000 / fps);

    cv::Size frame_size(frame_width, frame_height);

    // 3. Tạo đối tượng VideoWriter
    //    Sử dụng codec VP80 cho file .webm
    cv::VideoWriter writer(
        output_filename,
        cv::VideoWriter::fourcc('V', 'P', '8', '0'),
        fps, // Báo cho file video là 30 FPS
        frame_size
    );

    if (!writer.isOpened()) {
        std::cerr << "LOI: Khong the tao file video output (kiem tra duoi file .webm)." << std::endl;
        cap.release();
        return false;
    }

    std::cout << "   ...Dang ghi webcam (FPS: " << fps << ", Thoi luong: " << duration_seconds << "s)..." << std::endl;

    // 4. Vòng lặp ghi hình (CÓ GIỮ NHỊP)
    //    Tự động tính 10 * 30 = 300 frames
    int num_frames = static_cast<int>(duration_seconds * fps);

    for (int i = 0; i < num_frames; ++i) {
        // Ghi lại thời điểm bắt đầu xử lý khung hình
        auto start_time = std::chrono::steady_clock::now();

        cv::Mat frame;
        cap >> frame; // Lấy 1 khung hình từ webcam

        if (frame.empty()) {
            std::cerr << "   ...LOI: Khung hinh trong tu webcam." << std::endl;
            continue;
        }

        writer.write(frame); // Ghi (nén) khung hình vào file

        // Tính toán thời gian đã trôi qua cho việc đọc và ghi
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Tính thời gian cần ngủ bù để giữ đúng nhịp 33ms
        int sleep_time = delay_ms - static_cast<int>(elapsed_ms);

        if (sleep_time > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
    }

    std::cout << "   ...Ghi webcam hoan tat." << std::endl;

    // 5. Dọn dẹp
    cap.release();
    writer.release();
    return true;
}