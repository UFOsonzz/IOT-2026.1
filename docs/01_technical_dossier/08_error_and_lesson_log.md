# Nhật ký lỗi và bài học

Tài liệu ghi nhận các lỗi kỹ thuật phát sinh và giải pháp xử lý theo từng giai đoạn phát triển.

## 1. Giai đoạn chạy thông luồng (Sprint 0)

| Mã lỗi | Hiện tượng | Nguyên nhân | Cách khắc phục | Bài học |
| :--- | :--- | :--- | :--- | :--- |
| **ERR-01** | ESP32 không tìm thấy Backend qua UDP Discovery | Backend thiếu UDP listener tại cổng 8888 | Bổ sung `UdpDiscoveryProtocol` trong `backend/app/main.py`, mở cổng `8888/udp` trong `docker-compose.yml` | Kiểm thử giao thức Auto-Discovery ở cả Client và Server |
| **ERR-02** | Khởi chạy Docker báo lỗi xác thực TimescaleDB | Mật khẩu trong `.env.example` lệch với cấu hình mặc định | Đồng bộ mật khẩu `gsp_pass` giữa `.env.example`, `docker-compose.yml` và `db.py` | Cấu hình biến môi trường phải đồng nhất giữa các dịch vụ |
| **ERR-03** | Lệch mã băm SHA-256 giữa C++ và Python | Khác biệt định dạng số thực khi tạo chuỗi băm | Thống nhất Canonical String: `<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>` với 2 chữ số thập phân | Băm trên chuỗi chuẩn hóa cố định, không băm JSON thô |

## 2. Giai đoạn cảm biến thực và mạch phần cứng (Sprint 1)

| Mã lỗi | Hiện tượng | Nguyên nhân | Cách khắc phục | Bài học |
| :--- | :--- | :--- | :--- | :--- |
| **ERR-04** | Xung đột bus I2C giữa OLED, cảm biến và RTC | Task OLED cập nhật 500ms trùng thời điểm task đọc cảm biến và RTC mỗi 2s | Điều phối tuần tự hóa truy cập bus I2C trong `PeripheralManager`, tránh gọi chồng chéo | Chia sẻ bus I2C giữa nhiều thiết bị cần kiểm soát thời gian và trạng thái bận |
| **ERR-05** | Nhiễu tín hiệu và nhận diện sai khi bấm nút | Độ nảy cơ học của tiếp điểm nút bấm vật lý | Thêm bộ lọc Debounce 35ms và cờ khóa trạng thái chống lặp khi giữ nút | Bắt buộc xử lý Debounce cho mọi nút bấm điều khiển |
| **ERR-06** | Dữ liệu nhiệt ẩm sai khi đổi cảm biến sang SHT40 | SHT40 dùng command đo 0xFD và công thức độ ẩm khác SHT30 | Bổ sung hàm `decodeSht40`, macro chọn cảm biến trong `app_config.h` và cấu hình cờ `-D SENSOR_TYPE_SHT40` / `SHT30` trong `platformio.ini` | Tách riêng hàm giải mã theo Datasheet từng dòng cảm biến và quản lý cấu hình bằng build flags |
| **ERR-07** | Lỗi gán chân khi chuyển giữa ESP32 Classic và ESP32-S3 | ESP32-S3 không có GPIO34 và chân I2C mặc định khác ESP32 Classic | Định nghĩa sơ đồ chân theo macro mục tiêu phần cứng `CONFIG_IDF_TARGET_ESP32S3` trong `app_config.h` | Quản lý Pinout tập trung theo cấu hình Board |
