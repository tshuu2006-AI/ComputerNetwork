//libraries
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h> 
#include <ws2tcpip.h> 
#include <iostream>
#include "Common.h"
#include "Server.h"

#pragma comment(lib, "Ws2_32.lib") 

void Server::ServiceDiscovery() {

    // 1. Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return;
    }

    std::cout << "Host: Successfully initialize Winsock.\n";

    // 2. Create Socket UDP
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Create Socket Failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return ;
    }

    // 3. (Optional) Allow reusing the address
    BOOL bOptVal = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&bOptVal, sizeof(bOptVal));

    // 4. Bind the socket to the listening port
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(DISCOVERY_PORT);
    localAddr.sin_addr.s_addr = INADDR_ANY; // Listening to all the Network Interface Card.

    if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return ;
    }
    std::cout << "Host: Listening to port " << DISCOVERY_PORT << "...\n";

    // 5. Yêu cầu hệ điều hành tham gia nhóm Multicast
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP); // Địa chỉ nhóm
    mreq.imr_interface.s_addr = INADDR_ANY;             // Giao diện mạng

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
        std::cerr << "setsockopt IP_ADD_MEMBERSHIP failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return;
    }
    std::cout << "Host: Da tham gia nhom multicast " << MULTICAST_IP << "\n";

    // 6. Vòng lặp chính: Nhận và Phản hồi
    char recvBuf[2048];
    sockaddr_in controllerAddr; // Địa chỉ của Controller gửi tin
    int controllerAddrLen = sizeof(controllerAddr);

    while (true)
    {
        // Chờ nhận tin nhắn...
        int bytesIn = recvfrom(sock, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&controllerAddr, &controllerAddrLen);
        if (bytesIn == SOCKET_ERROR) {
            std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
            continue;
        }

        recvBuf[bytesIn] = '\0'; // Thêm ký tự kết thúc chuỗi
        std::cout << "Host: Received the message from " << inet_ntoa(controllerAddr.sin_addr) << "\n";

        // 7. Kiểm tra xem có đúng là tin nhắn "phát hiện" không
        if (strcmp(recvBuf, DISCOVERY_MSG_REQ) == 0)
        {
            std::cout << "Host: This is a discovery message. Sending a response...\n";

            // Gửi tin nhắn phản hồi TRỰC TIẾP về cho Controller
            sendto(sock, DISCOVERY_MSG_RES, (int)strlen(DISCOVERY_MSG_RES), 0, (sockaddr*)&controllerAddr, controllerAddrLen);
        }
    }

    // (Code sẽ không chạy đến đây trong ví dụ này)
    closesocket(sock); 
    WSACleanup();
    return;
}