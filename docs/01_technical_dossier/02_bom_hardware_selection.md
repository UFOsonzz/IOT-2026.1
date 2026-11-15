# Lựa chọn cảm biến và danh mục vật tư

Phụ trách: TV1

---

## 1. Đối chiếu đại lượng đo và linh kiện

| Đại lượng đo | Linh kiện | Dải đo và Sai số | Giao thức | Đáp ứng yêu cầu |
| :--- | :--- | :--- | :--- | :--- |
| Nhiệt độ kho lạnh | SHT30 hoặc SHT40 | -40°C đến +125°C, sai số ±0.2°C (SHT40: ±0.1°C) | I2C (0x44) | Đáp ứng NFR-03 cho dược phẩm |
| Độ ẩm tương đối | SHT30 hoặc SHT40 | 0% đến 100% RH, sai số ±2.0% RH (SHT40: ±1.5% RH) | I2C (0x44) | Đáp ứng NFR-03 |
| Thời gian thực | IC RTC DS3231SN | Sai số ±2ppm (~1 phút/năm), bù nhiệt TCXO | I2C (0x68) | Timestamp UTC chuẩn khi mất mạng |
| Điện áp pin | Cầu phân áp 2 trở 100kΩ, tụ 100nF | Chia đôi 3.0V..4.2V về 1.5V..2.1V | ADC1 | Giám sát pin duy trì ≥ 8 giờ (NFR-04) |
| Báo động tại chỗ | Relay 1 kênh + Còi 5V | Đóng cắt 5V DC, cách ly quang | Digital Out | Độ trễ còi ≤ 3 giây (NFR-01) |

> **Cấu hình cảm biến:** SHT30 và SHT40 dùng chung địa chỉ I2C `0x44` và sơ đồ chân. Chọn loại cảm biến trong `firmware/platformio.ini` qua cờ: `build_flags = -D SENSOR_TYPE_SHT40` hoặc `-D SENSOR_TYPE_SHT30`.

---

## 2. Danh mục vật tư

| STT | Linh kiện | Thông số kỹ thuật | SL | Đơn giá (VNĐ) | Thành tiền (VNĐ) | Ghi chú |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | ESP32 / ESP32-S3 | Dual-core 240MHz, WiFi, BLE | 1 | 110.000 | 110.000 | Chọn môi trường `esp32dev` hoặc `esp32s3` |
| 2 | Cảm biến SHT30 / SHT40 | I2C, vỏ bọc chống ẩm | 1 | 100.000 | 100.000 | Cấu hình qua `platformio.ini` |
| 3 | Màn hình OLED SSD1306 0.96 inch | 128x64, giao tiếp I2C | 1 | 30.000 | 30.000 | Hiển thị thông số tại chỗ |
| 4 | Module MicroSD + RTC DS3231 | SPI (SD) + I2C (DS3231 kèm pin CR2032) | 1 | 100.000 | 100.000 | Lưu đệm offline tối thiểu 7 ngày |
| 5 | Module Relay 1 kênh 5V | Kích mức HIGH, có diode chống ngược | 1 | 25.000 | 25.000 | Cách ly tải còi hú khỏi vi điều khiển |
| 6 | Còi hú 5V và đèn báo | Nguồn 5V DC, âm lượng lớn | 1 | 20.000 | 20.000 | Cảnh báo tức thời tại hiện trường |
| 7 | Mạch sạc và pin Li-ion 18650 | Dung lượng ≥ 2600mAh, mạch boost 5V | 1 | 100.000 | 100.000 | Duy trì hoạt động ≥ 8 giờ khi mất điện |
| 8 | Linh kiện phụ | Vỏ hộp IP65, 3 nút bấm, trở, tụ | 1 | 50.000 | 50.000 | Đấu nối và bảo vệ mạch |
| **Tổng** | | | | | **535.000** | |
