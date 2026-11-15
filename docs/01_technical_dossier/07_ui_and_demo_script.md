# Giao diện và kịch bản demo

## 1. Các màn hình chính của Web Dashboard

1. Màn hình hoạt động bình thường: Trạng thái thiết bị `ONLINE`, thông số nhiệt ẩm đạt chuẩn GSP, chữ ký hợp lệ.
2. Màn hình cảnh báo vượt ngưỡng: Thẻ đo nhấp nháy đỏ khi nhiệt độ vượt dải 2.0°C đến 8.0°C.
3. Màn hình kiểm toán dữ liệu: Bản ghi can thiệp trái phép bị gắn cờ `TAMPERED`, kết quả kiểm toán chuỗi băm báo cảnh báo vi phạm.

## 2. Kịch bản demo

| Bước | Thao tác | Kết quả trên phần cứng và giao diện | Trạng thái |
| :--- | :--- | :--- | :--- |
| **Bước 1** | Khởi chạy Docker và cấp nguồn mạch ESP32 / ESP32-S3 | Thiết bị chuyển trạng thái `ONLINE`, dữ liệu nhiệt ẩm thực từ SHT30/SHT40 và thời gian thực từ DS3231 đẩy lên Web mỗi 2 giây | Đã hoàn thành |
| **Bước 2** | Nhấn nút BTN3 (GPIO25 trên Classic / GPIO15 trên S3) hoặc làm nóng cảm biến | Nhiệt độ vượt ngưỡng 8.0°C, Relay kích hoạt còi đèn tại chỗ, Web Dashboard nhấp nháy cảnh báo đỏ | Đã hoàn thành |
| **Bước 3** | Nhấn nút BTN2 (GPIO33 trên Classic / GPIO5 trên S3) | Còi hú tại chỗ ngắt âm lượng, cảnh báo trên Web vẫn lưu vết phục vụ truy xuất | Đã hoàn thành |
| **Bước 4** | Ngắt kết nối mạng WiFi của mạch | Quá 30 giây Broker phát tin LWT báo `OFFLINE` trên Web | Đã hoàn thành |
| **Bước 5** | Chạy kịch bản sửa đổi dữ liệu nhiệt độ trên đường truyền | Backend phát hiện sai lệch mã băm SHA-256, gắn nhãn đỏ `TAMPERED` trên bảng nhật ký | Đã hoàn thành |
