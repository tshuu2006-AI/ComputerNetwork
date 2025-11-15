#include "websocket_server.h"
#include <iostream>
#include <gdiplus.h> // Cần cho GDI+
#include <windows.h> // <-- THÊM VÀO cho SetProcessDPIAware

#pragma comment(lib, "gdiplus.lib") 
#pragma comment(lib, "user32.lib") // <-- THÊM VÀO cho SetProcessDPIAware

int main() {
    // --- BÁO CHO WINDOWS BIẾT APP NÀY "NHẬN THỨC DPI" ---
    // (Sửa lỗi chụp màn hình bị "cụt" trên màn hình có scale)
    SetProcessDPIAware();
    // ----------------------------------------------------

    // --- Khởi động GDI+ MỘT LẦN DUY NHẤT ---
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    // ------------------------------------

    std::cout << "🚀 Server dang khoi dong tai ws://127.0.0.1:8080" << std::endl;
    
    WebSocketServer server("127.0.0.1", 8080);
    
    server.start(); 

    // --- Tắt GDI+ MỘT LẦN DUY NHẤT ---
    Gdiplus::GdiplusShutdown(gdiplusToken);
    // ----------------------------------
    return 0;
}