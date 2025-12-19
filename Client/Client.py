from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit
import threading
import websocket
import base64
import socket
import subprocess
import platform
import json
from concurrent.futures import ThreadPoolExecutor

# ================= CONFIG =================
AGENT_IP = "127.0.0.1"    # IP Mặc định hiển thị trên Web
AGENT_PORT = 8080         # Port của C++ Agent (WebSocket)
FLASK_PORT = 5000         # Port của Python Web Server
DISCOVERY_PORT = 9999     # Port lắng nghe UDP Broadcast (Service Discovery)

app = Flask(__name__)
app.config["SECRET_KEY"] = "remote_control_secret_key"

# Cấu hình SocketIO với mode threading để tương thích tốt với các luồng nền
socketio = SocketIO(
    app,                    
    async_mode="threading",
    cors_allowed_origins="*",
)

# ================= GLOBAL STATE =================
agent_ws = None           # Object WebSocket kết nối tới Agent
agent_thread = None       # Luồng nhận dữ liệu từ Agent
agent_connected = False   # Trạng thái kết nối
stop_event = threading.Event()
lock = threading.Lock()   # Khóa để tránh xung đột luồng
agent_state = {
    "camera": False,
    "screen": False
}

# ================= HELPER FUNCTIONS =================

def get_local_ip():
    """Lấy IP LAN của máy Server Python"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Kết nối ảo đến Google DNS
        s.connect(('8.8.8.8', 80))
        local_ip = s.getsockname()[0]
    except Exception:
        local_ip = '127.0.0.1'
    finally:
        s.close()
    return local_ip

def scan_single_ip(ip):
    """Hàm quét 1 IP: Ping -> Kiểm tra Port Agent"""
    target_port = AGENT_PORT 
    
    # BƯỚC 1: Ping nhẹ (Timeout ngắn)
    param = '-n' if platform.system().lower() == 'windows' else '-c'
    command = ['ping', param, '1', '-w', '100', ip] 
    
    startupinfo = None
    if platform.system().lower() == 'windows':
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    
    try:
        # Thực hiện ping
        if platform.system().lower() == 'windows':
            response = subprocess.call(command, startupinfo=startupinfo)
        else:
            response = subprocess.call(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        if response != 0:
            return None # Máy tắt hoặc chặn Ping

        # BƯỚC 2: Kiểm tra Cổng (Port Check)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(0.2) # Timeout cực ngắn
        result = sock.connect_ex((ip, target_port))
        sock.close()

        if result == 0:
            # ==> TÌM THẤY AGENT!
            try:
                hostname = socket.gethostbyaddr(ip)[0]
            except:
                hostname = "Agent Device"
            
            print(f"[SCAN FOUND] Phát hiện Agent tại: {ip}")
            return {'ip': ip, 'hostname': hostname, 'status': 'Online'}
            
    except Exception:
        pass
        
    return None

# ================= BACKGROUND THREADS =================

def udp_listener():
    """Luồng lắng nghe tín hiệu UDP Broadcast từ Agent (Service Discovery)"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    
    try:
        # Bind vào tất cả các IP trên cổng 9999
        s.bind(('0.0.0.0', DISCOVERY_PORT))
        print(f"[DISCOVERY] Đang lắng nghe UDP trên cổng {DISCOVERY_PORT}...")
    except Exception as e:
        print(f"[DISCOVERY] Lỗi bind cổng UDP: {e}")
        return

    while True:
        try:
            # Nhận dữ liệu
            data, addr = s.recvfrom(1024)
            agent_ip = addr[0]
            message = data.decode('utf-8')
            
            # Giải mã JSON từ Agent: {"hostname": "PC-Name", "port": 8080}
            try:
                agent_info = json.loads(message)
                agent_info['ip'] = agent_ip 
                
                # Gửi ngay lên Web để hiển thị (Sự kiện 'discovery_found')
                socketio.emit('discovery_found', agent_info)
            except json.JSONDecodeError:
                pass
                
        except Exception as e:
            print(f"[DISCOVERY] Lỗi: {e}")
            break

def agent_receiver():
    """Luồng nhận dữ liệu từ WebSocket Agent C++"""
    global agent_ws, agent_connected

    print("[AGENT] Receiver thread started")
    try:
        while not stop_event.is_set():
            try:
                # Nhận dữ liệu từ C++
                opcode, data = agent_ws.recv_data()

                if opcode == websocket.ABNF.OPCODE_TEXT:
                    decoded_data = data.decode("utf-8")
                    print("[AGENT TEXT]", decoded_data)

                    # ===== ACK CAMERA =====
                    if decoded_data == "CAM_READY":
                        agent_state["camera"] = True
                        socketio.emit("status", {"msg": "Camera đã sẵn sàng", "status": "success"})

                    elif decoded_data == "CAM_FAILED":
                        agent_state["camera"] = False
                        socketio.emit("status", {"msg": "Mở camera thất bại", "status": "error"})

                    elif decoded_data == "CAM_CLOSED":
                        agent_state["camera"] = False

                    # ===== ACK SCREEN =====
                    elif decoded_data == "SCREEN_READY":
                        agent_state["screen"] = True

                    elif decoded_data == "SCREEN_CLOSED":
                        agent_state["screen"] = False

                    socketio.emit("agent_json", decoded_data)


                elif opcode == websocket.ABNF.OPCODE_BINARY:
                    # Nếu là Binary (Hình ảnh Camera/Screenshot)
                    encoded = base64.b64encode(data).decode("utf-8")
                    socketio.emit("agent_image", encoded)

            except websocket.WebSocketTimeoutException:
                continue # Timeout là bình thường, tiếp tục vòng lặp

            except Exception as e:
                print("[AGENT] Receive error (Mất kết nối):", e)
                break
    finally:
        # Dọn dẹp khi mất kết nối
        with lock:
            if agent_ws:
                try:
                    agent_ws.close()
                except: pass
            agent_ws = None
            agent_connected = False
            stop_event.clear()

        socketio.emit("status", {"msg": "Mất kết nối với Agent", "status": "error"})
        print("[AGENT] Receiver thread stopped")

