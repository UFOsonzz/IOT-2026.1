# Nhật ký lỗi và bài học kinh nghiệm

Ghi nhận các sự cố kỹ thuật thực tế và giải pháp xử lý theo từng sprint.

---

## 1. Giai đoạn chạy thông luồng (Sprint 0)

| Mã lỗi | Hiện tượng | Nguyên nhân | Cách khắc phục | Bài học |
| :--- | :--- | :--- | :--- | :--- |
| **ERR-01** | ESP32 không tìm thấy Backend qua UDP Discovery | Backend chưa mở UDP listener cổng 8888 | Thêm `UdpDiscoveryProtocol` trong `backend/app/main.py`, mở cổng `8888/udp` trong `docker-compose.yml` | Luôn kiểm thử hai đầu Client và Server cho các giao thức tự tìm kiếm |
| **ERR-02** | Khởi động Docker báo lỗi xác thực TimescaleDB | Mật khẩu trong `.env.example` lệch với cấu hình mặc định | Đồng bộ mật khẩu `gsp_pass` cho `.env.example`, `docker-compose.yml` và `db.py` | Biến môi trường phải thống nhất xuyên suốt mọi dịch vụ |
| **ERR-03** | Lệch mã băm SHA-256 giữa C++ và Python | Định dạng số thực khác nhau khi ghép chuỗi băm | Thống nhất Canonical String định dạng `%.2f` cố định 2 chữ số thập phân | Băm trên chuỗi chuẩn hóa cố định, không băm JSON thô |

---

## 2. Giai đoạn cảm biến thực và phần cứng (Sprint 1)

| Mã lỗi | Hiện tượng | Nguyên nhân | Cách khắc phục | Bài học |
| :--- | :--- | :--- | :--- | :--- |
| **ERR-04** | Xung đột bus I2C giữa OLED, cảm biến và RTC | Task OLED cập nhật 500ms trùng thời điểm đọc cảm biến và RTC mỗi 2s | Tuần tự hóa truy cập bus I2C trong `PeripheralManager`, tránh gọi chồng chéo | Khi nhiều module dùng chung một bus I2C cần kiểm soát thời điểm và trạng thái bận |
| **ERR-05** | Nhận diện sai và nhảy phím khi bấm nút | Độ nảy cơ học của tiếp điểm nút bấm vật lý | Bổ sung bộ lọc Debounce 35ms và cờ khóa chống lặp khi giữ nút | Mọi nút bấm phần cứng đều phải có debounce xử lý rung phím |
| **ERR-06** | Sai số liệu nhiệt ẩm khi đổi sang SHT40 | SHT40 dùng lệnh đo 0xFD và công thức độ ẩm khác SHT30 | Bổ sung hàm `decodeSht40`, macro trong `app_config.h` và cờ biên dịch trong `platformio.ini` | Tách hàm giải mã theo Datasheet từng linh kiện, quản lý bằng build flags |
| **ERR-07** | Lỗi chân GPIO khi chuyển sang ESP32-S3 | ESP32-S3 không có chân GPIO34 và chân I2C mặc định khác Classic | Định nghĩa pinout theo macro `CONFIG_IDF_TARGET_ESP32S3` trong `app_config.h` | Quản lý chân cắm tập trung theo macro cấu hình chip mục tiêu |
