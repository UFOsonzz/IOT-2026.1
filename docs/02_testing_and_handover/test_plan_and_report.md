# Kế Hoạch Và Biên Bản Kết Quả Kiểm Thử Hệ Thống (Bước 8)
*(Tài liệu thuộc Mục 5.2.8 & Rubric Kiểm thử - Phụ trách chính: TV4 cùng cả nhóm)*

> **Trạng thái tài liệu hiện tại:** Đã lập xong **Kế hoạch kiểm thử** cho 3 mức và 4 kịch bản bất lợi; đã kiểm chứng luồng logic Backend/DB/UI thông qua `tools/dummy_simulator.py`. Kết quả nghiệm thu trên phần cứng thực tế sẽ được cập nhật sau.

## 1. Kế hoạch và Kết quả Kiểm thử ở 3 mức

| Mức kiểm thử | Thành phần được kiểm thử | Phương pháp kiểm thử | Tiêu chí đạt (Pass Criteria) | Trạng thái (`feat/walking-skeleton`) | Kết quả nghiệm thu (Tuần 12) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Mức 1: Unit Test (Đơn vị)** | Hàm băm `computeHardwareSha256Hex()` trên ESP32-S3 và `compute_sha256_hex()` trên Python | So sánh đầu ra mã băm 64 ký tự hex với cùng chuỗi Canonical String mẫu. | Khớp 100% giữa C++ và Python. | Đã khớp trên nhánh `walking-skeleton` | `[Cập nhật sau]` |
| **Mức 1: Unit Test (CSDL)** | Trigger khóa bất biến `trg_immutable_telemetry` trên bảng `telemetry_logs` | Chạy câu lệnh `UPDATE telemetry_logs SET temp_c = 5.0;` và `DELETE FROM telemetry_logs;` trong `psql`. | PostgreSQL từ chối thực thi và trả về mã lỗi `SQLSTATE 42501`. | Đã kiểm chứng trên container `gsp-timescaledb` | `[Cập nhật sau]` |
| **Mức 2: Integration Test (API & DB)** | Toàn bộ điểm cuối REST API (`/devices`, `/telemetry`, `/verify-chain`) | Chạy tự động bộ sưu tập `06_postman_collection.json`. | 100% bài test trả về `200 OK` và khớp lược đồ JSON. | Đã chạy thông với dữ liệu Simulator | `[Cập nhật sau]` |
| **Mức 3: System Test (End-to-End)** | Toàn bộ luồng từ ESP32-S3 (SHT30, DS3231, SD) -> Mosquitto -> FastAPI -> TimescaleDB -> WebSocket -> React UI | Chạy mạch phần cứng hoàn chỉnh kết hợp đo thời gian phản hồi thực tế. | Độ trễ hiển thị `< 2s`, còi hú tại chỗ `<= 3s` khi vượt ngưỡng. | Mới chạy thông khung (chưa gắn SHT30 & SD thật) | `[Cập nhật sau]` |

## 2. Biên bản Kiểm thử 4 Kịch bản Bất lợi

| Mã KB | Kịch bản bất lợi | Quy trình tái hiện kiểm thử | Kết quả mong đợi | Kiểm thử bằng Simulator | Bằng chứng nghiệm thu phần cứng (Tuần 12) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ADV-01** | **Mất mạng giữa chừng** | Ngắt kết nối Wi-Fi/Hotspot trong khi mạch đang đo, sau đó bật lại mạng. | Mạch lưu bản ghi vào MicroSD kèm `ts_rtc` gốc; khi có mạng lại tự động đẩy bù lên kênh `sync` đủ 100% bản ghi. | Đã test luồng nhận kênh `sync` (Tùy chọn `[3]`) | `[Hoàn thành sau]` |
| **ADV-02** | **Máy chủ dừng rồi chạy lại** | Chạy lệnh `docker compose restart backend timescaledb` khi thiết bị đang gửi tin. | Nhờ cấu hình `persistence true` và `clean_session=False` (QoS 1), không bản tin nào bị mất và Backend tự động kết nối lại. | Đã cấu hình trong `mosquitto.conf` & `mqtt_worker.py` | `[Hoàn thành sau]` |
| **ADV-03** | **Dữ liệu sai định dạng hoặc bị tấn công sửa đổi** | Chạy tùy chọn `[4]` trong `dummy_simulator.py` (sửa nhiệt độ từ `9.80°C` xuống `4.50°C` trên đường truyền). | Hệ thống không bị treo; Backend phát hiện lệch mã băm SHA-256, đánh dấu `sig_verified = false` và hiển thị nhãn đỏ **`TAMPERED`** trên Web. | Đã chạy thông bằng Simulator (Tùy chọn `[4]`) | `[Hoàn thành sau]` |
| **ADV-04** | **Tải đột biến (Burst Load)** | Đẩy liên tiếp hàng trăm bản tin đồng bộ bù + bản tin trùng lặp lên Broker. | TimescaleDB ghi nhận mượt mà và tự động loại bỏ các bản ghi trùng lặp nhờ `ON CONFLICT (device_id, ts_rtc) DO NOTHING`. | Đã test khử trùng cơ bản (Tùy chọn `[3]`) | `[Hoàn thành sau]` |