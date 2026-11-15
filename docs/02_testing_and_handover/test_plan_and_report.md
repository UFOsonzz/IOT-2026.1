# Kế hoạch và kết quả kiểm thử hệ thống

Ghi nhận kết quả kiểm thử theo các cấp độ và kịch bản bất lợi.

---

## 1. Kết quả kiểm thử 3 cấp độ

| Cấp độ | Thành phần kiểm thử | Phương pháp | Tiêu chí đạt | Kết quả Sprint 1 |
| :--- | :--- | :--- | :--- | :--- |
| **Unit Test** | Giải mã cảm biến SHT30 và SHT40 | Kiểm tra CRC-8, chuyển đổi dữ liệu thô sang nhiệt ẩm | Đúng công thức Datasheet, loại bỏ frame lỗi CRC | **Pass** |
| **Unit Test** | Giải mã thời gian thực DS3231 | Kiểm tra định dạng BCD, tính toán epoch UTC, năm nhuận | Mốc thời gian chính xác, lọc dữ liệu sai | **Pass** |
| **Unit Test** | Lọc rung phím Debounce | Bắt cạnh tín hiệu, lọc nhiễu 35ms, chống lặp khi giữ phím | Nhận diện đúng sự kiện nhấn đơn | **Pass** |
| **Unit Test** | Khóa dữ liệu bất biến TimescaleDB | Thao tác UPDATE và DELETE trên bảng `telemetry_logs` | CSDL từ chối, trả về lỗi SQLSTATE 42501 | **Pass** |
| **Integration Test** | Điểm cuối REST API | Chạy kiểm thử tự động `06_postman_collection.json` | 100% request trả về mã 200 OK | **Pass** |
| **System Test** | Mạch phần cứng đến Web Dashboard | Mạch thật ESP32 / ESP32-S3 gửi dữ liệu qua MQTT lên Web | Hiển thị thời gian thực, còi hú kích hoạt khi vượt ngưỡng | **Pass** |

---

## 2. Kiểm thử 4 kịch bản bất lợi

| Mã | Kịch bản | Quy trình kiểm thử | Kết quả mong đợi | Trạng thái |
| :--- | :--- | :--- | :--- | :--- |
| **ADV-01** | Mất kết nối mạng | Ngắt WiFi khi mạch đang đo | Lưu đệm offline, tự động đẩy bù khi có mạng | Đã kiểm thử luồng nhận kênh sync trên Simulator |
| **ADV-02** | Khởi động lại máy chủ | Khởi động lại container Backend và TimescaleDB | Không mất gói tin nhờ cơ chế QoS 1 của MQTT | Đã cấu hình và kiểm chứng |
| **ADV-03** | Dữ liệu bị can thiệp | Sửa giá trị nhiệt độ gói tin trên đường truyền | Backend phát hiện lệch mã băm SHA-256, gắn cờ `TAMPERED` | Đã kiểm chứng thành công |
| **ADV-04** | Tải đột biến | Gửi liên tiếp lượng lớn bản tin đồng bộ bù | TimescaleDB tự động khử trùng lặp qua khóa chính kép | Đã kiểm chứng thành công |
