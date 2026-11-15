# Kiến trúc hệ thống và giao thức truyền thông

Phụ trách: TV4

---

## 1. Sơ đồ kiến trúc 4 lớp

```mermaid
flowchart TB
    classDef sensing fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#0d47a1
    classDef edge fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#1b5e20
    classDef middleware fill:#fff3e0,stroke:#ef6c00,stroke-width:2px,color:#e65100
    classDef app fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px,color:#4a148c
    classDef thirdparty fill:#eceff1,stroke:#607d8b,stroke-width:2px,stroke-dasharray: 5 5,color:#263238
    classDef security fill:#ffebee,stroke:#c62828,stroke-width:2px,color:#b71c1c

    subgraph L1 ["LỚP 1: CẢM NHẬN & NGOẠI VI [TV1]"]
        direction LR
        SHT30["🌡️ Cảm biến SHT30 / SHT40"]:::sensing
        DS3231["⏱️ RTC DS3231"]:::sensing
        OLED["🖥️ OLED SSD1306"]:::sensing
        SDCARD["💾 MicroSD"]:::sensing
        BTN["🔘 3 Nút bấm HMI"]:::sensing
        BAT["🔋 Pin 18650 & Cầu phân áp"]:::sensing
        RELAY["🚨 Relay & Còi hú 5V"]:::sensing
    end

    subgraph L2 ["LỚP 2: MẠNG & BIÊN [TV1, TV4]"]
        direction TB
        subgraph ESP32S3 ["ESP32 / ESP32-S3"]
            CORE0["⚙️ Core 0: Thu thập, Xử lý biên & Mật mã<br/>- Đọc cảm biến chu kỳ<br/>- Bật còi/đèn khi vượt ngưỡng<br/>- Băm SHA-256 & Ký ECDSA"]:::edge
            CORE1["🔄 Core 1: Quản lý mạng & Auto-Sync<br/>- Quản lý WiFi & MQTT<br/>- Ghi đệm SD khi rớt mạng<br/>- Đẩy bù nền khi có mạng"]:::edge
        end
        WIFI_GW["📡 Router WiFi / Hotspot"]:::thirdparty
    end

    SHT30 -- "I2C" --> CORE0
    DS3231 -- "I2C" --> CORE0
    CORE0 -- "I2C" --> OLED
    BTN -- "GPIO Interrupt" --> CORE0
    BAT -- "ADC1" --> CORE0
    CORE0 -- "GPIO Out" --> RELAY
    CORE0 -- "Queue (Gói tin + Chữ ký)" --> CORE1
    CORE1 <-- "SPI" --> SDCARD
    CORE1 -- "WiFi & MQTTS" --> WIFI_GW

    subgraph L3 ["LỚP 3: MÁY CHỦ & TRUNG GIAN [TV2, TV4]"]
        direction TB
        BROKER["☁️ MQTT Broker (Mosquitto)<br/>- Topics: telemetry, sync, alert, status<br/>- QoS 1, Retained, LWT"]:::thirdparty
        
        subgraph BACKEND ["Backend (FastAPI)"]
            VERIFY["🛡️ Xác thực SHA-256 & ECDSA"]:::security
            DEDUP["🧹 Bộ khử trùng lặp"]:::middleware
            RULE["⚡ Bộ xử lý cảnh báo & API"]:::middleware
        end

        DB[("🗄️ TimescaleDB<br/>- Bảng telemetry_logs: Append-Only<br/>- Khóa cứng UPDATE & DELETE")]:::middleware
    end

    WIFI_GW -- "MQTTS QoS 1 (Luồng 1 chiều)" --> BROKER
    BROKER -- "Subscribe" --> VERIFY
    VERIFY -- "Gói tin hợp lệ" --> DEDUP
    DEDUP -- "INSERT" --> DB
    VERIFY -- "Vượt ngưỡng / Lỗi chữ ký" --> RULE
    DB -- "SELECT" --> RULE

    subgraph L4 ["LỚP 4: ỨNG DỤNG NGƯỜI DÙNG [TV3]"]
        direction LR
        WEB["💻 Web Dashboard<br/>- Giám sát thời gian thực<br/>- Biểu đồ vi khí hậu<br/>- Kiểm toán & Xuất báo cáo"]:::app
        TELE_API["🤖 Telegram Bot API"]:::thirdparty
        PHONE["📱 Điện thoại quản lý"]:::app
    end

    RULE -- "WebSocket & REST API" --> WEB
    RULE -- "HTTPS POST" --> TELE_API
    TELE_API -- "Push Notification" --> PHONE
```

---

## 2. Lựa chọn công nghệ và giao thức

| Thành phần | Lựa chọn | Phương án loại bỏ | Lý do kỹ thuật |
| :--- | :--- | :--- | :--- |
| **Thiết bị → Broker** | **MQTT** | HTTP REST, CoAP | Nhẹ, tối ưu cho mạng chập chờn; hỗ trợ QoS 1 đảm bảo đẩy bù không mất gói và LWT để phát hiện mất kết nối ngay lập tức. |
| **Luồng dữ liệu** | **Một chiều** | Điều khiển 2 chiều từ xa | Đảm bảo tính pháp lý GSP: thiết bị chỉ đọc và gửi, tuyệt đối không nhận lệnh điều khiển từ xa để tránh can thiệp ngoài ý muốn. |
| **Cơ sở dữ liệu** | **TimescaleDB** | MongoDB, MySQL | Tối ưu chuỗi thời gian qua Hypertable; hỗ trợ `ON CONFLICT (device_id, ts_rtc) DO NOTHING` khử trùng lặp và Trigger khóa cứng `UPDATE/DELETE`. |
| **Server → Web** | **REST API + WebSocket** | HTTP Polling | WebSocket đẩy dữ liệu và cảnh báo theo thời gian thực tới trình duyệt mà không cần client gửi request liên tục. |
