# Hệ thống IoT Giám sát chuỗi cung ứng lạnh tuân thủ GSP

1. Đồ án học phần **IT4735 - IoT và Ứng dụng** - Đại học Bách Khoa Hà Nội.  
2. Hệ thống giám sát chuỗi cung ứng lạnh theo tiêu chuẩn Thực hành Tốt Bảo quản (**GSP - Good Storage Practices**) dành cho Vaccine (`+2.0°C` đến `+8.0°C`) và Thủy sản đông lạnh (`-20.0°C` đến `-40.0°C`).

---

## 1. Tổng quan kiến trúc

1. **Kiến trúc Một chiều:** Thiết bị biên ESP32-S3 chỉ đọc cảm biến, băm SHA-256, ký số ECDSA và đẩy dữ liệu lên Broker và **KHÔNG** mở kênh nhận lệnh điều khiển từ xa nhằm đảm bảo tính chống chối bỏ.
2. **Dữ liệu Chuỗi thời gian Bất biến (Append-Only và Immutable):** Bảng `telemetry_logs` trên TimescaleDB được khóa cứng thao tác `UPDATE` và `DELETE` ở tầng Engine của cơ sở dữ liệu.
3. **Đồng bộ bù Không trùng lặp:** Kết hợp con trỏ `sync_byte_offset` tại thiết bị và ràng buộc khóa chính kép `ON CONFLICT (device_id, ts_rtc) DO NOTHING` tại CSDL.
4. **Đa nhiệm 2 Lõi trên ESP32-S3:**
   - **Core 0:** Đọc cảm biến, kiểm tra ngưỡng GSP, kích hoạt Relay để bật còi và đèn tại chỗ và băm SHA-256 phần cứng.
   - **Core 1:** Duy trì kết nối Wi-Fi, MQTT Keep-Alive + Di chúc LWT và đẩy dữ liệu qua Queue.

---

## 2. Cấu trúc Thư mục

```text
IOT-2026.1/
├── .gitignore
├── .env.example
├── docker-compose.yml
├── README.md
├── run_simulator_mac.sh             # Script tự động chạy Simulator trên macOS/Linux
├── run_simulator_win.bat            # Script tự động chạy Simulator trên Windows
├── docs/
│   ├── architecture.md              # Sơ đồ kiến trúc 4 lớp
│   └── hardware_wiring.md           # Sơ đồ đấu nối và bảng gán chân ESP32-S3
├── infra/
│   ├── mosquitto/mosquitto.conf     # Cấu hình Eclipse Mosquitto
│   └── db/init.sql                  # Schema TimescaleDB, Hypertable và Trigger
├── backend/
│   ├── Dockerfile
│   ├── requirements.txt
│   └── app/
│       ├── main.py                  # REST API (chỉ đọc), WebSocket & UDP Discovery
│       ├── db.py                    # Truy vấn LATERAL JOIN, Cursor Pagination và Verify Chain
│       └── mqtt_worker.py           # Duy trì MQTT Subscriber và Xác thực chữ ký
├── web/
│   ├── Dockerfile
│   ├── package.json
│   ├── vite.config.ts
│   ├── tsconfig.json
│   ├── index.html
│   └── src/                         # Giao diện React + TypeScript + TailwindCSS v4
├── firmware/
│   ├── platformio.ini               # Cấu hình ESP32-S3-N16R8
│   ├── include/WifiConfig.h         # Quản lý Wi-Fi
│   └── src/main.cpp                 # Firmware chính
└── tools/
    └── dummy_simulator.py           # Script giả lập thiết bị đẩy gói tin MQTT kiểm thử
```

---

## 3. Hướng dẫn chạy Toàn bộ Hệ thống

### 3.1. Điều kiện tiên quyết

Để chạy trọn vẹn toàn bộ hệ sinh thái của đồ án, máy tính cần đáp ứng các yêu cầu sau:

