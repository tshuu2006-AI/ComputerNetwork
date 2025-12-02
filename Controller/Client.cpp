//// Controller.cpp
//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <iostream>
//#include <string>
//#include "Common.h"
//#include "Client.h"
//
//#pragma comment(lib, "Ws2_32.lib")
//
//void Client::ServiceDiscovery() {
//
//    // 1. Initialize Winsock
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        std::cerr << "WSAStartup failed.\n";
//        return;
//    }
//    std::cout << "Controller: Successfully initialize Winsock.\n";
//
//    // 2. Tạo Socket UDP
//    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//    if (sock == INVALID_SOCKET) {
//        std::cerr << "Create Socket Failed: " << WSAGetLastError() << "\n";
//        WSACleanup();
//        return;
//    }
//
//    // 3. (Quan trọng) Cài đặt thời gian timeout cho việc nhận
//    // Nếu không có máy nào phản hồi sau 3 giây, recvfrom sẽ ngừng chờ
//    DWORD timeout = 5000; // 5 seconds
//    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
//
//    // 4. Chuẩn bị địa chỉ Multicast để GỬI tin nhắn
//    sockaddr_in multicastAddr;
//    multicastAddr.sin_family = AF_INET;
//    multicastAddr.sin_port = htons(DISCOVERY_PORT);
//    multicastAddr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
//
//    // 5. Gửi tin nhắn "phát hiện" đến nhóm multicast
//    std::cout << "Controller: Sending a discovery message...\n";
//    if (sendto(sock, DISCOVERY_MSG_REQ, (int)strlen(DISCOVERY_MSG_REQ), 0, (sockaddr*)&multicastAddr, sizeof(multicastAddr)) == SOCKET_ERROR) {
//        std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
//        closesocket(sock);
//        WSACleanup();
//        return;
//    }
//
//    // 6. Vòng lặp nhận các phản hồi
//    char recvBuf[2048];
//    sockaddr_in hostAddr; // Địa chỉ của Host phản hồi
//    int hostAddrLen = sizeof(hostAddr);
//
//    std::cout << "Controller: Waiting for response...\n";
//
//    while (true)
//    {
//        // Chờ nhận phản hồi...
//        int bytesIn = recvfrom(sock, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&hostAddr, &hostAddrLen);
//
//        if (bytesIn == SOCKET_ERROR) {
//            // Kiểm tra xem có phải lỗi do timeout không
//            if (WSAGetLastError() == WSAETIMEDOUT) {
//                std::cout << "Controller: Het thoi gian cho. Ngung tim kiem.\n";
//                break; // Hết thời gian, thoát vòng lặp
//            }
//            std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
//            break;
//        }
//
//        recvBuf[bytesIn] = '\0';
//
//        // 7. Kiểm tra xem có đúng là tin nhắn "phản hồi" không
//        if (strcmp(recvBuf, DISCOVERY_MSG_RES) == 0)
//        {
//            // Đã tìm thấy một Host!
//            std::cout << "\n*** DA TIM THAY HOST! ***\n";
//            std::cout << "    Dia chi IP: " << inet_ntoa(hostAddr.sin_addr) << "\n";
//            std::cout << "    Port: " << ntohs(hostAddr.sin_port) << "\n\n";
//
//            // Lưu địa chỉ IP này vào danh sách của bạn
//            // (Trong ví dụ này, chúng ta chỉ in ra)
//        }
//    }
//
//    // 8. Dọn dẹp
//    closesocket(sock);
//    WSACleanup();
//
//    std::cout << "Press Enter to exit...";
//    std::cin.get();
//    return;
//}

#define _WINSOCK_DEPRECATED_NO_WARNINGS

// --- 1. NETWORK (BẮT BUỘC PHẢI ĐỨNG ĐẦU TIÊN) ---
// Phải include winsock2 trước windows.h để tránh xung đột
#include <winsock2.h>
#include <ws2tcpip.h>

