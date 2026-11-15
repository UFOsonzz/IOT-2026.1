# Sổ đăng ký và quản trị rủi ro

| Mã | Mô tả rủi ro | Khả năng | Mức ảnh hưởng | Phương án xử lý |
| :--- | :--- | :--- | :--- | :--- |
| **RR-01** | Mạng WiFi phòng lab lỗi hoặc chặn UDP Broadcast cổng 8888 | Cao | Cao | Thiết kế 3 tùy chọn kết nối: Mạng chung, Hotspot máy tính, và AP Direct 192.168.4.2 qua Captive Portal |
| **RR-02** | Cảm biến SHT30/SHT40 hoặc MicroSD hỏng, lỏng dây tiếp xúc | Trung bình | Cao | Chuẩn bị linh kiện dự phòng, Firmware phát hiện lỗi I2C và ngắt truyền tin khi dữ liệu không hợp lệ |
| **RR-03** | Lệch định dạng số thực hoặc lệch mốc thời gian giữa MCU và Server | Trung bình | Cao | Khóa cứng Canonical String định dạng `%.2f` và mốc thời gian UTC từ RTC DS3231 |
| **RR-04** | Sụt áp nguồn khi đóng Relay làm ESP32 / ESP32-S3 bị Brownout Reset | Trung bình | Trung bình | Cấp nguồn 5V riêng cho cuộn hút Relay và còi, cách ly quang với chân điều khiển của MCU |
| **RR-05** | Xung đột môi trường Docker giữa macOS và Windows | Thấp | Trung bình | Đồng bộ cấu hình `.env.example`, tách script khởi chạy riêng cho từng hệ điều hành |
