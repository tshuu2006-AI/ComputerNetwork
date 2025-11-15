#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <vector>
#include <map>

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
    void start();

private:
    std::string server_ip;
    int server_port;
    SOCKET server_socket;

    void handle_client(SOCKET client_socket);
    bool perform_handshake(SOCKET client_socket);
    bool receive_frame(SOCKET client_socket, std::vector<char>& out_frame);
    WebSocketFrameType decode_websocket_frame(const std::vector<char>& frame, std::string& out_payload);
    void process_command(SOCKET client_socket, const std::string& cmd);
    bool send_data(SOCKET client_socket, const std::string& data, bool isBinary);
    std::string calculate_accept_key(const std::string& client_key);
    // Hàm base64_encode cũ đã bị xóa bỏ
};