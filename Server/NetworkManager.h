#pragma once
#include "WebSocketServer.h"
#include "ServiceDiscovery.h"
#include "GlobalState.h"
#include <thread>
#include <atomic>
#include <iostream>

class NetworkManager {
private:

    GlobalState* global_state;

    WebSocketServer wsserver;
    ServiceDiscovery discovery;

    std::thread clientThread;
    std::thread serverThread;
    std::thread discoveryThread; 

    std::atomic<bool> isRunning;

public:
    NetworkManager(GlobalState* state);
    ~NetworkManager();

    void Start();
    void Stop();

private:
    void RunServerLoop();
};