#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "ServiceDiscovery.h" // Cần struct ServerInfo

class GlobalState {
public:
    // Trạng thái kết nối
    bool isConnected = false;
    bool isClientMode = true; // Mặc định là Client để tìm Server
    bool cameraModeOn = false;

    // Dữ liệu ảnh nhận được (cho Client)
    std::vector<char> currentImageBuffer;
    std::mutex imageMutex;

    // Danh sách server tìm thấy
    std::vector<ServerInfo> foundServers;

    // Logic kết nối UI
    bool hasConnectionRequest = false;
    std::string targetIP = "";
    int targetPort = 0;

    // --- CÁC HÀM HỖ TRỢ ---

    void setImageBuffer(const std::vector<char>& data) {
        std::lock_guard<std::mutex> lock(imageMutex);
        currentImageBuffer = data;
    }

    bool getTargetServer(std::string& ip, int& port) {
        if (hasConnectionRequest) {
            ip = targetIP;
            port = targetPort;
            return true;
        }
        return false;
    }

    void resetConnectionRequest() {
        hasConnectionRequest = false;
    }

    void setFoundServers(const std::vector<ServerInfo>& servers) {
        foundServers = servers;
    }
};