// --- 2. WINDOWS & GDI+ ---
#include <windows.h>
#include <objidl.h>   // Cần cho IStream (Sửa lỗi undeclared identifier IStream)
#include <gdiplus.h>

// --- 3. THƯ VIỆN CHUẨN ---
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

// --- 4. FILE CỦA BẠN ---
#include "Common.h" 

// --- 5. LINK THƯ VIỆN ---
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
// #pragma comment(lib, "Ole32.lib") // Thường windows.h đã có, nhưng nếu lỗi link thì bỏ comment dòng này

using namespace Gdiplus;

// --- BIẾN TOÀN CỤC ĐỂ LƯU TRỮ ẢNH ---
std::vector<char> g_currentImageBuffer;
std::mutex g_imageMutex;
HWND g_hWindow = NULL;
bool g_isConnected = false;

// --- HÀM TÌM KIẾM SERVER (SERVICE DISCOVERY) ---
// Trả về IP của Server nếu tìm thấy, rỗng nếu không thấy
std::string FindServerIP() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return "";

    DWORD timeout = 3000; // Chờ 3 giây
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    sockaddr_in multicastAddr;
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(DISCOVERY_PORT);
    multicastAddr.sin_addr.s_addr = inet_addr(MULTICAST_IP);

    // Gửi tin nhắn tìm kiếm
    sendto(sock, DISCOVERY_MSG_REQ, strlen(DISCOVERY_MSG_REQ), 0, (sockaddr*)&multicastAddr, sizeof(multicastAddr));

    char recvBuf[1024];
    sockaddr_in hostAddr;
    int hostLen = sizeof(hostAddr);

    std::cout << "Dang tim Server..." << std::endl;
    int bytes = recvfrom(sock, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&hostAddr, &hostLen);

    closesocket(sock);

    if (bytes > 0) {
        recvBuf[bytes] = '\0';
        if (strcmp(recvBuf, DISCOVERY_MSG_RES) == 0) {
            return inet_ntoa(hostAddr.sin_addr);
        }
    }
    return "";
}

// --- CLASS WEBSOCKET CLIENT ---
class RemoteClient {
    SOCKET sock;
public:
    RemoteClient() : sock(INVALID_SOCKET) {}

    bool Connect(const std::string& ip, int port) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return false;

        // --- HANDSHAKE ---
        // Client phải gửi Key ngẫu nhiên (Base64 giả lập ở đây cho đơn giản)
        std::string req =
            "GET / HTTP/1.1\r\n"
            "Host: " + ip + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";

        send(sock, req.c_str(), req.length(), 0);

