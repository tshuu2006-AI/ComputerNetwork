#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "Common.h"
#include "ServiceDiscovery.h"
#include <thread>
#include <chrono>
#pragma comment(lib, "Ws2_32.lib")

//std::vector<std::string> GetLocalIPAddresses() {
//    std::vector<std::string> ips;
//    char hostname[256];
//
//    // 1. Lấy tên máy (Hostname)
//    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) return ips;
//
//    // 2. Từ tên máy, lấy thông tin IP
//    struct hostent* host = gethostbyname(hostname);
//    if (host == NULL) return ips;
//
//    // 3. Duyệt danh sách các IP tìm được
//    for (int i = 0; host->h_addr_list[i] != 0; ++i) {
//        struct in_addr addr;
//        memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
//        ips.push_back(inet_ntoa(addr));
//    }
//    return ips;
//}
//
//
//std::string ServiceDiscovery::getHostnameFromIP(struct in_addr ip_addr) {
//    // gethostbyaddr là hàm blocking (chặn luồng) và có thể mất vài giây nếu DNS chậm
//    struct hostent* host_entry = gethostbyaddr(
//        (const char*)&ip_addr,
//        sizeof(ip_addr),
//        AF_INET
//    );
//
//    if (host_entry == NULL) {
//        // Có thể thất bại nếu máy chủ DNS không biết tên của IP này
//        return "Unknown Hostname";
//    }
//
//    // host_entry->h_name chứa tên máy chủ
//    if (host_entry->h_name != NULL) {
//        return std::string(host_entry->h_name);
//    }
//
//    return "Unknown Hostname";
//}
//
//
//// [THAY ĐỔI 1] Đổi kiểu trả về thành vector<string>
//std::vector<ServerInfo> ServiceDiscovery::ClientServiceDiscovery() {
//    std::vector<ServerInfo> foundServers;
//    std::vector<std::string> myIPs = GetLocalIPAddresses();
//
//    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//    if (sock == INVALID_SOCKET) {
//        std::cerr << "Create Socket Failed: " << WSAGetLastError() << "\n";
//        WSACleanup();
//        return foundServers;
//    }
//
//    // [FIX] Giảm Timeout xuống 2000ms (2 giây) cho mượt
//    DWORD timeout = 2000;
//    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
//
//    sockaddr_in multicastAddr;
//    multicastAddr.sin_family = AF_INET;
//    multicastAddr.sin_port = htons(DISCOVERY_PORT);
//    multicastAddr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
//
//    // Cho phép tìm thấy chính mình (Loopback)
//    BOOL loop = TRUE;
//    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loop, sizeof(loop));
//
//    std::cout << "[Client] Broadcasting discovery request...\n";
//    if (sendto(sock, DISCOVERY_MSG_REQ, (int)strlen(DISCOVERY_MSG_REQ), 0, (sockaddr*)&multicastAddr, sizeof(multicastAddr)) == SOCKET_ERROR) {
//        std::cerr << "sendto failed\n";
//        closesocket(sock);
//        return foundServers;
//    }
//
//    char recvBuf[2048];
//    sockaddr_in hostAddr;
//    int hostAddrLen = sizeof(hostAddr);
//
//    while (true) {
//        int bytesIn = recvfrom(sock, recvBuf, sizeof(recvBuf) - 1, 0, (sockaddr*)&hostAddr, &hostAddrLen);
//        if (bytesIn == SOCKET_ERROR) break; // Timeout hoặc lỗi
//
//        recvBuf[bytesIn] = '\0';
//        std::string msg = std::string(recvBuf);
//
//        // Lấy IP người gửi
//        std::string senderIP = inet_ntoa(hostAddr.sin_addr);
//
//        // [BƯỚC 2] KIỂM TRA: CÓ PHẢI LÀ TÔI KHÔNG?
//        bool isMe = false;
//        for (const std::string& localIP : myIPs) {
//            if (senderIP == localIP) {
//                isMe = true;
//                break;
//            }
//        }
//
//        if (isMe) {
//            // std::cout << "[Scanner] Ignored loopback packet from myself (" << senderIP << ")\n";
//            continue;
//        }
//
//
//        bool isDuplicate = false;
//        for (const auto& server : foundServers) {
//            if (server.ip == senderIP) {
//                isDuplicate = true;
//                break;
//            }
//        }
//        if (isDuplicate) {
//            // std::cout << "Duplicate packet from " << senderIP << "\n";
//            continue; // Bỏ qua gói tin này
//        }
//
//
//        // [FIX LOGIC] Kiểm tra xem tin nhắn có BẮT ĐẦU bằng mã quy ước không
//        if (msg.find(DISCOVERY_MSG_RES) == 0) {
//            std::string hostname = getHostnameFromIP(hostAddr.sin_addr);
//
//            // [FIX CRITICAL] Tách port TCP từ tin nhắn "MSG:PORT"
//            int tcpPort = 8080; // Giá trị mặc định an toàn
//            size_t colonPos = msg.find(':');
//
//            if (colonPos != std::string::npos) {
//                try {
//                    // Lấy phần chuỗi sau dấu ":" và chuyển thành số
//                    tcpPort = std::stoi(msg.substr(colonPos + 1));
//                }
//                catch (...) {
//                    tcpPort = 8080;
//                }
//            }
//
//            std::cout << "[Client] Found: " << senderIP << " (" << hostname << ") Port: " << tcpPort << "\n";
//            foundServers.push_back({ senderIP, hostname, tcpPort, false });
//        }
//    }
//
//    closesocket(sock);
//    return foundServers;
//}

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

    // 5. Vòng lặp gửi tin (Beacon Loop)
    while (isRunning) {
        // Gửi gói tin
        int sentBytes = sendto(sock, jsonMsg.c_str(), (int)jsonMsg.length(), 0, (sockaddr*)&recvAddr, sizeof(recvAddr));

        if (sentBytes == SOCKET_ERROR) {
            // std::cerr << "[Discovery] Sendto failed: " << WSAGetLastError() << "\n";
            // Không break, thử lại sau
        }

        // Ngủ 3 giây trước khi gửi tiếp
        // Dùng vòng lặp nhỏ để check isRunning thường xuyên hơn (để dừng nhanh hơn)
        for (int i = 0; i < 30; i++) {
            if (!isRunning) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "[Discovery] Stopping UDP Beacon.\n";
    closesocket(sock);
}