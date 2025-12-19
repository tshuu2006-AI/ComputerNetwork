#include "NetworkManager.h"
#include "GlobalState.h"
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

// Hàm chính bắt đầu chương trình Agent
int main() {
    std::cout << "[MAIN] Starting Application...\n";

    // 1. KHỞI TẠO WINSOCK (BẮT BUỘC để tránh lỗi 10093)
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup error: Cannot initialize Winsock. Error code : " << result << "\n";
        return 1;
    }
    std::cout << "[MAIN] Winsock initialized successfully.\n";

    // 2. KHỞI TẠO TRẠNG THÁI CHUNG VÀ QUẢN LÝ MẠNG
    GlobalState globalState;
    NetworkManager networkManager(&globalState);

    // 3. BẮT ĐẦU CHẠY CÁC THREAD (Server/Client Loop)
    networkManager.Start();
    std::cout << "[MAIN] NetworkManager threads started.\n";

    // 4. VÒNG LẶP CHÍNH (Giữ cho chương trình không bị tắt)

    std::cout << "[MAIN] Agent is running. Press CTRL+C to stop.\n";

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 5. DỌN DẸP (Sẽ không bao giờ được gọi nếu không break vòng lặp while(true))
    networkManager.Stop();
    WSACleanup();

    std::cout << "[MAIN] Application shut down.\n";
    return 0;
}