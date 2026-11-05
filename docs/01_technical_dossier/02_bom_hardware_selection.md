# (2) Lựa Chọn Cảm Biến Và Danh Mục Vật Tư
*(Tài liệu thuộc Bước 2 và Mục 5.3.1 - Phụ trách chính: TV1)*

## 1. Bảng đối chiếu đại lượng cần đo với cảm biến và cơ cấu chấp hành

| Đại lượng đo | Linh kiện | Dải đo và Độ chính xác | Giao thức | Đối chiếu yêu cầu phi chức năng |
| :--- | :--- | :--- | :--- | :--- |
| Nhiệt độ kho lạnh | Cảm biến **SHT30** | `-40°C .. +125°C`, sai số `+-0.2°C` | I2C (`0x44`) | Đáp ứng NFR-03 và môi trường kho lạnh ẩm. |
| Độ ẩm tương đối (RH) | Cảm biến **SHT30** | `0% .. 100% RH`, sai số `+-2.0% RH` | I2C (`0x44`) | Đáp ứng NFR-03; có màng PE chống đọng sương. |
| Nhãn thời gian thực | IC RTC **DS3231SN** | Sai số `+-2ppm` (tương đương ~1 phút/năm), có bù nhiệt TCXO | I2C (`0x68`) | Đảm bảo dấu thời gian chính xác khi mất mạng. |
| Điện áp Pin dự phòng 18650 | Cầu phân áp `2x 100kOhm` và tụ `100nF` | Chia đôi áp `3.0V..4.2V` về `1.5V..2.1V` | `ADC1_CH3` (`GPIO4`) | Giám sát dung lượng pin đảm bảo chạy `>= 8 giờ` (NFR-04). |
| Báo động tại chỗ | Module Relay 1 kênh + Còi và Đèn | Đóng cắt tải 5V DC, cách ly chống dòng ngược | Digital Out (`GPIO21`) | Đáp ứng độ trễ bật còi tại chỗ `<= 3 giây` (NFR-01). |

## 2. Danh mục vật tư dự kiến kèm thông số quyết định và chi phí

| STT | Tên linh kiện | Thông số kỹ thuật quyết định | SL | Đơn giá (VNĐ) | Thành tiền (VNĐ) | Lý do lựa chọn & Nguồn cung |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | Vi điều khiển ESP32-S3-DevKitC-1 | Module N16R8 (16MB Flash, 8MB PSRAM), Dual-core 240MHz | 1 | 180.000 | 180.000 | Chạy FreeRTOS 2 lõi tách biệt; có phần cứng tăng tốc SHA-256. |
| 2 | Đầu dò cảm biến SHT30 | Giao tiếp I2C, vỏ bọc chống ẩm/đọng sương | 1 | 100.000 | 100.000 | Tín hiệu số I2C đã hiệu chuẩn sẵn từ nhà máy, không bị trôi ADC. |
| 3 | Màn hình OLED SSD1306 0.96" | Độ phân giải 128x64, giao tiếp I2C (`0x3C`) | 1 | 30.000 | 30.000 | Dùng chung bus I2C0; hiển thị tại chỗ. |
| 4 | Module MicroSD + RTC DS3231 | Giao tiếp SPI (MicroSD) + I2C (DS3231 kèm pin CR2032) | 1 | 100.000 | 100.000 | Lưu đệm `.CSV` tối thiểu 7 ngày mất mạng. |
| 5 | Module Relay 1 kênh 5V | Kích mức HIGH, có transistor và diode chống dòng ngược | 1 | 25.000 | 25.000 | Cách ly tải công suất của còi hú khỏi chân GPIO vi điều khiển. |
| 6 | Còi hú mini 5V + Đèn chớp | Điện áp 5V DC, âm lượng lớn | 1 | 20.000 | 20.000 | Cảnh báo tức thời cho tài xế và thủ kho. |
| 7 | Mạch sạc + Pin Li-ion 18650 | Pin 3.7V dung lượng >= 2600mAh kèm mạch sạc/boost 5V | 1 | 100.000 | 100.000 | Duy trì hệ thống hoạt động >= 8 giờ khi mất điện. |
| 8 | Vật tư phụ (Hộp IP65, nút nhấn, trở, tụ) | Hộp nhựa ABS IP65, 3 nút nhấn HMI, trở 100kOhm, tụ 100nF | 1 | 50.000 | 50.000 | Chống ẩm kho lạnh và tạo giao diện 3 nút nhấn HMI. |
| **Tổng** | | | | | **605.000** | *Khảo giá tháng 10/2026* |