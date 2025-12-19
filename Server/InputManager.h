#pragma once
#include <Windows.h>
#include <string>
#include <vector>

class InputManager {
public:
    // Di chuyển chuột đến toạ độ x, y (tính theo pixel màn hình)
    static void MoveMouse(int x, int y);

    // Click chuột: left/right/middle, sự kiện down/up
    // type: 0=Left, 1=Right, 2=Middle
    // isDown: true=nhấn xuống, false=nhả ra
    static void MouseButton(int type, bool isDown);

    // Cuộn chuột
    static void MouseScroll(int delta);

    // Gõ phím (Virtual Key Code)
    static void KeyEvent(int vkCode, bool isDown);

private:
    // Hàm hỗ trợ lấy kích thước màn hình để tính toạ độ tuyệt đối
    static void GetScreenResolution(int& width, int& height);
};