        char buf[1024];
        if (recv(sock, buf, 1024, 0) > 0) {
            if (std::string(buf).find("101 Switching Protocols") != std::string::npos) {
                g_isConnected = true;
                return true;
            }
        }
        return false;
    }

    // Gửi lệnh (BẮT BUỘC PHẢI MASK VÌ LÀ CLIENT)
    void SendCommand(const std::string& cmd) {
        if (!g_isConnected) return;
        std::vector<unsigned char> frame;
        frame.push_back(0x81); // FIN + Text
        frame.push_back(0x80 | (unsigned char)cmd.length()); // Mask bit = 1

        // Mask Key (4 byte ngẫu nhiên)
        unsigned char mask[4] = { 1, 2, 3, 4 };
        frame.insert(frame.end(), mask, mask + 4);

        for (size_t i = 0; i < cmd.length(); i++) {
            frame.push_back(cmd[i] ^ mask[i % 4]);
        }
        send(sock, (char*)frame.data(), frame.size(), 0);
    }

    void ReceiveLoop() {
        while (g_isConnected) {
            // Đọc Header (2 byte đầu)
            unsigned char header[2];
            if (recv(sock, (char*)header, 2, 0) <= 0) break;

            int opcode = header[0] & 0x0F;
            unsigned long long payloadLen = header[1] & 0x7F;

            // Xử lý độ dài mở rộng (nếu ảnh lớn)
            if (payloadLen == 126) {
                unsigned char exLen[2];
                recv(sock, (char*)exLen, 2, 0);
                payloadLen = (exLen[0] << 8) | exLen[1];
            }
            else if (payloadLen == 127) {
                unsigned char exLen[8];
                recv(sock, (char*)exLen, 8, 0);

                // SỬA: Tính toán lại độ dài gói tin 64-bit chuẩn Big-Endian
                payloadLen = 0;
                for (int i = 0; i < 8; i++) {
                    payloadLen = (payloadLen << 8) | exLen[i];
                }
            }

            // Đọc dữ liệu ảnh
            std::vector<char> data(payloadLen);
            size_t received = 0;
            while (received < payloadLen) {
                int r = recv(sock, data.data() + received, payloadLen - received, 0);
                if (r <= 0) break;
                received += r;
            }

            // Nếu là Binary Frame (0x02) -> Là ảnh JPEG
            if (opcode == 0x02) {
                {
                    std::lock_guard<std::mutex> lock(g_imageMutex);
                    g_currentImageBuffer = data;
                }
                // Báo cho cửa sổ vẽ lại
                InvalidateRect(g_hWindow, NULL, FALSE);

                // Gửi lệnh tiếp theo ngay lập tức để lấy ảnh mới (Loop)
                SendCommand("SCREENSHOT");
            }
        }
        g_isConnected = false;
    }
};

// --- HÀM VẼ GIAO DIỆN ---
void OnPaint(HDC hdc) {
    std::lock_guard<std::mutex> lock(g_imageMutex);
    if (g_currentImageBuffer.empty()) return;

    // Tạo luồng nhớ (Stream) từ buffer
    IStream* pStream = NULL;
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, g_currentImageBuffer.size());
    if (hGlobal) {
        void* pData = GlobalLock(hGlobal);
        memcpy(pData, g_currentImageBuffer.data(), g_currentImageBuffer.size());
        GlobalUnlock(hGlobal);
        CreateStreamOnHGlobal(hGlobal, TRUE, &pStream);

        if (pStream) {
            Graphics graphics(hdc);
            Image image(pStream);

            // Vẽ full màn hình client
            RECT rect;
            GetClientRect(g_hWindow, &rect);
            graphics.DrawImage(&image, 0, 0, rect.right, rect.bottom);

            pStream->Release();
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        OnPaint(hdc);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_ERASEBKGND: return 1; // Chống nháy
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- MAIN ---
int main() {
    // 1. Khởi tạo GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 2. Tìm Server
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    std::string serverIP = FindServerIP();

    if (serverIP.empty()) {
        std::cout << "Khong tim thay Server!" << std::endl;
        // Fallback về localhost để test nếu không tìm thấy
        serverIP = "127.0.0.1";
        std::cout << "Thu ket noi Localhost..." << std::endl;
    }
    else {
        std::cout << "Thay Server tai: " << serverIP << std::endl;
    }

    // 3. Tạo cửa sổ
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RemoteView";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    g_hWindow = CreateWindow(wc.lpszClassName, L"Remote Desktop Viewer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);

    // 4. Kết nối và chạy vòng lặp nhận ảnh
    RemoteClient client;
    std::thread networkThread([&]() {
        if (client.Connect(serverIP, 8080)) {
            std::cout << "Ket noi thanh cong! Dang lay du lieu..." << std::endl;

            // Kích hoạt chuỗi bằng lệnh đầu tiên
            client.SendCommand("SCREENSHOT");

            // Vào vòng lặp nhận
            client.ReceiveLoop();
        }
        else {
            std::cout << "Khong the ket noi WebSocket!" << std::endl;
        }
        });
    networkThread.detach();

    // 5. Message Loop của Windows
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}