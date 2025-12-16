#pragma once
#include <windows.h>
#include <string>

struct AppInfo {
    int id;
    std::string name;
    std::string exePath;
    bool isRunning;
    DWORD pid;
};


struct ServerInfo {
    std::string ip;
    std::string hostname;
    int port;
    bool isOnline;
};

