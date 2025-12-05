#include "WebSocketClient.h"



bool WebSocketClient::Connect(const std::string& ip, int port) {
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return false;

    // --- HANDSHAKE ---
    // Client phải gửi Key ngẫu nhiên (Base64 giả lập ở đây cho đơn giản)
    std::string req =
        "GET / HTTP/1.1\r\n"
        "Host: " + ip + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

    send(sock, req.c_str(), req.length(), 0);

    char buf[1024];
    if (recv(sock, buf, 1024, 0) > 0) {
        if (std::string(buf).find("101 Switching Protocols") != std::string::npos) {
            global_state->isConnected = true;
            return true;
        }
    }
    return false;
}

// Gửi lệnh (BẮT BUỘC PHẢI MASK VÌ LÀ CLIENT)
void WebSocketClient::SendCommand(const std::string& cmd) {
    if (!global_state->isConnected) return;
    std::vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + Text
    frame.push_back(0x80 | (unsigned char)cmd.length()); // Mask bit = 1

    // Mask Key (4 byte ngẫu nhiên)
    unsigned char mask[4] = { 1, 2, 3, 4 };
    frame.insert(frame.end(), mask, mask + 4);

    for (size_t i = 0; i < cmd.length(); i++) {
        frame.push_back(cmd[i] ^ mask[i % 4]);
    }
    send(sock, (char*)frame.data(), frame.size(), 0);
}

void WebSocketClient::ReceiveLoop() {
    while (global_state->isConnected) {
        // Đọc Header (2 byte đầu)
        unsigned char header[2];
        if (recv(sock, (char*)header, 2, 0) <= 0) break;

        int opcode = header[0] & 0x0F;
        unsigned long long payloadLen = header[1] & 0x7F;

        // Xử lý độ dài mở rộng (nếu ảnh lớn)
        if (payloadLen == 126) {
            unsigned char exLen[2];
            recv(sock, (char*)exLen, 2, 0);
            payloadLen = (exLen[0] << 8) | exLen[1];
        }
        else if (payloadLen == 127) {
            unsigned char exLen[8];
            recv(sock, (char*)exLen, 8, 0);

            // SỬA: Tính toán lại độ dài gói tin 64-bit chuẩn Big-Endian
            payloadLen = 0;
            for (int i = 0; i < 8; i++) {
                payloadLen = (payloadLen << 8) | exLen[i];
            }
        }

        // Đọc dữ liệu ảnh
        std::vector<char> data(payloadLen);
        size_t received = 0;
        while (received < payloadLen) {
            int r = recv(sock, data.data() + received, payloadLen - received, 0);
            if (r <= 0) break;
            received += r;
        }

        // Nếu là Binary Frame (0x02) -> Là ảnh JPEG
        if (opcode == 0x02) {
            
            global_state->setImageBuffer(data);

            // Gửi lệnh tiếp theo ngay lập tức để lấy ảnh mới (Loop)
            SendCommand("SCREENSHOT");
        }
    }
    global_state->isConnected = false;
}