1. **Docker & Docker Compose:** Đã cài đặt [Docker Desktop](https://www.docker.com/) hoặc Docker Engine + Docker Compose Plugin v2 (Linux) và Docker Daemon đang hoạt động.
2. **Kiểm tra giải phóng các cổng mạng trên máy, tránh tình trạng Port conflict:**
   - `1883`: MQTT Broker
   - `5432`: Database
   - `8000`: Backend REST API và WebSocket
   - `5173`: Frontend Web Dashboard
3. **Nguồn phát dữ liệu IoT:**
   > Lệnh `docker compose up` chỉ khởi chạy *tầng Nền tảng Server và Web* (Broker + DB + Backend + Frontend). Khi vừa chạy Docker, hệ thống ở trạng thái **chờ**, thiết bị sẽ hiển thị `OFFLINE` trên Web, chưa có đồ thị và thông số đo đạc.
   > Để có dữ liệu chạy thông toàn bộ luồng End-to-End, cần 1 trong 2 nguồn dữ liệu:
   > - **Cách A (test nhanh hoặc demo):** Cài sẵn **Python 3.8+** trên máy host để chạy script Simulator (xem [Mục 4](#4-hướng-dẫn-chạy-giả-lập-dữ-liệu)).
   > - **Cách B (Môi trường thực tế có phần cứng):** Cần mạch **ESP32-S3-N16R8**, cáp kết nối USB Type-C và tiện ích **PlatformIO** (xem [Mục 5](#5-hướng-dẫn-biên-dịch-và-nạp-firmware-cho-esp32)).

---

### 3.2. Khởi chạy Server và Web

#### Bước 1: Chuẩn bị file biến môi trường
Tại thư mục gốc của dự án, sao chép `.env.example` thành `.env`:

```bash
cp .env.example .env
```

Mặc định file `.env.example` đã điền sẵn các tham số cấu hình phù hợp để chạy ngay. Có thể tùy biến `POSTGRES_USER`, `POSTGRES_PASSWORD` hoặc `POSTGRES_DB`, và cần cập nhật tương ứng biến `DATABASE_URL` theo cú pháp: `postgresql://<POSTGRES_USER>:<POSTGRES_PASSWORD>@timescaledb:5432/<POSTGRES_DB>`

#### Bước 2: Build và Khởi chạy các container
Chạy lệnh sau tại thư mục gốc của dự án:

```bash
docker compose up -d --build
```

#### Bước 3: Kiểm tra trạng thái các dịch vụ
Chạy lệnh:

```bash
docker compose ps
```

Nếu thành công, cả 4 container sẽ có trạng thái `Up` (hoặc `healthy` đối với database):

| Tên Container | Dịch vụ | Cổng ánh xạ | Trạng thái mong đợi |
| :--- | :--- | :--- | :--- |
| `gsp-mosquitto` | MQTT Message Broker | `localhost:1883` | Up |
| `gsp-timescaledb` | CSDL TimescaleDB / PG16 | `localhost:5432` | Up (healthy) |
| `gsp-backend` | FastAPI REST + WebSocket + MQTT Worker | `localhost:8000` | Up |
| `gsp-web` | React + TypeScript Dashboard | `localhost:5173` | Up |

#### Bước 4: Truy cập các điểm dịch vụ
- **Giao diện Web Dashboard:** [http://localhost:5173](http://localhost:5173)
- **Tài liệu API Backend (Swagger UI):** [http://localhost:8000/docs](http://localhost:8000/docs)
- **Địa chỉ kết nối MQTT Broker:** `localhost:1883`
- **Địa chỉ kết nối TimescaleDB:** `localhost:5432`

---

### 3.3. Kích hoạt Luồng Dữ liệu IoT

Sau khi kiểm tra 4 container đã hoạt động, hãy chọn một trong hai phương thức sau để cấp dữ liệu cho hệ thống:

1. **Chạy Giả lập kiểm thử trên máy tính:** Chuyển sang **[Mục 4. Hướng dẫn chạy giả lập dữ liệu](#4-hướng-dẫn-chạy-giả-lập-dữ-liệu)**. Script sẽ tự động phát sinh bản tin chuẩn GSP, bản tin cảnh báo vượt ngưỡng, bản tin tấn công giả mạo (TAMPERED), và đẩy bù dữ liệu Offline (MicroSD Sync).
2. **Chạy cùng mạch phần cứng ESP32-S3:** Chuyển sang **[Mục 5. Hướng dẫn Biên dịch và Nạp Firmware cho ESP32](#5-hướng-dẫn-biên-dịch-và-nạp-firmware-cho-esp32)**. Mạch sẽ đọc cảm biến, ký số và gửi dữ liệu thực tế về Broker qua Wi-Fi.

---

### 3.4. Các lệnh Quản trị trong Docker thường dùng

- Xem toàn bộ log thời gian thực:
  ```bash
  docker compose logs -f
  ```
- Xem log riêng của từng dịch vụ (ví dụ backend hoặc broker):
  ```bash
  docker compose logs -f backend
  docker compose logs -f mosquitto
  ```
- Khởi động lại toàn bộ hoặc một dịch vụ:
  ```bash
  docker compose restart
  # hoặc chỉ restart backend
  docker compose restart backend
  ```
- Dừng toàn bộ hệ thống (giữ nguyên dữ liệu CSDL):
  ```bash
  docker compose down
  ```
- Dừng và Xóa sạch dữ liệu (Reset CSDL về trạng thái ban đầu):
  ```bash
  docker compose down -v
  ```

---

## 4. Hướng dẫn chạy giả lập dữ liệu

Khi chưa cắm mạch phần cứng ESP32-S3, giảng viên và nhóm phát triển có thể chạy script giả lập để kiểm thử toàn bộ luồng: **MQTT → FastAPI → TimescaleDB → WebSocket → React UI**

Để tránh xung đột thư viện với hệ điều hành, dự án đã đóng gói sẵn các script tự động thiết lập môi trường ảo `.venv`, cài đặt `paho-mqtt` và khởi chạy giả lập.

**Đối với Windows,** mở Command Prompt hoặc PowerShell tại thư mục gốc của dự án và chạy:
```cmd
run_simulator_win.bat
```
hoặc click đúp chuột trực tiếp vào file `run_simulator_win.bat` trong File Explorer

**Đối với macOS / Linux,** mở Terminal tại thư mục gốc của dự án và chạy:

```bash
# Cấp quyền thực thi, chỉ cần chạy 1 lần
chmod +x run_simulator_mac.sh
# Khởi chạy
./run_simulator_mac.sh
```

Khi script chạy, mở trình duyệt tại `http://localhost:5173` để quan sát:
1. Trạng thái thiết bị chuyển sang **ONLINE** xanh lá.
2. Nhiệt độ, độ ẩm, điện áp pin cập nhật trực tiếp qua WebSocket.
3. Khi có bản tin vượt ngưỡng (`> 8.0°C`) hoặc bản tin giả lập bị sửa đổi trái phép, giao diện sẽ nhấp nháy cảnh báo đỏ và gắn nhãn **TAMPERED**.
4. Nhấn nút **"Kiểm toán chuỗi chữ ký GSP"** trên góc phải để gọi API `/api/v1/devices/<DEVICE_ID>/verify-chain` đối soát toàn bộ chuỗi bản ghi.

---

## 5. Hướng dẫn Biên dịch và Nạp Firmware cho ESP32

### Bước 1: Biên dịch và nạp xuống mạch ESP32-S3-N16R8
Firmware đã tích hợp sẵn cơ chế quản lý mạng linh hoạt trong `firmware/include/WifiConfig.h` (lưu cấu hình vào bộ nhớ NVS Flash, không cần sửa cứng SSID/IP trong mã nguồn). Sử dụng **PlatformIO CLI** hoặc **PlatformIO IDE** trên VS Code:

```bash
cd firmware

# Biên dịch
pio run

# Nạp firmware xuống mạch và mở cổng Serial Monitor
pio run --target upload --target monitor
```

### Bước 2: Chọn kịch bản kết nối mạng Wi-Fi và MQTT Broker

Khi mới nạp code hoặc nhấn giữ nút `BOOT (GPIO0)` trong 5 giây, mạch ESP32-S3 sẽ phát mạng Wi-Fi riêng có tên `GSP-ESP32-XXXX`:

- **Option 3 (AP Direct - Không cần Router ngoài):** Kết nối trực tiếp máy tính chạy Docker vào Wi-Fi `GSP-ESP32-XXXX`. Không cần cấu hình gì thêm, ESP32-S3 tự động gửi gói tin MQTT về địa chỉ máy tính `192.168.4.2:1883`.
    
- **Option 1 (Mạng Wi-Fi chung):** Kết nối vào Wi-Fi `GSP-ESP32-XXXX`, mở trình duyệt truy cập `http://192.168.4.1`, chọn **Option 1**, chọn tên Wi-Fi 2.4GHz và lưu lại. Sau khi khởi động lại, ESP32-S3 tự động phát bản tin UDP Broadcast (cổng `8888`) để tìm địa chỉ IP của máy chủ Backend/MQTT trong mạng LAN (hoặc dùng IP Broker nhập thủ công nếu có).
    
- **Option 2 (Wi-Fi phát từ máy tính):** Mở `http://192.168.4.1`, chọn **Option 2** khi ESP32-S3 kết nối vào mạng Wi-Fi Hotspot phát ra từ máy tính đang chạy Docker (mạch tự động lấy IP Gateway làm địa chỉ MQTT Broker).
