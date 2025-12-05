#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#include "GlobalState.h"
#include "WebSocketClient.h"
#include "Client.h"
#include "AppWindow.h"
#include "Common.h"

#pragma comment(lib, "Ws2_32.lib")
// KHÔNG CẦN gdiplus.lib nữa

int main() {
    // 1. Khởi tạo Winsock cho UDP Discovery
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // 2. Tìm Server IP
    Client discoveryClient;
    std::string serverIP = discoveryClient.ServiceDiscovery();

    if (serverIP.empty()) {
        std::cout << "Khong tim thay Server! Fallback ve localhost." << std::endl;
        serverIP = "127.0.0.1";
    }
    else {
        std::cout << "Tim thay Server tai: " << serverIP << std::endl;
    }

    // 3. Khởi tạo GlobalState (Dùng chung cho cả 2 luồng)
    GlobalState sharedState;

    // 4. Khởi tạo WebSocket Client (Luồng Mạng)
    WebSocketClient wsClient(&sharedState);

    std::thread networkThread([&]() {
        // Logic kết nối giữ nguyên như cũ
        if (wsClient.Connect(serverIP, 8080)) {
            std::cout << "Ket noi WebSocket thanh cong!" << std::endl;
            wsClient.SendCommand("SCREENSHOT");
            wsClient.ReceiveLoop();
        }
        else {
            std::cout << "Khong the ket noi WebSocket!" << std::endl;
        }
        });
    networkThread.detach(); // Chạy ngầm

    // 5. Khởi tạo Cửa sổ (Luồng Giao diện)
    AppWindow appWindow(&sharedState);
    appWindow.Initialize(TITLE, WIDTH, HEIGHT);

    // Chạy vòng lặp và truyền sharedState vào để nó lấy dữ liệu
    appWindow.RunMessageLoop();

    // 6. Kết thúc
    return 0;
}