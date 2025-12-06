#include "WebSocketServer.h"
#include "Server.h" // <--- 1. NHỚ INCLUDE FILE CHỨA ServiceDiscovery
#include <iostream>
#include <gdiplus.h>
#include <windows.h>
#include <thread> // <--- 2. Cần thư viện thread

#pragma comment(lib, "gdiplus.lib") 
#pragma comment(lib, "user32.lib")

int main() {
    // --- Setup màn hình ---
    SetProcessDPIAware();

    // --- Setup GDI+ ---
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // =================================================================
    // BƯỚC QUAN TRỌNG: CHẠY SERVICE DISCOVERY TRÊN LUỒNG RIÊNG
    // =================================================================
    // Lý do: Hàm ServiceDiscovery có vòng lặp while(true) để lắng nghe
    // tin nhắn multicast. Nếu chạy trực tiếp ở đây, code sẽ kẹt mãi mãi
    // và không bao giờ xuống được dòng server.start().

    std::thread discoveryThread([]() {
        Server discoveryHost; // Tạo đối tượng Server (lớp chứa hàm Discovery)
        discoveryHost.ServiceDiscovery(); // Gọi hàm lắng nghe
        });

    // detach() để luồng này chạy ngầm độc lập, không chặn luồng chính
    discoveryThread.detach();

    std::cout << "--> Da bat che do Service Discovery (UDP Multicast)..." << std::endl;


    WebSocketServer server("0.0.0.0", 8080);
    server.start(); // <-- Hàm này cũng thường sẽ chặn (blocking) tại đây

    // --- Dọn dẹp (Thực tế sẽ ít khi chạy tới đây nếu server.start() loop vô tận) ---
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}