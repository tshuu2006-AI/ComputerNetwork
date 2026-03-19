# Remote Administration Tool (RAT) – C++ & Python 🚀

Dự án này là một công cụ quản trị máy tính từ xa toàn diện, được xây dựng dựa trên kiến trúc 3 lớp (3-tier architecture). Hệ thống cho phép giám sát và điều khiển các máy trạm trong mạng nội bộ với hiệu suất cao và độ trễ thấp (<100ms) nhờ tận dụng các API hệ thống cấp thấp của Windows.



---

## 📌 Tính năng chính

### 1. Giám sát & Điều khiển từ xa (Remote Control)
* **Screen Streaming:** Livestream màn hình tốc độ cao sử dụng công nghệ **DXGI Desktop Duplication API** để truy cập trực tiếp VRAM của GPU.
* **Webcam Streaming:** Truy cập và truyền tải tín hiệu video trực tiếp từ Webcam thông qua thư viện OpenCV với backend `CAP_DSHOW`.
* **Webcam Recording:** Ghi hình luồng video từ Webcam và lưu trữ trực tiếp trên máy mục tiêu dưới định dạng `.avi` sử dụng bộ mã hóa MJPG.
* **Remote Input:** Mô phỏng chính xác thao tác chuột (di chuyển, click, cuộn) và bàn phím thông qua API `SendInput`.

### 2. Quản trị hệ thống (System Management)
* **Task Manager:** Giám sát danh sách tiến trình (PID, tên, RAM thực thi) bằng `TlHelp32` và thực hiện kết thúc tiến trình (Kill process).
* **App Manager:** Quét hệ thống Registry (HKLM/HKCU) để liệt kê phần mềm đã cài đặt và khởi chạy ứng dụng từ xa.
* **Power Control:** Thực thi các lệnh tắt máy (Shutdown), khởi động lại (Restart) và khóa máy (Lock) sau khi thực hiện nâng quyền `SE_SHUTDOWN_NAME`.

### 3. Giám sát & Mạng
* **Keylogger:** Ghi lại mọi sự kiện phím gõ theo thời gian thực bằng kỹ thuật **Low-level Windows Hooks** (`WH_KEYBOARD_LL`).
* **Service Discovery:** Tự động phát hiện thiết bị trong mạng LAN qua giao thức UDP Broadcast (cổng 9999).

---

## 🏗 Kiến trúc hệ thống
Hệ thống vận hành dựa trên hai luồng dữ liệu chính (Screen Stream và Control Stream) chạy song song:

| Lớp | Thành phần | Vai trò chính |
| :--- | :--- | :--- |
| **Agent** | C++ Server | Đóng vai trò WebSocket Server, thực hiện các tác vụ can thiệp hệ thống cấp thấp. |
| **Middleware** | Python Bridge | Sử dụng Flask-SocketIO làm cầu nối điều phối và chuyển đổi giao thức. |
| **Interface** | Web Client | Giao diện Dashboard (HTML/JS) hiển thị hình ảnh và gửi lệnh điều khiển. |



---

## 🛠 Công nghệ sử dụng
* **Ngôn ngữ:** C++ (Server), Python (Middleware), HTML/CSS/JS (Client).
* **Mạng:** Winsock 2.2, WebSocket (tự triển khai), UDP Broadcast.
* **Đồ họa:** DXGI Desktop Duplication API, OpenCV, GDI+.
* **Hệ thống:** Win32 API (`SendInput`, `Toolhelp32`, `Registry`, `Windows Hooks`).

---

## 🚀 Hướng dẫn chạy chương trình
1. **Agent:** Chạy file `x64/Server.exe` để mở máy chủ trên máy mục tiêu.
2. **Client:** Chạy file `Client/Client.exe` để khởi động Middleware và mở giao diện điều khiển.
3. **Truy cập:** Dashboard sẽ khả dụng tại `http://localhost:5000`.

---

## ⚙️ Hướng dẫn cài đặt & Biên dịch (Build from Source)
Nếu bạn muốn build lại mã nguồn từ đầu:

### 1. Chuẩn bị
* Môi trường Windows 10/11.
* Visual Studio 2019/2022/2026 (C++ Desktop Development).
* Python 3.12 và pip.

### 2. Cấu hình thư viện
1. Clone repository về máy.
2. Chạy file `setup_libs.bat` để tự động tải các thư viện **OpenCV** và **SFML** (mất khoảng 5 phút).
3. Trong thư mục `Client`, chạy lệnh:
   ```bash
   pip install -r requirements.txt


- **User Stories**  
  <a href="https://www.notion.so/328714709b37809f9e0bcb3a455f734f?v=328714709b37809fa1c7000c17ee3536&source=copy_link" target="_blank">
    View User Stories
  </a>

- **WBS (Work Breakdown Structure)**  
  <a href="https://www.notion.so/328714709b378020a948dcf6db37efd1?v=328714709b3780c5a3d4000cace4e2f2&source=copy_link" target="_blank">
    View WBS
  </a>

- **Kanban Board**  
  <a href="https://www.notion.so/Kanban-Task-Board-ad0714709b378357bd41018c8777981a?source=copy_link" target="_blank">
    View Kanban Board
  </a>

- **Task Tracking**  
  <a href="https://www.notion.so/328714709b3780af83d9cbfee79d1905?v=328714709b37801a83de000cd2c6eff6&source=copy_link" target="_blank">
    View Task Tracking
