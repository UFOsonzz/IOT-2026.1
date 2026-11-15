# Biên bản họp nhóm

---

### Buổi họp 1: Nghiệm thu Sprint 1

- **Thời gian:** 2026-11-15
- **Địa điểm:** Phòng lab
- **Thành viên tham dự:** TV1, TV2, TV3, TV4 (đầy đủ)
- **Nội dung cuộc họp:**
  1. **Kết quả hoàn thành:**
     - Lắp ráp mạch thực tế gồm OLED SSD1306, RTC DS3231, cảm biến SHT30 và SHT40.
     - Firmware hoạt động ổn định trên cả ESP32 Classic và ESP32-S3.
     - Dữ liệu đo gửi thành công lên Backend qua MQTT và hiển thị thời gian thực trên Web Dashboard.
  2. **Kế hoạch Sprint 2:**
     - Triển khai ghi đệm dữ liệu ra thẻ MicroSD khi mất mạng.
     - Thay thế chữ ký dummy bằng chữ ký số ECDSA secp256r1 thật.
  3. **Sự cố kỹ thuật đã xử lý:**
     - Xung đột bus I2C giữa OLED và cảm biến: Giải quyết bằng tuần tự hóa truy cập trong `PeripheralManager`.
     - Hiện tượng dội phím nút bấm: Áp dụng bộ lọc Debounce 35ms.
- **Kết luận:**
  - Nghiệm thu hoàn thành Sprint 1, tiến hành merge nhánh `feat/sprint1-esp32` vào `main`.
