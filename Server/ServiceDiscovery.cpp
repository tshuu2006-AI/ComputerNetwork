#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "Common.h"
#include "ServiceDiscovery.h"
#include <thread>
#include <chrono>
#pragma comment(lib, "Ws2_32.lib")

std::string ServiceDiscovery::getLocalHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        return "Unknown-Agent";
    }
    return std::string(hostname);
}


void ServiceDiscovery::ServerServiceDiscovery(std::atomic<bool>& isRunning) {
    std::cout << "[Discovery] Starting UDP Beacon (Sender)...\n";

    // 1. Tạo Socket UDP
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[Discovery] Socket creation failed\n";
        return;
    }

    // 2. Bật chế độ Broadcast
    BOOL broadcast = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast)) < 0) {
        std::cerr << "[Discovery] Failed to set broadcast option\n";
        closesocket(sock);
        return;
    }

    // 3. Cấu hình địa chỉ đích (255.255.255.255:9999)
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(BROADCAST_PORT);
    recvAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // 4. Chuẩn bị nội dung tin nhắn JSON
    // Format: {"hostname": "MyPC", "port": 8080}
    std::string myName = getLocalHostname();
    std::string jsonMsg = "{\"hostname\": \"" + myName + "\", \"port\": 8080}";

    std::cout << "[Discovery] Broadcasting: " << jsonMsg << "\n";

    while (isRunning) {
        int sentBytes = sendto(sock, jsonMsg.c_str(), (int)jsonMsg.length(), 0, (sockaddr*)&recvAddr, sizeof(recvAddr));

        if (sentBytes == SOCKET_ERROR) {}
        for (int i = 0; i < 30; i++) {
            if (!isRunning) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "[Discovery] Stopping UDP Beacon.\n";
    closesocket(sock);
}