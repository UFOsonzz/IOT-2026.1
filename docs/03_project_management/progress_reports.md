# Báo cáo tiến độ dự án

Cập nhật kết quả thực tế, sự cố kỹ thuật và kế hoạch theo từng Sprint.

---

## Sprint 0: Walking Skeleton

**Thời gian:** Tuần 2 đến Tuần 7  
**Mục tiêu:** Xây dựng khung chạy thông End-to-End từ thiết bị biên đến Web Dashboard bằng dữ liệu giả lập.

### Kết quả đạt được

| Lớp | Thành phần | Kết quả |
| :--- | :--- | :--- |
| **Firmware** | `main.cpp`, `wifi_manager` | FreeRTOS dual-core: Core 0 đọc cảm biến giả lập và băm SHA-256; Core 1 quản lý WiFi, MQTT, LWT |
| **Infra** | `docker-compose.yml`, `init.sql`, `mosquitto.conf` | 4 container Docker chạy ổn định; kích hoạt Hypertable và Trigger bất biến trên TimescaleDB |
| **Backend** | `main.py`, `db.py`, `mqtt_worker.py` | FastAPI xử lý 4 topic MQTT, xác thực SHA-256, phân trang con trỏ, WebSocket, UDP Discovery |
| **Frontend** | `App.tsx`, TypeScript | Dashboard thời gian thực, bảng nhật ký đo, nhấp nháy cảnh báo và kiểm toán chuỗi băm |
| **Simulator** | `dummy_simulator.py`, `gui_simulator.py` | Kiểm thử thành công 5 kịch bản: Normal, Alert, Sync, Tamper, LWT |
| **Docs** | Technical Dossier và PM docs | Hoàn thành bộ hồ sơ kiến trúc, sơ đồ đấu nối, lược đồ CSDL và kế hoạch kiểm thử |

### Sự cố kỹ thuật đã xử lý

| Mã | Hiện tượng | Nguyên nhân | Cách khắc phục |
| :--- | :--- | :--- | :--- |
| ERR-01 | UDP Discovery không phản hồi | Backend chưa mở listener cổng 8888 | Bổ sung `UdpDiscoveryProtocol` và mở port `8888:8888/udp` trong Docker |
| ERR-02 | Lỗi xác thực TimescaleDB | Mật khẩu lệch giữa `.env.example` và mã nguồn | Đồng bộ mật khẩu `gsp_pass` cho toàn bộ hệ thống |
| ERR-03 | Lệch mã băm SHA-256 | Định dạng số thực khác nhau giữa C++ và Python | Chuẩn hóa Canonical String định dạng `%.2f` cố định 2 chữ số thập phân |

---

## Sprint 1: Cảm biến thực I2C và Mạch phần cứng

**Thời gian:** Tuần 8  
**Mục tiêu:** Tích hợp phần cứng thực tế gồm SHT30/SHT40, RTC DS3231, OLED SSD1306 và ngắt GPIO cho nút bấm trên cả ESP32 Classic và ESP32-S3.

### Kết quả đạt được

| PBI | Hạng mục | Trạng thái | Ghi chú |
| :--- | :--- | :--- | :--- |
| PBI-01 | Tích hợp SHT30 và SHT40 qua I2C | ✅ Done | Đọc địa chỉ 0x44, kiểm tra CRC-8, cấu hình linh hoạt qua `platformio.ini` |
| PBI-02 | Tích hợp RTC DS3231 qua I2C | ✅ Done | Đọc timestamp UTC từ địa chỉ 0x68, kiểm tra cờ OSF và định dạng BCD |
| PBI-03 | Màn hình OLED SSD1306 | ✅ Done | Hiển thị 2 trang: Thông số vi khí hậu và Trạng thái mạng; chuyển trang qua nút bấm |
| PBI-04 | Ngắt GPIO cho 3 nút bấm | ✅ Done | Bắt ngắt kèm Debounce 35ms: BTN1 đổi trang, BTN2 tắt còi, BTN3 giả lập quá nhiệt 9.5°C |

- **Phần cứng thực tế:** Mạch chạy ổn định trên cả ESP32 Classic và ESP32-S3 DevKit.
- **Giao tiếp I2C:** 3 module (SHT30/SHT40, DS3231, SSD1306) dùng chung 1 bus I2C không xung đột địa chỉ.
- **Luồng dữ liệu:** Gói tin đo từ cảm biến thực gửi lên Backend, vượt qua kiểm tra CRC-8 và SHA-256, hiển thị thời gian thực trên Web.

### Sự cố kỹ thuật đã xử lý

| Mã | Hiện tượng | Nguyên nhân | Cách khắc phục |
| :--- | :--- | :--- | :--- |
| ERR-04 | Xung đột bus I2C | Task OLED 500ms trùng thời điểm đọc cảm biến và RTC mỗi 2s | Tuần tự hóa truy cập bus I2C trong `PeripheralManager` |
| ERR-05 | Rung phím cơ học | Tiếp điểm nút bấm bị dội cơ học | Thêm bộ lọc Debounce 35ms và cờ khóa trạng thái chống lặp khi giữ phím |
| ERR-06 | Sai số liệu SHT40 | SHT40 dùng lệnh đo 0xFD và công thức độ ẩm khác SHT30 | Bổ sung hàm `decodeSht40` và cấu hình cờ chọn cảm biến trong `platformio.ini` |
| ERR-07 | Lỗi chân trên ESP32-S3 | ESP32-S3 không có GPIO34 và chân I2C mặc định khác Classic | Tách pinout theo macro `CONFIG_IDF_TARGET_ESP32S3` trong `app_config.h` |

### Đánh giá và Kế hoạch tiếp theo

- **Tiến độ:** ✅ Hoàn thành 100% mục tiêu Sprint 1, nghiệm thu đạt yêu cầu trên phần cứng thật.
- **Kế hoạch Sprint 2:** Triển khai lưu trữ đệm ra thẻ MicroSD khi mất mạng và chữ ký số ECDSA thật.
