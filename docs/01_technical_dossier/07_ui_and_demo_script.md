# (7) Giao Diện Và Kịch Bản Demo
*(Thành phần bắt buộc số 7 theo Mục 5.3.1 - Phụ trách chính: TV3)*

## 1. Các màn hình chính của Web Dashboard
- `[TODO - TV3 chèn ảnh chụp màn hình 1: Trạng thái hoạt động bình thường chuẩn GSP (ONLINE, ECDSA Verified)]`
- `[TODO - TV3 chèn ảnh chụp màn hình 2: Giao diện nhấp nháy Đỏ khi vượt ngưỡng nhiệt độ > 8.0°C]`
- `[TODO - TV3 chèn ảnh chụp màn hình 3: Phát hiện gói tin bị sửa đổi trái phép (Nhãn TAMPERED) & Kết quả Kiểm toán chuỗi chữ ký]`

## 2. Kịch bản Demo (Cập nhật theo tiến độ luồng chạy thông)

| Bước | Thao tác thực hiện khi Demo | Kết quả mong đợi trên Phần cứng & Giao diện | Trạng thái hiện tại (`feat/walking-skeleton`) |
| :--- | :--- | :--- | :--- |
| **Bước 1** | Khởi chạy Docker (`docker compose up -d`) và cấp nguồn cho mạch ESP32-S3 (hoặc chạy `dummy_simulator.py`). | Trạng thái thiết bị trên Web chuyển từ `OFFLINE` sang `ONLINE` màu xanh lá; dữ liệu cập nhật mỗi 10s qua WebSocket. | **Đã chạy thông** (Dữ liệu giả lập trên MCU & Simulator) |
| **Bước 2** | Nhấn nút `BTN3 (GPIO7 - TEST)` trên mạch (hoặc chọn `[2]` trên Simulator). *(Giai đoạn sau: Tác động nhiệt trực tiếp vào cảm biến SHT30)*. | Nhiệt độ vọt lên `> 8.0°C` -> Relay đóng bật còi/đèn tại chỗ, Web nhấp nháy cảnh báo Đỏ. *(Giai đoạn sau: Bắn thêm tin nhắn Telegram)*. | **Đã có Relay, Nút TEST & Web Alert** (`[TODO Tuần 8: SHT30]`, `[TODO Tuần 10: Telegram]`) |
| **Bước 3** | Nhấn nút `BTN2 (GPIO6 - ACK/Mute)` tại mạch biên. | Còi hú tại chỗ ngắt tiếng để tài xế xử lý sự cố, nhưng cảnh báo trên Web vẫn giữ nguyên để lưu vết. | **Đã có logic trong Firmware** *(Cần tối ưu sang ngắt GPIO ở Tuần 8)* |
| **Bước 4** | Ngắt kết nối mạng (hoặc chạy tùy chọn `[3]`, `[5]` trên Simulator) để kiểm tra LWT và luồng đẩy bù `sync`. | Khi mất mạng quá 30s, Broker phát bản tin LWT báo `OFFLINE`. Khi có mạng lại, các bản ghi lưu đệm đẩy lên với nhãn `AUTO-SYNC (SD)` không trùng lặp. | **Đã chạy thông LWT & Dedup tại DB** (`[TODO Tuần 9: Đọc/ghi file CSV trên MicroSD thật]`) |
| **Bước 5** | Chạy tùy chọn `[4]` trong `dummy_simulator.py` (sửa nhiệt độ từ `9.80°C` thành `4.50°C` trên đường truyền) và bấm nút **"Kiểm toán chuỗi chữ ký GSP"**. | Bản ghi bị gắn cờ đỏ `TAMPERED`; hộp thoại kiểm toán báo `COMPROMISED` và chỉ rõ số lượng bản ghi bị can thiệp. | **Đã chạy thông** (Băm SHA-256 thật + Chữ ký Dummy; `[TODO Tuần 9: ECDSA secp256r1 thật]`) |