#include "AppManager.h"
#include <tlhelp32.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "advapi32.lib")

// --- HELPER CONVERSION ---

std::string AppManager::WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

std::wstring AppManager::StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size);
    return wstr;
}

std::string AppManager::EscapeJSON(const std::string& s) {
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
            if ('\x00' <= c && c <= '\x1f') {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            }
            else {
                o << c;
            }
        }
    }
    return o.str();
}

std::string AppManager::CleanPath(std::string path) {
    if (path.empty()) return "";
    // Xóa dấu ngoặc kép
    path.erase(std::remove(path.begin(), path.end(), '\"'), path.end());
    // Xóa tham số sau dấu phẩy (thường gặp trong Registry)
    size_t comma = path.find_last_of(",");
    if (comma != std::string::npos) path = path.substr(0, comma);
    return path;
}

std::string AppManager::GetFileName(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return (slash != std::string::npos) ? path.substr(slash + 1) : path;
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// --- CORE LOGIC ---

std::map<std::string, DWORD> AppManager::GetRunningProcessMap() {
    std::map<std::string, DWORD> mapProcs;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return mapProcs;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            std::string exe = WStringToString(pe32.szExeFile);
            mapProcs[ToLower(exe)] = pe32.th32ProcessID;
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return mapProcs;
}

void AppManager::ScanRegistry(HKEY hRoot, const wchar_t* subKey, std::map<std::string, AppInfo>& uniqueApps) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;

    DWORD index = 0;
    WCHAR keyName[256];
    DWORD keyLen = 256;

    while (RegEnumKeyExW(hKey, index++, keyName, &keyLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        HKEY hSub;
        if (RegOpenKeyExW(hKey, keyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
            WCHAR wName[256] = { 0 };
            WCHAR wIcon[1024] = { 0 };
            DWORD sName = sizeof(wName);
            DWORD sIcon = sizeof(wIcon);

            RegQueryValueExW(hSub, L"DisplayName", NULL, NULL, (LPBYTE)wName, &sName);
            RegQueryValueExW(hSub, L"DisplayIcon", NULL, NULL, (LPBYTE)wIcon, &sIcon);

            if (wcslen(wName) > 0 && wcslen(wIcon) > 0) {
                std::string finalPath = CleanPath(WStringToString(wIcon));
                // Chỉ lấy file .exe
                if (finalPath.length() > 4 && ToLower(finalPath.substr(finalPath.length() - 4)) == ".exe") {
                    if (uniqueApps.find(finalPath) == uniqueApps.end()) {
                        AppInfo app;
                        app.name = WStringToString(wName);
                        app.exePath = finalPath;
                        uniqueApps[finalPath] = app;
                    }
                }
            }
            RegCloseKey(hSub);
        }
        keyLen = 256;
    }
    RegCloseKey(hKey);
}

void AppManager::ScanAppPaths(std::map<std::string, AppInfo>& uniqueApps) {
    const wchar_t* path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths";
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;

    DWORD index = 0;
    WCHAR keyName[256];
    DWORD keyLen = 256;

    while (RegEnumKeyExW(hKey, index++, keyName, &keyLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        HKEY hSub;
        if (RegOpenKeyExW(hKey, keyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
            WCHAR wFullPath[1024] = { 0 };
            DWORD sPath = sizeof(wFullPath);
            // Lấy giá trị Default của key
            if (RegQueryValueExW(hSub, NULL, NULL, NULL, (LPBYTE)wFullPath, &sPath) == ERROR_SUCCESS) {
                std::string finalPath = CleanPath(WStringToString(wFullPath));
                if (finalPath.length() > 4 && ToLower(finalPath.substr(finalPath.length() - 4)) == ".exe") {
                    if (uniqueApps.find(finalPath) == uniqueApps.end()) {
                        AppInfo app;
                        app.name = WStringToString(keyName); // Tên app là tên key (vd: chrome.exe)
                        app.exePath = finalPath;
                        uniqueApps[finalPath] = app;
                    }
                }
            }
            RegCloseKey(hSub);
        }
        keyLen = 256;
    }
    RegCloseKey(hKey);
}

// --- PUBLIC METHODS ---

std::vector<AppInfo> AppManager::GetFullAppList() {
    std::map<std::string, AppInfo> uniqueApps;

    // 1. Quét Registry (Installed Apps)
    ScanRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", uniqueApps);
    ScanRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", uniqueApps);
    ScanRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", uniqueApps);

    // 2. Quét App Paths (Ex: excel.exe, chrome.exe)
    ScanAppPaths(uniqueApps);

    // 3. Lấy danh sách PID đang chạy để map vào
    std::map<std::string, DWORD> runningMap = GetRunningProcessMap();
    std::vector<AppInfo> result;

    int idCounter = 1;
    for (auto& pair : uniqueApps) {
        AppInfo& app = pair.second;
        app.id = idCounter++;

        // Lấy tên file exe (vd: C:\...\chrome.exe -> chrome.exe)
        std::string exeName = ToLower(GetFileName(app.exePath));

        if (runningMap.count(exeName)) {
            app.isRunning = true;
            app.pid = runningMap[exeName];
        }
        else {
            app.isRunning = false;
            app.pid = 0;
        }
        result.push_back(app);
    }
    return result;
}

std::string AppManager::GetAppListJSON() {
    std::vector<AppInfo> apps = GetFullAppList();
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < apps.size(); ++i) {
        json << "{";
        json << "\"id\":" << apps[i].id << ",";
        json << "\"name\":\"" << EscapeJSON(apps[i].name) << "\",";
        json << "\"path\":\"" << EscapeJSON(apps[i].exePath) << "\",";
        json << "\"isRunning\":" << (apps[i].isRunning ? "true" : "false") << ",";
        json << "\"pid\":" << apps[i].pid;
        json << "}";
        if (i < apps.size() - 1) json << ",";
    }
    json << "]";
    return json.str();
}

DWORD AppManager::StartApp(const std::string& path) {
    if (path.empty()) return 0;

    // Chuyển đổi UTF-8 string sang WideString để hỗ trợ đường dẫn có dấu/đặc biệt
    std::wstring wPath = StringToWString(path);
    std::wstring workingDir = wPath.substr(0, wPath.find_last_of(L"\\"));

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Bọc đường dẫn trong ngoặc kép
    std::wstring cmdLine = L"\"" + wPath + L"\"";

    if (CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL,
        workingDir.empty() ? NULL : workingDir.c_str(),
        &si, &pi)) {

        DWORD newPid = pi.dwProcessId;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return newPid;
    }
    return 0;
}

bool AppManager::StopApp(DWORD pid) {
    if (pid == 0) return false;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    bool res = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return res;
}