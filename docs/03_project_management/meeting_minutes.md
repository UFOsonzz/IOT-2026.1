# Nhật ký họp nhóm

---

### Buổi họp 1: Đánh giá và nghiệm thu Sprint 1

- **Thời gian:** 2026-11-15
- **Hình thức:** Trực tiếp tại phòng lab
- **Tham dự:** Đầy đủ các thành viên (TV1, TV2, TV3, TV4)
- **Nội dung:**
  1. *Đã hoàn thành:*
     - Lắp ráp mạch hoàn chỉnh với màn hình OLED SSD1306, IC RTC DS3231, cảm biến SHT30 và SHT40.
     - Cấu hình Firmware chạy tương thích cả ESP32 Classic và ESP32-S3.
     - Dữ liệu đo thực tế đã gửi thành công lên Backend qua MQTT và hiển thị trên Web Dashboard.
  2. *Kế hoạch tiếp theo (Sprint 2):*
     - Triển khai lưu đệm dữ liệu ra thẻ MicroSD khi mất kết nối mạng.
     - Thay thế chữ ký dummy bằng chữ ký số ECDSA secp256r1 thực tế.
  3. *Vướng mắc và giải pháp:*
     - Lỗi xung đột bus I2C giữa OLED và cảm biến đã được xử lý bằng tuần tự hóa truy cập trong Firmware.
     - Hiện tượng dội phím cơ học được khắc phục bằng bộ lọc Debounce 35ms.
- **Quyết định nhóm:**
  - Chốt hoàn thành Sprint 1, merge nhánh `feat/sprint1-esp32` vào `main`.
