#include "AppWindow.h"

AppWindow::~AppWindow() {
    // SFML tự động dọn dẹp tài nguyên khi object bị hủy
    if (m_window.isOpen()) {
        m_window.close();
    }
}

void AppWindow::Initialize(const std::string& title, int width, int height) {
    // 1. Tạo cửa sổ (Thay thế toàn bộ đoạn RegisterClass + CreateWindowEx)
    m_window.create(sf::VideoMode(width, height), title);

    // Giới hạn FPS để tránh ngốn CPU (Win32 cũ không có cái này nên chạy rất nóng máy)
    m_window.setFramerateLimit(FPS);
}


void AppWindow::RunMessageLoop() {
    sf::Image tempImage;
    // Vòng lặp chính (Thay thế cho GetMessage/DispatchMessage)
    while (m_window.isOpen()) {

        // --- Xử lý sự kiện (Input) ---
        sf::Event event;
        while (m_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                m_window.close(); // Xử lý đóng cửa sổ

            // Nếu cần xử lý Resize để co giãn ảnh:
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                m_window.setView(sf::View(visibleArea));
            }
        }

        // --- Xử lý Logic (Update) ---
        if (global_state->loadToSFMLImage(tempImage)) {

            if (m_texture.loadFromImage(tempImage)) {
                // Cập nhật Sprite
                m_sprite.setTexture(m_texture, true);

                // Tính toán co giãn hình ảnh cho vừa màn hình (như logic cũ)
                    sf::Vector2u windowSize = m_window.getSize();
                    sf::Vector2u textureSize = m_texture.getSize();

                    float scaleX = (float)windowSize.x / textureSize.x;
                    float scaleY = (float)windowSize.y / textureSize.y;

                    m_sprite.setScale(scaleX, scaleY);
            };
        };

        // --- Vẽ (Render) ---
        // Thay thế toàn bộ đoạn WM_PAINT, BeginPaint, EndPaint
        m_window.clear();      // Xóa màn hình đen
        m_window.draw(m_sprite); // Vẽ ảnh
        m_window.display();    // Hiển thị lên
    }
}