# Quản trị rủi ro

| Mã | Mô tả rủi ro | Khả năng | Ảnh hưởng | Phương án xử lý |
| :--- | :--- | :--- | :--- | :--- |
| **RR-01** | WiFi phòng lab lỗi hoặc chặn UDP Broadcast cổng 8888 | Cao | Cao | Hỗ trợ 3 tùy chọn mạng: WiFi chung, Hotspot máy tính và AP Direct 192.168.4.2 qua Captive Portal |
| **RR-02** | Cảm biến SHT30/SHT40 hoặc MicroSD hỏng, lỏng dây nối | Trung bình | Cao | Có linh kiện dự phòng; Firmware phát hiện lỗi I2C và ngừng gửi tin khi dữ liệu bất thường |
| **RR-03** | Lệch định dạng số thực hoặc timestamp giữa MCU và Server | Trung bình | Cao | Chuẩn hóa Canonical String định dạng `%.2f` và lấy timestamp UTC từ RTC DS3231 |
| **RR-04** | Sụt áp khi đóng Relay gây Brownout Reset vi điều khiển | Trung bình | Trung bình | Dùng nguồn 5V riêng cho cuộn hút Relay và còi, cách ly quang với chân điều khiển MCU |
| **RR-05** | Xung đột môi trường Docker giữa macOS và Windows | Thấp | Trung bình | Đồng bộ file `.env.example`, tách script khởi chạy riêng cho từng hệ điều hành |
