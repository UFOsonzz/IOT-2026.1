# Hệ thống IoT Giám sát chuỗi cung ứng lạnh tuân thủ GSP

1. Đồ án học phần **IT4735 - IoT và Ứng dụng** - Đại học Bách Khoa Hà Nội.  
2. Hệ thống giám sát chuỗi cung ứng lạnh theo tiêu chuẩn Thực hành Tốt Bảo quản (**GSP - Good Storage Practices**) dành cho Vaccine (`+2.0°C` đến `+8.0°C`) và Thủy sản đông lạnh (`-20.0°C` đến `-40.0°C`).

---

## 1. Tổng quan kiến trúc

1. **Kiến trúc Một chiều (Unidirectional):** Thiết bị biên ESP32-S3 chỉ đọc cảm biến, băm SHA-256, ký số ECDSA và đẩy dữ liệu lên Broker; **KHÔNG** mở kênh nhận lệnh điều khiển từ xa nhằm đảm bảo tính chống chối bỏ.
2. **Dữ liệu Chuỗi thời gian Bất biến (Append-Only & Immutable):** Bảng `telemetry_logs` trên TimescaleDB được khóa cứng thao tác `UPDATE` và `DELETE` ở tầng Engine của cơ sở dữ liệu bằng Trigger `trg_immutable_telemetry` (mã lỗi SQLSTATE `42501`).
3. **Đồng bộ bù Không trùng lặp (Idempotent Auto-Sync):** Kết hợp lưu đệm Offline tại biên và ràng buộc khóa chính kép `ON CONFLICT (device_id, ts_rtc) DO NOTHING` tại CSDL.
4. **Đa nhiệm 2 Lõi trên ESP32-S3:**
   - **Core 0:** Đọc cảm biến, kiểm tra ngưỡng GSP, kích hoạt Relay bật còi/đèn tại chỗ tức thời, xử lý nút nhấn HMI, băm SHA-256 và tính toán ECDSA.
   - **Core 1:** Quản lý kết nối Wi-Fi (3 chế độ + Captive Portal + UDP Auto-Discovery), duy trì MQTT Keep-Alive + Di chúc LWT và đẩy dữ liệu qua Queue.

---

## 2. Cấu trúc Thư mục

```text
IOT-2026.1/
├── .env.example
├── .gitignore
├── README.md
├── docker-compose.yml
├── run_simulator_mac.sh                       # Script tự động chạy GUI Simulator trên macOS/Linux
├── run_simulator_win.bat                      # Script tự động chạy GUI Simulator trên Windows
├── backend/
│   ├── Dockerfile
│   ├── requirements.txt
│   └── app/
│       ├── db.py                              # Kết nối TimescaleDB, LATERAL JOIN, Cursor Pagination, Verify Chain
│       ├── main.py                            # FastAPI REST API (chỉ đọc), WebSocket & UDP Discovery (8888/udp)
│       └── mqtt_worker.py                     # MQTT Subscriber (QoS 1) và đối soát chuỗi chuẩn hóa SHA-256/ECDSA
├── docs/
│   ├── 01_technical_dossier/                  # Hồ sơ kỹ thuật (8 hạng mục cốt lõi)
│   │   ├── 01_requirements_and_econ.md        # Đặc tả yêu cầu FR/NFR & Phân tích kinh tế TCO/ROI
│   │   ├── 02_bom_hardware_selection.md       # Lựa chọn cảm biến & Danh mục vật tư (BOM)
│   │   ├── 03_system_architecture.md          # Kiến trúc 4 lớp & Lập luận chọn giao thức
│   │   ├── 04_hardware_wiring.md              # Sơ đồ đấu nối phần cứng & Bảng gán chân ESP32-S3
│   │   ├── 05_database_schema.md              # Lược đồ CSDL 4 bảng & Chiến lược đánh chỉ số (Index)
│   │   ├── 06_api_spec.md                     # Hợp đồng dữ liệu MQTT, Canonical String & Đặc tả REST/WS API
│   │   ├── 06_postman_collection.json         # Bộ kiểm thử tự động REST API trên Postman
│   │   ├── 07_ui_and_demo_script.md           # Thiết kế giao diện & Kịch bản Demo 5 bước
│   │   └── 08_error_and_lesson_log.md         # Nhật ký lỗi kỹ thuật và bài học kinh nghiệm
│   ├── 02_testing_and_handover/               # Hồ sơ kiểm thử & Bàn giao
│   │   ├── handover_dossier.md                # Hồ sơ bàn giao hệ thống & Checklist an toàn thông tin
│   │   ├── rubric_self_assessment.md          # Bản tự đánh giá theo Rubric học phần
│   │   ├── test_plan_and_report.md            # Kế hoạch & Báo cáo kiểm thử 3 mức + 4 kịch bản bất lợi
│   │   └── trial_run_7days_log.md             # Nhật ký vận hành thử 7 ngày & 3 chỉ tiêu độ tin cậy
│   └── 03_project_management/                 # Hồ sơ quản lý dự án
│       ├── git_workflow_and_collaboration.md  # Quy trình Git, phân nhánh & tiêu chuẩn hoàn thành (DoD)
│       ├── meeting_minutes.md                 # Biên bản họp nhóm định kỳ
│       ├── product_backlog.md                 # Quản lý Product Backlog theo từng Sprint
│       ├── progress_reports.md                # Báo cáo tiến độ theo giai đoạn
│       ├── risk_register.md                   # Sổ đăng ký và quản trị rủi ro (Risk Register)
│       └── schedule_and_roles.md              # Kế hoạch tiến độ 15 tuần & Phân công vai trò
├── firmware/
│   ├── .gitignore
│   ├── platformio.ini                         # Cấu hình biên dịch cho ESP32-S3-WROOM-1-N16R8
│   ├── include/WifiConfig.h                   # Quản lý Wi-Fi 3 chế độ, Captive Portal & UDP Auto-Discovery
│   └── src/main.cpp                           # Firmware chính chạy FreeRTOS 2 lõi
├── infra/
│   ├── db/init.sql                            # Khởi tạo Hypertable TimescaleDB, Trigger bất biến & dữ liệu mẫu
│   └── mosquitto/mosquitto.conf               # Cấu hình Eclipse Mosquitto 2.x (Persistence & Queue)
├── tools/
│   ├── gui_simulator.py                       # Ứng dụng Desktop GUI mô phỏng phần cứng ESP32-S3 & tấn công (HIL)
│   └── dummy_simulator.py                     # Công cụ CLI giả lập nhanh 5 kịch bản qua dòng lệnh
└── web/
    ├── Dockerfile
    ├── index.html
    ├── package.json
    ├── tsconfig.json
    ├── vite.config.ts
    └── src/                                   # Giao diện React 18 + TypeScript + TailwindCSS v4
        ├── App.tsx
        ├── index.css
        ├── main.tsx
        └── types.ts
```

