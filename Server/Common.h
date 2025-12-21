// Common.h
#pragma once
#include <string>

// Địa chỉ IP Multicast
#define MULTICAST_IP "239.0.0.1" 

// Port dùng để khám phá
#define DISCOVERY_PORT 50000

// Tin nhắn "phát hiện" mà Controller gửi đi
#define DISCOVERY_MSG_REQ "REMOTE_DISCOVERY_PING"

// Tin nhắn "phản hồi" mà Host gửi về
#define DISCOVERY_MSG_RES "REMOTE_HOST_PONG"

#define BROADCAST_PORT 9999 

#define BROADCAST_IP "255.255.255.255"

//Window Configuration
const std::string TITLE = "Remote Control";
const int WIDTH = 800;
const int HEIGHT = 600;