#include "TaskManager.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip> // Cần cho std::hex trong EscapeJson

#pragma comment(lib, "shell32.lib")

// ==========================================
// CÁC HÀM HỖ TRỢ (UTF-8, JSON ESCAPE)
// ==========================================

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str_to[0], size_needed, NULL, NULL);
    return str_to;
}

std::wstring utf8_to_wstring(const std::string& utf8_str) {
    if (utf8_str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.size(), &wstr[0], size_needed);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

// Hàm xử lý ký tự đặc biệt để chuỗi hợp lệ trong JSON
std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (auto c : s) {
        switch (c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            // Loại bỏ các ký tự điều khiển (0x00 - 0x1F)
            if ((unsigned char)c < 0x20) {}
            else o << c;
        }
    }
    return o.str();
}

// ==========================================
// LOGIC LIST APPS -> TRẢ VỀ JSON
// ==========================================

struct WindowInfo {
    DWORD pid;
    std::string title;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    std::vector<WindowInfo>* pList = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    if (!IsWindowVisible(hwnd)) return TRUE;
    int length = GetWindowTextLengthW(hwnd);
    if (length == 0) return TRUE;

    std::vector<WCHAR> buffer(length + 1);
    GetWindowTextW(hwnd, &buffer[0], length + 1);
    std::wstring wsTitle(&buffer[0]);

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    if (wsTitle != L"Program Manager") {
        WindowInfo info;
        info.pid = pid;
        info.title = wstring_to_utf8(wsTitle);
        pList->push_back(info);
    }
    return TRUE;
}

std::string TaskManager::GetAppList() {
    std::vector<WindowInfo> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));

    // Xây dựng chuỗi JSON thủ công
    std::stringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& win : windows) {
        if (!first) ss << ",";
        first = false;
        // Format: {"pid": 123, "name": "Title Window"}
        ss << "{\"pid\": " << win.pid << ", \"name\": \"" << EscapeJson(win.title) << "\"}";
    }
    ss << "]";
    return ss.str();
}

// ==========================================
// LOGIC LIST PROCESSES -> TRẢ VỀ JSON
// ==========================================

std::string TaskManager::GetProcessList() {
    std::stringstream ss;
    ss << "["; // Mở mảng JSON

    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return "[]"; // Trả về mảng rỗng nếu lỗi
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "[]";
    }

    bool first = true;
    do {
        if (!first) ss << ",";
        first = false;

        std::wstring ws(pe32.szExeFile);
        std::string strName = wstring_to_utf8(ws);

        // Format: {"pid": 123, "name": "notepad.exe", "mem": "N/A"}
        // Thêm trường "mem" nếu muốn hiển thị thêm cột, hiện tại để tạm chuỗi trống hoặc số 0
        ss << "{\"pid\": " << pe32.th32ProcessID
            << ", \"name\": \"" << EscapeJson(strName) << "\""
            << "}";

    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    ss << "]"; // Đóng mảng JSON
    return ss.str();
}

// ==========================================
// LOGIC KILL & START (GIỮ NGUYÊN)
// ==========================================

bool TaskManager::KillProcessByID(int pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        // std::cerr << "Loi OpenProcess PID " << pid << "\n";
        return false;
    }
    bool result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result;
}

bool TaskManager::StartProcessByName(const std::string& name) {
    std::wstring wName = utf8_to_wstring(name);
    HINSTANCE hRes = ShellExecuteW(NULL, L"open", wName.c_str(), NULL, NULL, SW_SHOW);
    return (int)hRes > 32;
}