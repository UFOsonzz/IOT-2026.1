# Hệ thống IoT Giám sát chuỗi cung ứng lạnh tuân thủ GSP

Đồ án học phần **IT4735 - IoT và Ứng dụng** - Đại học Bách Khoa Hà Nội.  
Hệ thống giám sát chuỗi cung ứng lạnh theo tiêu chuẩn Thực hành Tốt Bảo quản (**GSP - Good Storage Practices**) dành cho Vaccine (`+2.0°C` đến `+8.0°C`) và Thủy sản đông lạnh (`-20.0°C` đến `-40.0°C`).

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
├── infra/
│   ├── mosquitto/mosquitto.conf     # Cấu hình Eclipse Mosquitto
│   └── db/init.sql                  # Schema TimescaleDB, Hypertable và Trigger
├── backend/
│   ├── Dockerfile
│   ├── requirements.txt
│   └── app/
│       ├── main.py                  # REST API (chỉ đọc) và WebSocket
│       ├── db.py                    # Truy vấn LATERAL JOIN, Cursor Pagination và Verify Chain
│       └── mqtt_worker.py           # Duy trì MQTT Subscriber và Xác thực ECDSA
├── web/
│   ├── Dockerfile
│   ├── package.json
│   ├── vite.config.ts
│   ├── tsconfig.json
│   ├── index.html
│   └── src/                         # Giao diện React + TypeScript + TailwindCSS v4
├── firmware/
│   ├── platformio.ini               # Cấu hình ESP32-S3-N16R8
│   ├── include/secrets.example.h
│   └── src/main.cpp                 # Firmware FreeRTOS 2 Core chạy thông
└── tools/
    └── dummy_simulator.py           # Script giả lập thiết bị đẩy gói tin MQTT kiểm thử
```

---

## 3. Hướng dẫn chạy Toàn bộ Hệ thống

### Bước 1: Chuẩn bị file biến môi trường
Tại thư mục gốc của dự án, sao chép `.env.example` thành `.env`:

```bash
cp .env.example .env
```

### Bước 2: Khởi chạy
Chạy lệnh sau đây:

```bash
docker compose up -d --build
```

Kiểm tra trạng thái các dịch vụ:

```bash
docker compose ps
```

- **Giao diện Web Dashboard:** Truy cập [http://localhost:5173](http://localhost:5173)
- **Tài liệu API Backend - Swagger UI:** Truy cập [http://localhost:8000/docs](http://localhost:8000/docs)
- **MQTT Broker - Mosquitto:** Lắng nghe tại `localhost:1883`
- **TimescaleDB - PostgreSQL:** Lắng nghe tại `localhost:5432` (User: `gsp_user` / Pass: `gsp_pass_2026` / DB: `gsp_coldchain`)

---

## 4. Hướng dẫn Giả lập dữ liệu

Khi chưa cắm mạch phần cứng ESP32-S3, giảng viên và nhóm phát triển có thể chạy script giả lập để kiểm thử toàn bộ luồng: **MQTT → FastAPI → TimescaleDB → WebSocket → React UI**

```bash
# Cài đặt thư viện paho-mqtt trên máy host nếu chưa có
pip install paho-mqtt

# Chạy trình giả lập đẩy dữ liệu thời gian thực
python tools/dummy_simulator.py
```

Khi script chạy, mở trình duyệt tại `http://localhost:5173` để quan sát:
1. Trạng thái thiết bị chuyển sang **ONLINE** xanh lá.
2. Nhiệt độ, độ ẩm, điện áp pin cập nhật trực tiếp qua WebSocket mà không cần tải lại trang.
3. Khi có bản tin vượt ngưỡng (`> 8.0°C`) hoặc bản tin giả lập bị sửa đổi trái phép, giao diện lập tức nhấp nháy cảnh báo đỏ và gắn nhãn **TAMPERED**.
4. Nhấn nút **"Kiểm toán chuỗi chữ ký GSP"** trên góc phải để gọi API `/api/v1/devices/GSP-TRUCK-01/verify-chain` đối soát toàn bộ chuỗi bản ghi.

---

## 5. Hướng dẫn Biên dịch và Nạp Firmware cho ESP32

### Bước 1: Cấu hình thông tin Wi-Fi và IP máy chủ MQTT
Sao chép file mẫu `secrets.example.h` sang `secrets.h`:

```bash
cp firmware/include/secrets.example.h firmware/include/secrets.h
```

Mở `firmware/include/secrets.h` và cập nhật:
- `WIFI_SSID` / `WIFI_PASSWORD`: Tên và mật khẩu Wi-Fi tại phòng chấm đồ án.
- `MQTT_HOST`: Địa chỉ IPv4 trong mạng LAN của máy tính đang chạy Docker Mosquitto (xem bằng lệnh `ipconfig` trên Windows hoặc `ifconfig` / `ip a` trên macOS/Linux).

### Bước 2: Biên dịch và nạp xuống mạch ESP32-S3-N16R8
Sử dụng **PlatformIO CLI** hoặc tiện ích **PlatformIO IDE** trên VS Code:

```bash
cd firmware

# Biên dịch
pio run

# Nạp firmware xuống mạch và mở cổng Serial Monitor
pio run --target upload --target monitor
```

### Bước 3: Thao tác kiểm thử trực tiếp trên mạch phần cứng
- **Hoạt động bình thường:** Mỗi 10 giây, Core 0 tạo bản ghi nhiệt độ quanh mức `4.50°C`, băm SHA-256 bằng `mbedTLS` và chuyển sang Core 1 bắn lên topic `gsp/v1/GSP-TRUCK-01/telemetry`.
- **Kiểm thử Cảnh báo Vượt ngưỡng & Bật Relay (FR-04):** Nhấn giữ nút **BTN3 (`GPIO7`)**, nhiệt độ giả lập vọt lên `9.50°C` → Module Relay tại **`GPIO21`** đóng tiếp điểm bật Còi + Đèn ngay lập tức, đồng thời thẻ nhiệt độ trên Web Dashboard chuyển sang màu Đỏ nhấp nháy.
- **Kiểm thử Tắt tiếng còi tại chỗ (HMI Mute):** Nhấn nút **BTN2 (`GPIO6`)** để ngắt Relay còi báo tại chỗ.
- **Kiểm thử Di chúc LWT (`OFFLINE`):** Rút dây nguồn USB của mạch ESP32-S3 → Sau 30 giây Keep-Alive, Mosquitto Broker tự động phát bản tin Di chúc LWT chuyển trạng thái thiết bị trên Web Dashboard sang **OFFLINE (LWT)**.