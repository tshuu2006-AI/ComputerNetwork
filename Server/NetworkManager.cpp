#include "NetworkManager.h"
#include <chrono>

// --- CONSTRUCTOR ---
NetworkManager::NetworkManager(GlobalState* state)
    : global_state(state),       
    wsserver("0.0.0.0", 8080), 
    discovery(),             
    isRunning(false)        
{}

// --- DESTRUCTOR ---
NetworkManager::~NetworkManager() {
    Stop(); 
}

// --- START ---
void NetworkManager::Start() {
    if (isRunning) return;
    isRunning = true;

    std::cout << "[NetworkManager] Starting threads...\n";
    serverThread = std::thread(&NetworkManager::RunServerLoop, this);

    discoveryThread = std::thread([this]() {
        this->discovery.ServerServiceDiscovery(this->isRunning);
        });
}

// --- STOP ---
void NetworkManager::Stop() {
    if (!isRunning) return;
    isRunning = false;

    if (serverThread.joinable()) serverThread.join();
    if (discoveryThread.joinable()) discoveryThread.detach();

    std::cout << "[NetworkManager] Threads stopped.\n";
}


// --- SERVER LOOP (Logic lắng nghe) ---
void NetworkManager::RunServerLoop() {

    if (!wsserver.Start(this->isRunning)) {
        std::cout << "[Server] Failed to bind port 8080 or crashed!\n";
        return;
    }
    std::cout << "[Server] Server loop finished.\n";
}