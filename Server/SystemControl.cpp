// SystemControl.cpp
#include "SystemControl.h"

// 1. Định nghĩa hàm xin quyền
bool SystemControl::GrantShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // Lấy token của tiến trình hiện tại
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    // Lấy LUID của quyền Shutdown
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

    // Thiết lập quyền
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Áp dụng quyền
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

    // Kiểm tra xem có lỗi không
    bool success = (GetLastError() == ERROR_SUCCESS);
    CloseHandle(hToken);
    return success;
}

// 2. Định nghĩa hàm Tắt máy
bool SystemControl::Shutdown() {
    if (!GrantShutdownPrivilege()) return false;

    return ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
}

// 3. Định nghĩa hàm Khởi động lại
bool SystemControl::Restart() {
    if (!GrantShutdownPrivilege()) return false;

    // EWX_REBOOT: Khởi động lại
    return ExitWindowsEx(EWX_REBOOT | EWX_FORCE,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
}

// 4. Định nghĩa hàm Khóa máy
bool SystemControl::Lock() {
    return LockWorkStation();
}