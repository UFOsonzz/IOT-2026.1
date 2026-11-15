# Hồ sơ bàn giao hệ thống

## Phần 1: Hướng dẫn dựng lại hệ thống

Yêu cầu môi trường Docker Compose và quy trình khởi chạy 4 container được trình bày tại `README.md`. Cấu hình biên dịch Firmware cho chip (ESP32 Classic / ESP32-S3) và cảm biến (SHT30 / SHT40) được quản lý qua `firmware/platformio.ini`.

## Phần 2: Sơ đồ kiến trúc và đấu nối phần cứng

- Sơ đồ kiến trúc 4 lớp: Xem tại `docs/01_technical_dossier/03_system_architecture.md`.
- Sơ đồ đấu nối phần cứng: Xem tại `docs/01_technical_dossier/04_hardware_wiring.md`.

## Phần 3: Danh sách thiết bị bàn giao

| Mã thiết bị | Tên thiết bị | Vị trí thử nghiệm | Vi điều khiển | Phiên bản Firmware |
| :--- | :--- | :--- | :--- | :--- |
| `GSP-TRUCK-01` | Thiết bị giám sát kho lạnh | Thùng bảo quản vaccine | ESP32 / ESP32-S3 | v1.1.0-sprint1 |

## Phần 4: Hướng dẫn vận hành

1. **Theo dõi dữ liệu:** Truy cập `http://localhost:5173`. Trạng thái thiết bị hiển thị `ONLINE`, thông số nhiệt ẩm cập nhật thời gian thực.
2. **Xử lý cảnh báo tại chỗ:** Khi còi hú phát tín hiệu do nhiệt độ ngoài dải 2.0°C đến 8.0°C, nhấn nút BTN2 (GPIO33 trên Classic / GPIO5 trên S3) để tắt còi.
3. **Cấu hình lại mạng WiFi:** Nhấn giữ nút BOOT (GPIO0) trong 5 giây. Kết nối vào mạng `GSP-ESP32-XXXX`, truy cập `http://192.168.4.1` để nhập thông tin WiFi mới.
4. **Kiểm toán dữ liệu:** Nhấn nút "Kiểm toán chuỗi chữ ký GSP" trên Web Dashboard để đối soát tính toàn vẹn dữ liệu.

## Phần 5: Xử lý sự cố thường gặp

| Hiện tượng | Nguyên nhân | Cách xử lý |
| :--- | :--- | :--- |
| Web hiển thị `OFFLINE` | Mất nguồn hoặc mất kết nối WiFi quá 30 giây | Kiểm tra nguồn cấp và điểm phát WiFi |
| Bảng nhật ký có nhãn đỏ `TAMPERED` | Dữ liệu bị sửa đổi trái phép trên đường truyền | Nhấn kiểm toán chuỗi băm để xác định gói tin bị can thiệp |
| Web hiển thị `WS Reconnecting...` | Backend khởi động lại hoặc cổng 8000 bị chặn | Kiểm tra container Backend bằng lệnh `docker compose ps` |

## Phần 6: Danh mục kiểm tra an toàn thông tin

- [x] 1. Không lưu cứng mật khẩu WiFi trong mã nguồn Git, quản lý qua NVS Flash.
- [x] 2. Kiến trúc dữ liệu một chiều: Thiết bị biên không mở cổng nhận lệnh điều khiển từ xa.
- [x] 3. Tính toán mã băm SHA-256 trên Canonical String tại thiết bị trước khi truyền.
- [ ] 4. Ký số ECDSA secp256r1 thực tế (Kế hoạch Sprint 2).
- [x] 5. Đánh số gói tin `seq_no` kết hợp nhãn thời gian `ts_rtc` chống Replay Attack.
- [x] 6. Khóa cứng thao tác UPDATE và DELETE trên bảng `telemetry_logs` bằng Trigger CSDL.
- [ ] 7. Ghi nhật ký kiểm toán vào bảng `audit_logs` (Kế hoạch Sprint 3).
- [x] 8. Giới hạn CORS chỉ cho phép các phương thức đọc trên REST API.
- [x] 9. Giới hạn số lượng bản ghi tối đa mỗi truy vấn chống tấn công DoS.
- [ ] 10. Xác thực người dùng trên Mosquitto Broker và kích hoạt TLS.
- [ ] 11. Đổi mật khẩu mặc định trong `.env` trước khi triển khai chính thức.
- [ ] 12. Thực hiện phép thử bàn giao với nhóm khác.