# ================= ROUTES =================
@app.route("/")
def index():
    local_ip = get_local_ip()
    return render_template(
        "index.html",
        agent_ip=local_ip,  # Gợi ý IP LAN hiện tại
        agent_port=AGENT_PORT,
        flask_port=FLASK_PORT
    )

# ================= SOCKET.IO EVENTS =================

@socketio.on("connect_agent")
def connect_agent(data):
    """Xử lý lệnh kết nối từ Web -> C++ Agent"""
    global agent_ws, agent_thread, agent_connected

    with lock:
        if agent_connected:
            emit("status", {"msg": "Agent đã kết nối rồi", "status": "warning"})
            return

        try:
            # Tạo kết nối WebSocket Client tới C++ Server
            ws = websocket.create_connection(
                f"ws://{data['ip']}:{data['port']}",
                timeout=3
            )
            ws.settimeout(1) # Timeout đọc socket

            agent_ws = ws
            agent_connected = True
            stop_event.clear()

            # Khởi động luồng nhận dữ liệu
            agent_thread = threading.Thread(target=agent_receiver, daemon=True)
            agent_thread.start()

            emit("status", {"msg": "Kết nối Agent C++ thành công!", "status": "success"})

        except Exception as e:
            agent_ws = None
            agent_connected = False
            emit("status", {"msg": f"Kết nối thất bại: {e}", "status": "error"})

@socketio.on("send_command")
def send_command(data):
    global agent_ws, agent_state

    if not agent_connected or not agent_ws:
        emit("status", {"msg": "Chưa kết nối Agent", "status": "error"})
        return

    cmd = data.get("cmd")
    if not cmd:
        return

    try:
        # ===== CAMERA =====
        if cmd == "CAMERA_ON":
            if agent_state["screen"]:
                emit("status", {
                    "msg": "Đang chia sẻ màn hình, hãy tắt screen trước",
                    "status": "warning"
                })
                return

            if agent_state["camera"]:
                emit("status", {
                    "msg": "Camera đã bật",
                    "status": "info"
                })
                return

            agent_ws.send("CMD_OPEN_CAM")
            emit("status", {"msg": "Đang khởi động camera...", "status": "process"})
            return

        if cmd == "CAMERA_OFF":
            agent_ws.send("CMD_CLOSE_CAM")
            return

        # ===== SCREEN =====
        if cmd == "SCREEN_ON":
            if agent_state["camera"]:
                emit("status", {
                    "msg": "Camera đang bật, hãy tắt camera trước",
                    "status": "warning"
                })
                return

            agent_ws.send("CMD_OPEN_SCREEN")
            return

        if cmd == "SCREEN_OFF":
            agent_ws.send("CMD_CLOSE_SCREEN")
            return

        # ===== OTHER =====
        agent_ws.send(cmd)

    except Exception as e:
        emit("status", {"msg": f"Lỗi gửi lệnh: {e}", "status": "error"})



@socketio.on('scan_network')
def handle_scan_network():
    """Xử lý lệnh quét mạng thủ công"""
    print("------------------------------------------------")
    print("[SCAN] Bắt đầu quét mạng...")
    emit('scan_status', {'msg': 'Đang xác định dải IP...', 'status': 'process'})
    
    local_ip = get_local_ip()
    ip_parts = local_ip.split('.')
    base_ip = f"{ip_parts[0]}.{ip_parts[1]}.{ip_parts[2]}"
    
    emit('scan_status', {'msg': f'Đang quét dải {base_ip}.1 - {base_ip}.254...', 'status': 'process'})

    ip_list = [f"{base_ip}.{i}" for i in range(1, 255)]
    found_devices = []

    # Dùng ThreadPool để quét nhanh (100 luồng)
    with ThreadPoolExecutor(max_workers=100) as executor:
        results = executor.map(scan_single_ip, ip_list)

    for res in results:
        if res:
            found_devices.append(res)

    print(f"[SCAN] Hoàn tất. Tìm thấy: {len(found_devices)}")
    emit('scan_result', found_devices)
    
    if len(found_devices) == 0:
        emit('scan_status', {'msg': 'Không tìm thấy thiết bị nào (Kiểm tra Firewall/Port).', 'status': 'error'})
    else:
        emit('scan_status', {'msg': f'Hoàn tất! Tìm thấy {len(found_devices)} thiết bị.', 'status': 'success'})

@socketio.on("disconnect")
def client_disconnect():
    print("[WEB] Client disconnected")

# ================= MAIN ENTRY POINT =================
if __name__ == "__main__":
    # 1. Khởi động luồng lắng nghe UDP Discovery
    udp_thread = threading.Thread(target=udp_listener, daemon=True)
    udp_thread.start()

    print(f"✅ Web App running at http://127.0.0.1:{FLASK_PORT}")
    print(f"✅ UDP Discovery listening on port {DISCOVERY_PORT}")

    # 2. Khởi động Flask SocketIO Server
    socketio.run(
        app,
        host="0.0.0.0",
        port=FLASK_PORT,
        debug=False, # Tắt debug để tránh xung đột với luồng
        allow_unsafe_werkzeug=True
    )