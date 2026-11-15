# Báo cáo tiến độ dự án

Cập nhật sau mỗi Sprint, ghi nhận kết quả thực tế, sự cố kỹ thuật và kế hoạch tiếp theo.

---

## Sprint 0: Walking Skeleton

**Thời gian:** Tuần 2 đến Tuần 7  
**Mục tiêu:** Xây dựng khung chạy thông End-to-End từ thiết bị biên đến Web Dashboard bằng dữ liệu giả lập.

### Kết quả đạt được

| Lớp | Thành phần | Kết quả |
| :--- | :--- | :--- |
| **Firmware** | `main.cpp`, `wifi_manager` | FreeRTOS dual-core: Core 0 đọc cảm biến và băm SHA-256; Core 1 quản lý WiFi, MQTT, LWT |
| **Infra** | `docker-compose.yml`, `init.sql`, `mosquitto.conf` | 4 Docker container hoạt động ổn định; TimescaleDB kích hoạt Hypertable và Trigger bất biến |
| **Backend** | `main.py`, `db.py`, `mqtt_worker.py` | FastAPI xử lý 4 topic MQTT, kiểm tra SHA-256, Cursor Pagination, WebSocket, UDP Discovery |
| **Frontend** | `App.tsx`, TypeScript | Dashboard thời gian thực, bảng nhật ký đo, nhấp nháy cảnh báo, kiểm toán chuỗi băm |
| **Simulator** | `dummy_simulator.py`, `gui_simulator.py` | Kiểm thử thành công 5 kịch bản: Normal, Alert, Sync, Tamper, LWT |
| **Docs** | Technical Dossier và PM docs | Hoàn thành bộ hồ sơ kiến trúc, sơ đồ mạch, lược đồ CSDL, kế hoạch kiểm thử |

### Lỗi đã gặp và khắc phục

| Lỗi | Nguyên nhân | Cách sửa |
| :--- | :--- | :--- |
| ERR-01: UDP Discovery không phản hồi | Backend chưa mở listener cổng 8888 | Thêm `UdpDiscoveryProtocol` và mở port `8888:8888/udp` trong Docker |
| ERR-02: Xác thực TimescaleDB thất bại | Mật khẩu lệch giữa `.env.example` và mã nguồn | Đồng bộ mật khẩu `gsp_pass` cho toàn bộ hệ thống |
| ERR-03: Lệch mã băm SHA-256 | Định dạng số thực khác nhau giữa C++ và Python | Chuẩn hóa Canonical String định dạng `%.2f` |

---

## Sprint 1: Cảm biến thực I2C và Mạch phần cứng

**Thời gian:** Tuần 8  
**Mục tiêu:** Tích hợp phần cứng thực tế gồm cảm biến nhiệt ẩm SHT30/SHT40, IC RTC DS3231, màn hình OLED SSD1306 và ngắt GPIO cho nút bấm. Hỗ trợ cả ESP32 Classic và ESP32-S3.

### Kết quả đạt được

| PBI | Hạng mục | Trạng thái | Ghi chú |
| :--- | :--- | :--- | :--- |
| PBI-01 | Tích hợp SHT30 và SHT40 qua I2C | ✅ Done | Đọc địa chỉ 0x44, kiểm tra CRC-8, hỗ trợ cấu hình linh hoạt SHT30 hoặc SHT40 |
| PBI-02 | Tích hợp RTC DS3231 qua I2C | ✅ Done | Lấy mốc thời gian UTC từ địa chỉ 0x68, kiểm tra cờ OSF, định dạng BCD và ngày hợp lệ |
| PBI-03 | Màn hình OLED SSD1306 | ✅ Done | Hiển thị 2 trang: Thông số đo môi trường và Trạng thái mạng |
| PBI-04 | Ngắt GPIO cho nút bấm | ✅ Done | Bắt ngắt CHANGE với Debounce 35ms: BTN1 đổi trang, BTN2 tắt còi, BTN3 giả lập quá nhiệt |

* **Phần cứng thực tế:** Lắp ráp mạch thành công, chạy ổn định trên cả ESP32 Classic và ESP32-S3 DevKit.
* **Giao tiếp I2C:** 3 module (SHT30/SHT40, DS3231, SSD1306) dùng chung 1 bus I2C, không xung đột địa chỉ.
* **Luồng dữ liệu:** Gói tin đo thực tế gửi lên Backend, vượt qua kiểm tra CRC-8 và SHA-256, hiển thị thời gian thực trên Web Dashboard.

### Lỗi đã gặp và khắc phục

| Lỗi | Nguyên nhân | Cách sửa |
| :--- | :--- | :--- |
| ERR-04: Xung đột bus I2C | OLED cập nhật 500ms trùng thời điểm đọc cảm biến và RTC | Điều phối tuần tự hóa truy cập bus I2C trong `PeripheralManager` |
| ERR-05: Nhiễu tiếp điểm nút bấm | Độ nảy cơ học khi nhấn nút vật lý | Áp dụng Debounce 35ms và khóa chống lặp khi giữ nút |
| ERR-06: Sai lệch dữ liệu SHT40 | SHT40 dùng command đo 0xFD và công thức độ ẩm khác SHT30 | Bổ sung hàm `decodeSht40` và cấu hình cờ chọn cảm biến trong `platformio.ini` |
| ERR-07: Lỗi gán chân ESP32-S3 | ESP32-S3 không có GPIO34 và chân I2C mặc định khác Classic | Tách Pinout theo macro phần cứng `CONFIG_IDF_TARGET_ESP32S3` |

### Đánh giá và Kế hoạch tiếp theo

* **Tiến độ:** ✅ Hoàn thành toàn bộ mục tiêu Sprint 1, kiểm thử thực tế đạt yêu cầu.
* **Kế hoạch Sprint 2:** Triển khai lưu trữ thẻ MicroSD khi mất mạng và cơ chế ký số ECDSA.
