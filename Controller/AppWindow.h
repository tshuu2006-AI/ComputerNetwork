#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "GlobalState.h"

class AppWindow {
private:

    //Fps
    int FPS = 90;
    // Thay thế cho HWND
    sf::RenderWindow m_window;

    // Thay thế cho HGLOBAL, IStream, GDI+ Image
    sf::Texture m_texture;
    sf::Sprite m_sprite;

protected:
    // Biến toàn cục
    GlobalState* global_state;

public:

    AppWindow(GlobalState* state) : global_state(state) {};
    ~AppWindow();

    // Khởi tạo cửa sổ
    void Initialize(const std::string& title, int width, int height);

    // Chạy vòng lặp chính
    void RunMessageLoop();

};