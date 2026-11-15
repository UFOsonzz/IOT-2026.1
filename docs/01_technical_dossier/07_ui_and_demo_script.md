# Giao diện và kịch bản demo

Phụ trách: TV3

---

## 1. Các chế độ hiển thị trên Web Dashboard

1. **Bình thường:** Trạng thái `ONLINE`, nhiệt độ và độ ẩm trong ngưỡng an toàn GSP (+2.0°C đến +8.0°C), chữ ký hợp lệ.
2. **Cảnh báo:** Thẻ đo nhấp nháy đỏ khi nhiệt độ vượt ngưỡng an toàn.
3. **Kiểm toán:** Gói tin bị can thiệp trên đường truyền sẽ bị gắn cờ `TAMPERED`, kết quả kiểm toán chuỗi băm báo vi phạm toàn vẹn.

---

## 2. Kịch bản demo

| Bước | Thao tác | Hiện tượng trên mạch và giao diện | Trạng thái |
| :--- | :--- | :--- | :--- |
| **Bước 1** | Bật Docker và cấp nguồn mạch ESP32 / ESP32-S3 | Thiết bị chuyển `ONLINE`; dữ liệu nhiệt ẩm từ SHT30/SHT40 và timestamp từ DS3231 đẩy lên Web mỗi 2 giây. | Đã hoàn thành |
| **Bước 2** | Nhấn BTN3 (GPIO25 trên Classic / GPIO15 trên S3) hoặc làm nóng cảm biến | Nhiệt độ vượt 8.0°C, Relay kích hoạt còi và đèn tại chỗ, Web Dashboard nhấp nháy cảnh báo đỏ. | Đã hoàn thành |
| **Bước 3** | Nhấn BTN2 (GPIO33 trên Classic / GPIO5 trên S3) | Còi ngắt âm thanh tại chỗ, sự cố cảnh báo vẫn lưu trên Web phục vụ truy xuất. | Đã hoàn thành |
| **Bước 4** | Ngắt kết nối WiFi của mạch | Quá 30 giây Broker phát LWT chuyển trạng thái sang `OFFLINE` trên Web. | Đã hoàn thành |
| **Bước 5** | Sửa đổi giá trị nhiệt độ trong gói tin truyền | Backend phát hiện sai lệch mã băm SHA-256, gắn nhãn đỏ `TAMPERED` trên bảng dữ liệu. | Đã hoàn thành |
