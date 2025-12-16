#pragma once
#include <string>
#include <vector>

class TaskManager {
public:
    // Trả về chuỗi danh sách các process hoặc app (Format: "PID - Name\n...")
    static std::string GetProcessList();
    static std::string GetAppList();

    // Kill process dựa trên PID
    static bool KillProcessByID(int pid);

    // Start process dựa trên tên file (VD: "notepad.exe")
    static bool StartProcessByName(const std::string& name);
};