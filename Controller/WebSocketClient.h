#pragma once
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <iostream>
#include "GlobalState.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "bcrypt.lib") // Vẫn cần cho SHA-1
class WebSocketClient {
	SOCKET sock;
protected:
	GlobalState* global_state;

public:
	WebSocketClient(GlobalState* state) : sock(INVALID_SOCKET), global_state(state) {}
	bool Connect(const std::string& ip, int port);
	void SendCommand(const std::string& cmd);
	void ReceiveLoop();
};

