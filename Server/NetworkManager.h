#pragma once
#include "WebSocketServer.h"
#include "ServiceDiscovery.h"
#include "GlobalState.h"
#include <thread>
#include <atomic>
#include <iostream>

class NetworkManager {
private:
    // Dữ liệu dùng chung
    GlobalState* global_state;

    // Các thành phần mạng
    WebSocketServer wsserver;
    ServiceDiscovery discovery;

    // Quản lý đa luồng
    std::thread clientThread;
    std::thread serverThread;
    std::thread discoveryThread; // <--- THÊM DÒNG NÀY (Luồng thứ 3)

    std::atomic<bool> isRunning;

public:
    // Constructor nhận con trỏ GlobalState
    NetworkManager(GlobalState* state);

    // Destructor (dọn dẹp thread)
    ~NetworkManager();

    // Hàm bắt đầu chạy mọi thứ
    void Start();

    // Hàm dừng mọi thứ an toàn
    void Stop();

private:

    // Vòng lặp xử lý của Server (Chạy ngầm)
    void RunServerLoop();
};