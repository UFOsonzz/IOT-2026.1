## Sổ đăng ký và quản trị rủi ro

| Mã RR | Mô tả rủi ro | Khả năng | Mức ảnh hưởng | Biện pháp phòng ngừa & Phương án dự phòng |
| :--- | :--- | :--- | :--- | :--- |
| **RR-01** | Mạng Wi-Fi tại phòng bảo vệ đồ án bị lỗi hoặc chặn gói tin UDP Broadcast cổng 8888. | Cao | Cao | Đã thiết kế sẵn **Option 2** (bắt Hotspot từ máy tính/điện thoại) và **Option 3 (AP Direct `192.168.4.2`)** + ô nhập IP Broker thủ công trong `WifiConfig.h`. Quay sẵn video demo dự phòng. |
| **RR-02** | Cảm biến SHT30 hoặc module MicroSD bị hỏng/lỏng dây trước ngày demo. | Trung bình | Cao | Mua dự phòng thêm 01 cảm biến SHT30 và 01 module MicroSD; trong Firmware có cơ chế tự động Fallback về dữ liệu giả lập nếu bus I2C không phản hồi để hệ thống không bị treo. |
| **RR-03** | Tích hợp code giữa Firmware, Backend và Web bị lệch định dạng JSON hoặc lệch múi giờ. | Trung bình | Cao | Đã khóa cứng **Data Contract** (chuỗi Canonical String và Unix Timestamp UTC `ts_rtc`) ngay từ nhánh `feat/walking-skeleton` và kiểm chứng bằng `dummy_simulator.py`. |
| **RR-04** | Sụt áp nguồn khi đóng Relay bật còi hú 5V làm ESP32-S3 bị khởi động lại (Brownout Reset). | Trung bình | Trung bình | Cấp nguồn cho Relay và Còi từ đường `5V (VBUS)` riêng biệt với đường `3.3V` của MCU và sử dụng module Relay có cách ly quang (đã ghi rõ trong `hardware_wiring.md`). |
| **RR-05** | Xung đột cấu hình môi trường Docker giữa máy macOS và Windows của các thành viên. | Thấp | Trung bình | Chuẩn hóa `docker-compose.yml`, đồng bộ `.env.example` và tách riêng 2 script `run_simulator_mac.sh` và `run_simulator_win.bat`. |
