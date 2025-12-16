#pragma once
#include <windows.h>

class SystemControl {
private:
    // Hàm n?i b?: Xin c?p quy?n SE_SHUTDOWN_NAME
    // Ch? s? d?ng trong file .cpp, không c?n public ra ngoài
    static bool GrantShutdownPrivilege();

public:
    // Các hàm ch?c n?ng chính
    static bool Shutdown();
    static bool Restart();
    static bool Lock();
};