## 3. Hướng dẫn chạy Toàn bộ Hệ thống

### 3.1. Điều kiện tiên quyết

Để chạy trọn vẹn hệ sinh thái của đồ án, máy tính cần đáp ứng các yêu cầu sau:

1. **Docker & Docker Compose:** Đã cài đặt [Docker Desktop](https://www.docker.com/) hoặc Docker Engine + Docker Compose Plugin v2 (Linux) và Docker Daemon đang hoạt động.
    
2. **Giải phóng các cổng mạng trên máy host (tránh xung đột cổng):**
    
    - `1883/tcp`: MQTT Broker (Eclipse Mosquitto)
        
    - `5432/tcp`: Cơ sở dữ liệu (TimescaleDB / PostgreSQL 16)
        
    - `8000/tcp`: Backend REST API và WebSocket (FastAPI)
        
    - `8888/udp`: Cổng UDP Broadcast Auto-Discovery (giúp ESP32-S3 tự động tìm IP Server)
        
    - `5173/tcp`: Giao diện Web Dashboard (React + Vite)
        
3. **Nguồn phát dữ liệu IoT:**
    
    > Lệnh `docker compose up` chỉ khởi chạy _tầng Nền tảng Server và Web_ (Broker + DB + Backend + Frontend). Khi mới khởi chạy, hệ thống ở trạng thái **chờ**, thiết bị mẫu `GSP-TRUCK-01` sẽ hiển thị `OFFLINE (LWT)` trên Web.
    > 
    > Để kích hoạt luồng dữ liệu End-to-End, bạn có thể chọn 1 trong 2 nguồn:
    > 
    > - **Cách A (Kiểm thử & Demo bằng phần mềm mô phỏng):** Cài sẵn **Python 3.9+** (kèm `tkinter`) trên máy host để chạy ứng dụng Desktop GUI Simulator hoặc CLI Simulator (xem [Mục 4](https://www.google.com/search?q=%234-h%C6%B0%E1%BB%9Bng-d%E1%BA%ABn-ch%E1%BA%A1y-b%E1%BB%99-gi%E1%BA%A3-l%E1%BA%ADp-thi%E1%BA%BFt-b%E1%BB%8B-simulator)).
    >     
    > - **Cách B (Chạy với mạch phần cứng thực tế):** Cần mạch **ESP32-S3-DevKitC-1 (N16R8)**, cáp USB Type-C và tiện ích **PlatformIO** (xem [Mục 5](https://www.google.com/search?q=%235-h%C6%B0%E1%BB%9Bng-d%E1%BA%ABn-bi%C3%AAn-d%E1%BB%8Bch-v%C3%A0-n%E1%BA%A1p-firmware-cho-esp32-s3)).
    >     
    

### 3.2. Khởi chạy Server và Web bằng một lệnh

#### Bước 1: Chuẩn bị file biến môi trường

Tại thư mục gốc của dự án, sao chép `.env.example` thành `.env`:

Bash

```
cp .env.example .env
```

Mặc định file `.env.example` đã cấu hình sẵn các thông số đồng bộ để chạy ngay. Nếu tùy chỉnh `POSTGRES_USER`, `POSTGRES_PASSWORD` hoặc `POSTGRES_DB`, hãy cập nhật tương ứng biến `DATABASE_URL`.

#### Bước 2: Build và khởi chạy 4 container

Chạy lệnh sau tại thư mục gốc của dự án:

Bash

```
docker compose up -d --build
```

#### Bước 3: Kiểm tra trạng thái các dịch vụ

Bash

```
docker compose ps
```

Khi khởi chạy thành công, cả 4 container sẽ ở trạng thái `Up` (và `healthy` đối với TimescaleDB):

|**Tên Container**|**Dịch vụ**|**Cổng ánh xạ**|**Trạng thái mong đợi**|
|---|---|---|---|
|`gsp-mosquitto`|Eclipse Mosquitto 2.x MQTT Broker|`localhost:1883`|Up|
|`gsp-timescaledb`|CSDL TimescaleDB (PostgreSQL 16)|`localhost:5432`|Up (healthy)|
|`gsp-backend`|FastAPI REST + WebSocket + MQTT Worker + UDP Discovery|`localhost:8000` (TCP), `8888` (UDP)|Up|
|`gsp-web`|React + TypeScript + TailwindCSS v4 Dashboard|`localhost:5173`|Up|

#### Bước 4: Truy cập các điểm dịch vụ

- **Giao diện Web Dashboard:** [http://localhost:5173](http://localhost:5173/)
    
- **Tài liệu API Backend (Swagger UI):** [http://localhost:8000/docs](http://localhost:8000/docs)
    
- **Địa chỉ kết nối MQTT Broker:** `localhost:1883`
    
- **Địa chỉ kết nối TimescaleDB:** `localhost:5432`
    

### 3.3. Các lệnh Quản trị Docker thường dùng

- Xem toàn bộ log thời gian thực:
    
    Bash
    
    ```
    docker compose logs -f
    ```
    
- Xem log riêng của từng dịch vụ (ví dụ `backend` hoặc `mosquitto`):
    
    Bash
    
    ```
    docker compose logs -f backend
    docker compose logs -f mosquitto
    ```
    
- Khởi động lại dịch vụ:
    
    Bash
    
    ```
    docker compose restart
    # hoặc chỉ khởi động lại backend
    docker compose restart backend
    ```
    
- Dừng toàn bộ hệ thống (giữ nguyên dữ liệu CSDL):
    
    Bash
    
    ```
    docker compose down
    ```
    
- Dừng và xóa sạch dữ liệu Docker Volume (Reset CSDL về trạng thái khởi tạo):
    
    Bash
    
    ```
    docker compose down -v
    ```
    

## 4. Hướng dẫn chạy Bộ giả lập thiết bị (Simulator)

Dự án cung cấp **2 công cụ giả lập** trong thư mục `tools/` để kiểm thử toàn bộ luồng **MQTT $\rightarrow$ FastAPI $\rightarrow$ TimescaleDB $\rightarrow$ WebSocket $\rightarrow$ React UI** mà không cần cắm mạch phần cứng:

### 4.1. Chạy ứng dụng Desktop GUI Simulator (`tools/gui_simulator.py`) - Khuyên dùng

Ứng dụng giao diện đồ họa (dùng `customtkinter`) mô phỏng trực quan phần cứng ESP32-S3-N16R8: màn hình OLED SSD1306, thanh trượt chỉnh nhiệt độ/độ ẩm SHT30, xả pin dự phòng 18650 khi rút nguồn 5V, ngắt Wi-Fi ghi đệm ra file thẻ nhớ thật (`tools/sd_card_buffer.csv`), rút thẻ nhớ sửa lậu dữ liệu (Tampering Lab) và đẩy bù tự động (Auto-Sync).

- **Trên Windows:** Mở Command Prompt / PowerShell tại thư mục gốc và chạy (hoặc nhấp đúp vào file):
    
    DOS
    
    ```
    run_simulator_win.bat
    ```
    
- **Trên macOS / Linux:** Mở Terminal tại thư mục gốc và chạy:
    
    Bash
    
    ```
    chmod +x run_simulator_mac.sh
    ./run_simulator_mac.sh
    ```
    

_(Script sẽ tự động tạo môi trường ảo `.venv`, cài đặt `paho-mqtt`, `customtkinter` và mở cửa sổ GUI)._

### 4.2. Chạy công cụ CLI Simulator trên dòng lệnh (`tools/dummy_simulator.py`)

Nếu chạy trên môi trường không có giao diện đồ họa (hoặc thiếu thư viện `tkinter` của hệ điều hành), bạn có thể chạy công cụ menu dòng lệnh hỗ trợ đầy đủ 5 kịch bản kiểm thử:

Bash

```
source .venv/bin/activate  # Trên Windows: .venv\Scripts\activate
python tools/dummy_simulator.py --mode interactive
# Hoặc chạy tự động xoay vòng kịch bản mỗi 3 giây:
python tools/dummy_simulator.py --mode auto --interval 3.0
```

### 4.3. Quan sát kết quả trên Web Dashboard (`http://localhost:5173`)

1. Trạng thái thiết bị chuyển sang **`THIẾT BỊ: ONLINE`** màu xanh lá.
    
2. Nhiệt độ, độ ẩm, điện áp pin 18650 và bảng nhật ký cập nhật trực tiếp qua WebSocket.
    
3. Khi nhiệt độ vượt ngưỡng (`> +8.0°C` hoặc `< +2.0°C`), giao diện phát cảnh báo nhấp nháy đỏ.
    
4. Khi thử nghiệm sửa lậu dữ liệu trên thẻ SD hoặc bắn gói tin MITM, bản ghi bị gắn nhãn đỏ **`TAMPERED`**.
    
5. Nhấn nút **"Kiểm toán chuỗi chữ ký GSP"** ở góc trên bên phải để gọi API `/api/v1/devices/GSP-TRUCK-01/verify-chain` đối soát tính toàn vẹn của toàn bộ lịch sử dữ liệu.
    

## 5. Hướng dẫn Biên dịch và Nạp Firmware cho ESP32-S3

### Bước 1: Biên dịch và nạp xuống mạch ESP32-S3-N16R8

Firmware trong `firmware/` sử dụng cơ chế quản lý mạng lưu trên bộ nhớ NVS Flash (`firmware/include/WifiConfig.h`), không cần sửa cứng SSID hay IP trong mã nguồn. Sử dụng **PlatformIO CLI** hoặc **PlatformIO IDE** trên VS Code:

Bash

```
cd firmware

# Biên dịch kiểm tra mã nguồn
pio run

# Nạp firmware xuống mạch và mở cổng Serial Monitor (115200 baud)
pio run --target upload --target monitor
```

### Bước 2: Chọn kịch bản kết nối mạng Wi-Fi và MQTT Broker

Khi nạp code lần đầu (chưa có Wi-Fi trong NVS) hoặc khi nhấn giữ nút **`BOOT (GPIO0)` trong 5 giây**, mạch ESP32-S3 sẽ phát mạng Wi-Fi Access Point có tên **`GSP-ESP32-XXXX`**:

- **Option 3 (AP Direct - Không cần Router ngoài):** Kết nối trực tiếp máy tính đang chạy Docker vào Wi-Fi `GSP-ESP32-XXXX`. Không cần cấu hình gì thêm, ESP32-S3 tự động gửi gói tin MQTT về địa chỉ máy tính `192.168.4.2:1883`.
    
- **Option 1 (Mạng Wi-Fi chung / Router / Hotspot điện thoại):** Kết nối vào Wi-Fi `GSP-ESP32-XXXX`, mở trình duyệt truy cập `http://192.168.4.1`, chọn **Option 1**, chọn tên Wi-Fi 2.4GHz, nhập mật khẩu rồi bấm **LƯU CẤU HÌNH** $\rightarrow$ **KHỞI ĐỘNG LẠI**. Khi vào mạng chung, ESP32-S3 tự động phát bản tin UDP Broadcast (cổng `8888`) để tìm IP của máy chủ Backend/MQTT.
    
- **Option 2 (Wi-Fi phát trực tiếp từ máy tính):** Mở `http://192.168.4.1`, chọn **Option 2** khi ESP32-S3 kết nối vào mạng Wi-Fi Hotspot phát ra từ chính máy tính đang chạy Docker. Mạch sẽ tự động lấy địa chỉ **IP Gateway** làm địa chỉ MQTT Broker mà không cần dò UDP.