#include "screenshot.h"
#include <vector>
#include <windows.h>
#include <gdiplus.h> // Đã được khởi động ở main.cpp

#pragma comment(lib, "gdiplus.lib") 

using namespace Gdiplus;

// Hàm tìm CLSID cho encoder (ví dụ: "image/jpeg")
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

void capture_screen(const std::string& filename) {
    // Không cần GdiplusStartup/Shutdown ở đây nữa (đã chuyển ra main.cpp)

    // Sử dụng "Virtual Screen" để lấy TẤT CẢ các màn hình
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);
    
    // Copy từ (x, y) của màn hình ảo vào (0, 0) của bitmap
    BitBlt(hDC, 0, 0, width, height, hScreen, x, y, SRCCOPY);

    // Chuyển HBITMAP sang Gdiplus::Bitmap
    Bitmap bmp(hBitmap, NULL);
    CLSID clsid;
    GetEncoderClsid(L"image/jpeg", &clsid);
    
    std::wstring wname(filename.begin(), filename.end());
    bmp.Save(wname.c_str(), &clsid, NULL); // Lưu file

    // Dọn dẹp tài nguyên GDI
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
}