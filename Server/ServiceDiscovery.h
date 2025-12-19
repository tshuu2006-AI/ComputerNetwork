#pragma once
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic> 
#include "DataInfo.h"

#pragma comment(lib, "Ws2_32.lib")

class ServiceDiscovery {
private:
    std::string getLocalHostname(); 
public:
    void ServerServiceDiscovery(std::atomic<bool>& isRunning);
};
