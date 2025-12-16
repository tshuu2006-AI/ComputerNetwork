#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <map>
#include "DataInfo.h"

class AppManager {
public:
    // Lấy danh sách app
    static std::vector<AppInfo> GetFullAppList();

    // [MỚI] Lấy danh sách app nhưng trả về chuỗi JSON
    static std::string GetAppListJSON();

    static DWORD StartApp(const std::string& path);
    static bool StopApp(DWORD pid);

private:
    static std::string WStringToString(const std::wstring& wstr);
    static std::wstring StringToWString(const std::string& str); // Helper mới cho Unicode
    static std::string CleanPath(std::string path);
    static std::string GetFileName(const std::string& path);
    static std::string EscapeJSON(const std::string& s); // Helper escape ký tự đặc biệt

    static std::map<std::string, DWORD> GetRunningProcessMap();
    static void ScanRegistry(HKEY hRoot, const wchar_t* subKey, std::map<std::string, AppInfo>& uniqueApps);
    static void ScanAppPaths(std::map<std::string, AppInfo>& uniqueApps);
};