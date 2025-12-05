// Controller.cpp
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "Common.h"
#include "Client.h"

#pragma comment(lib, "Ws2_32.lib")

std::string Client::ServiceDiscovery() {

    // 1. Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return "";
    }
    std::cout << "Controller: Successfully initialize Winsock.\n";

    // 2. Tạo Socket UDP
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Create Socket Failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return "";
    }

    // 3. (Quan trọng) Cài đặt thời gian timeout cho việc nhận
    // Nếu không có máy nào phản hồi sau 3 giây, recvfrom sẽ ngừng chờ
    DWORD timeout = 5000; // 5 seconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    // 4. Chuẩn bị địa chỉ Multicast để GỬI tin nhắn
    sockaddr_in multicastAddr;
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(DISCOVERY_PORT);
    multicastAddr.sin_addr.s_addr = inet_addr(MULTICAST_IP);

    // 5. Gửi tin nhắn "phát hiện" đến nhóm multicast
    std::cout << "Controller: Sending a discovery message...\n";
    if (sendto(sock, DISCOVERY_MSG_REQ, (int)strlen(DISCOVERY_MSG_REQ), 0, (sockaddr*)&multicastAddr, sizeof(multicastAddr)) == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return "";
    }

    // 6. Vòng lặp nhận các phản hồi
    char recvBuf[2048];
    sockaddr_in hostAddr; // Địa chỉ của Host phản hồi
    int hostAddrLen = sizeof(hostAddr);

    std::cout << "Controller: Waiting for response...\n";
    std::string foundIP;
    while (true)
    {
        // Chờ nhận phản hồi...
        int bytesIn = recvfrom(sock, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&hostAddr, &hostAddrLen);

        if (bytesIn == SOCKET_ERROR) {
            // Kiểm tra xem có phải lỗi do timeout không
            if (WSAGetLastError() == WSAETIMEDOUT) {
                std::cout << "Controller: Het thoi gian cho. Ngung tim kiem.\n";
                break; // Hết thời gian, thoát vòng lặp
            }
            std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
            break;
        }

        recvBuf[bytesIn] = '\0';

        // 7. Kiểm tra xem có đúng là tin nhắn "phản hồi" không
        if (strcmp(recvBuf, DISCOVERY_MSG_RES) == 0)
        {
            // Đã tìm thấy một Host!
            std::cout << "\n*** DA TIM THAY HOST! ***\n";
            std::cout << "    Dia chi IP: " << inet_ntoa(hostAddr.sin_addr) << "\n";
            std::cout << "    Port: " << ntohs(hostAddr.sin_port) << "\n\n";
            foundIP = inet_ntoa(hostAddr.sin_addr);
            break;
        }
    }

    // 8. Dọn dẹp
    closesocket(sock);
    WSACleanup();

    return foundIP;
};
