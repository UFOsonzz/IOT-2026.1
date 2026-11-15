# Hồ sơ bàn giao hệ thống

---

## 1. Khởi tạo và cài đặt

- **Môi trường Server và Web:** Chi tiết quy trình chạy 4 Docker container được mô tả tại [README.md](../../README.md).
- **Biên dịch Firmware:** Cấu hình chip (ESP32 Classic / ESP32-S3) và cảm biến (SHT30 / SHT40) được quản lý qua `firmware/platformio.ini`.

---

## 2. Tài liệu kỹ thuật tham chiếu

- Kiến trúc hệ thống 4 lớp: [03_system_architecture.md](../01_technical_dossier/03_system_architecture.md).
- Sơ đồ đấu nối và gán chân: [04_hardware_wiring.md](../01_technical_dossier/04_hardware_wiring.md).

---

## 3. Danh sách thiết bị bàn giao

| Mã thiết bị | Tên thiết bị | Vị trí thử nghiệm | Vi điều khiển | Phiên bản Firmware |
| :--- | :--- | :--- | :--- | :--- |
| `GSP-TRUCK-01` | Thiết bị giám sát kho lạnh | Thùng bảo quản vaccine | ESP32 / ESP32-S3 | v1.1.0-sprint1 |

---

## 4. Hướng dẫn vận hành

1. **Giám sát dữ liệu:** Mở trình duyệt tại `http://localhost:5173`. Trạng thái thiết bị hiển thị `ONLINE`, dữ liệu nhiệt ẩm cập nhật theo thời gian thực.
2. **Xử lý báo động tại chỗ:** Khi còi kêu do nhiệt độ ra ngoài dải +2.0°C đến +8.0°C, nhấn nút BTN2 (GPIO33 trên Classic / GPIO5 trên S3) để tắt chuông.
3. **Cấu hình lại WiFi:** Nhấn giữ nút BOOT (GPIO0) trong 5 giây. Kết nối vào mạng `GSP-ESP32-XXXX`, truy cập `http://192.168.4.1` để nhập thông tin mạng mới.
4. **Kiểm toán dữ liệu:** Nhấn nút "Kiểm toán chuỗi chữ ký GSP" trên Web Dashboard để đối soát tính toàn vẹn của chuỗi dữ liệu.

---

## 5. Xử lý sự cố thường gặp

| Hiện tượng | Nguyên nhân | Cách xử lý |
| :--- | :--- | :--- |
| Web báo `OFFLINE` | Mất nguồn hoặc ngắt kết nối WiFi quá 30 giây | Kiểm tra nguồn cấp và điểm phát sóng WiFi |
| Bản ghi có nhãn đỏ `TAMPERED` | Dữ liệu bị can thiệp trái phép trên đường truyền | Nhấn nút kiểm toán để xác định gói tin bị sửa đổi |
| Web báo `WS Reconnecting...` | Backend khởi động lại hoặc cổng 8000 bị chặn | Kiểm tra trạng thái container Backend qua lệnh `docker compose ps` |

---

## 6. Checklist an toàn thông tin

- [x] 1. Không lưu cứng mật khẩu WiFi trong Git; quản lý cấu hình qua NVS Flash.
- [x] 2. Kiến trúc dữ liệu một chiều: Thiết bị biên không mở cổng nhận lệnh điều khiển từ xa.
- [x] 3. Tính mã băm SHA-256 trên Canonical String tại thiết bị trước khi gửi.
- [ ] 4. Ký số ECDSA secp256r1 thực tế (Kế hoạch Sprint 2).
- [x] 5. Đánh số gói `seq_no` kết hợp timestamp `ts_rtc` chống Replay Attack.
- [x] 6. Khóa cứng thao tác UPDATE và DELETE trên bảng `telemetry_logs` bằng Trigger CSDL.
- [ ] 7. Ghi nhật ký kiểm toán vào bảng `audit_logs` (Kế hoạch Sprint 3).
- [x] 8. Giới hạn CORS chỉ cho phép các phương thức đọc trên REST API.
- [x] 9. Giới hạn số lượng bản ghi tối đa mỗi truy vấn để chống DoS.
- [ ] 10. Bật xác thực người dùng trên Mosquitto Broker và kích hoạt TLS.
- [ ] 11. Đổi mật khẩu mặc định trong `.env` trước khi bàn giao thực tế.
- [ ] 12. Thực hiện kiểm thử bàn giao chéo với nhóm khác.
