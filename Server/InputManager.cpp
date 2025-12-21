#include "InputManager.h"

void InputManager::GetScreenResolution(int& width, int& height) {
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
}

void InputManager::MoveMouse(int x, int y) {
    int screenW, screenH;
    GetScreenResolution(screenW, screenH);

    // Tỉ lệ nén ảnh mà bạn đang dùng
    float compressScale = 0.6f;

    // Phóng đại tọa độ nhận được về kích thước thực tế của màn hình
    double realX = (double)x / compressScale;
    double realY = (double)y / compressScale;

    // Chuyển đổi sang hệ tọa độ chuẩn hóa của Windows (0 -> 65535)
    double fX = realX * (65535.0 / (double)screenW);
    double fY = realY * (65535.0 / (double)screenH);

    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = (LONG)fX;
    input.mi.dy = (LONG)fY;

    SendInput(1, &input, sizeof(INPUT));
}

void InputManager::MouseButton(int type, bool isDown) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;

    switch (type) {
    case 0: // Left
        input.mi.dwFlags = isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
    case 1: // Right
        input.mi.dwFlags = isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
    case 2: // Middle
        input.mi.dwFlags = isDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
    }

    SendInput(1, &input, sizeof(INPUT));
}

void InputManager::MouseScroll(int delta) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta; // 120 ho?c -120
    SendInput(1, &input, sizeof(INPUT));
}

void InputManager::KeyEvent(int vkCode, bool isDown) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    // N?u nh? phím thì thêm c? KEYEVENTF_KEYUP, nh?n thì ?? 0
    input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;

    SendInput(1, &input, sizeof(INPUT));
}