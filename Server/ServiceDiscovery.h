#pragma once
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic> // Thêm thư viện atomic
#include "DataInfo.h"

#pragma comment(lib, "Ws2_32.lib")

// Định nghĩa cấu trúc ServerInfo nếu chưa có trong DataInfo.h

class ServiceDiscovery {
private:
    std::string getLocalHostname(); // Hàm helper mới

public:
    // Sửa hàm này nhận biến isRunning để có thể dừng vòng lặp
    void ServerServiceDiscovery(std::atomic<bool>& isRunning);
};
