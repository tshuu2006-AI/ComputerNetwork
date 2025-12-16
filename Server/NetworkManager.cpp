#include "NetworkManager.h"
#include <chrono> // Để dùng sleep

// --- CONSTRUCTOR ---
NetworkManager::NetworkManager(GlobalState* state)
    : global_state(state),       // 1. Gán con trỏ state          // 2. Truyền state vào Client
    wsserver("0.0.0.0", 8080), // 3. Server lắng nghe mọi IP, port 8080
    discovery(),               // 4. Init Discovery
    isRunning(false)           // 5. Mặc định chưa chạy
{
}

// --- DESTRUCTOR ---
NetworkManager::~NetworkManager() {
    Stop(); // Đảm bảo dừng thread khi hủy class
}

// --- START ---
void NetworkManager::Start() {
    if (isRunning) return;
    isRunning = true;

    std::cout << "[NetworkManager] Starting threads...\n";

    // 1. TCP Server (Nhận kết nối điều khiển/ảnh)
    serverThread = std::thread(&NetworkManager::RunServerLoop, this);

    // 3. UDP Discovery Server (Lắng nghe câu hỏi "Có ai ở đó không?" và trả lời) -> THÊM MỚI
    // Lưu ý: Chúng ta cần Discovery luôn chạy ngầm để người khác (hoặc chính mình) tìm thấy
    discoveryThread = std::thread([this]() {
        // Truyền biến isRunning vào để thread biết khi nào cần dừng
        this->discovery.ServerServiceDiscovery(this->isRunning);
        });
}

// --- STOP ---
void NetworkManager::Stop() {
    if (!isRunning) return;
    isRunning = false;

    // Đợi các thread kết thúc
    if (serverThread.joinable()) serverThread.join();

    // Lưu ý: discoveryThread hiện tại của bạn dùng recvfrom blocking không có timeout
    // nên nó sẽ KHÔNG dừng ngay lập tức khi gọi Stop(). 
    // Khi tắt app, hệ điều hành sẽ tự kill nó (detach để tránh crash khi tắt).
    if (discoveryThread.joinable()) discoveryThread.detach();

    std::cout << "[NetworkManager] Threads stopped.\n";
}

// --- CLIENT LOOP (Logic kết nối & Reconnect) ---


// --- SERVER LOOP (Logic lắng nghe) ---
void NetworkManager::RunServerLoop() {
    // 1. Bắt đầu lắng nghe (Bind & Listen)
    // Hàm Start này ĐÃ CHỨA vòng lặp while(isRunning) bên trong rồi.
    // Nó sẽ block ở đây cho đến khi isRunning = false.
    if (!wsserver.Start(this->isRunning)) {
        std::cout << "[Server] Failed to bind port 8080 or crashed!\n";
        return;
    }

    // Khi hàm Start trả về, nghĩa là Server đã dừng.
    std::cout << "[Server] Server loop finished.\n";
}