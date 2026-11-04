# Hồ sơ bàn giao hệ thống (dự kiến)
*(Sản phẩm cuối cùng của Bước 10 theo mẫu 5.7.5 - Phụ trách chính: TV4 & TV3 - Hoàn thiện ở Tuần 14)*

> **Ghi chú:** Đây là khung hồ sơ bàn giao được khởi tạo từ giai đoạn `feat/walking-skeleton`. Các thông số lắp đặt thực tế, danh mục kiểm tra an toàn thông tin và biên bản Phép thử bàn giao sẽ được chốt ở **Tuần 14** sau khi hoàn tất đợt vận hành thử 7 ngày.

## Phần 1: Hướng dẫn dựng lại hệ thống bằng một lệnh
- Toàn bộ yêu cầu môi trường (Docker Compose, Python 3.8+, PlatformIO) và quy trình khởi chạy 4 container bằng lệnh `cp .env.example .env && docker compose up -d --build` được trình bày chi tiết tại file `README.md` ở thư mục gốc.

## Phần 2: Sơ đồ kiến trúc và Sơ đồ đấu nối cập nhật đúng thực tế
- **Sơ đồ kiến trúc 4 lớp:** Xem tại `docs/01_technical_dossier/03_system_architecture.md`.
- **Sơ đồ đấu nối và Bảng gán chân ESP32-S3:** Xem tại `docs/01_technical_dossier/04_hardware_wiring.md` *(Sẽ rà soát và cập nhật lại đúng thực tế sau khi đóng hộp IP65 ở Tuần 12)*.

## Phần 3: Danh sách tài sản bàn giao (Cập nhật khi bàn giao ở Tuần 14)

| Mã Thiết bị | Tên Thiết bị | Vị trí lắp đặt / Mô hình thử nghiệm | Vi điều khiển | Phiên bản Firmware | Ngày lắp đặt / Nghiệm thu |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `GSP-TRUCK-01` | `[Cập nhật sau]` | `[Mô hình thùng lạnh bảo quản Vaccine (+2°C .. +8°C)]` | ESP32-S3-WROOM-1-N16R8 | `[v1.0.0-skeleton -> Cập nhật sau]` | `[Điền sau]` |

## Phần 4: Hướng dẫn sử dụng dành cho Người vận hành (Bản nháp)
1. **Cách theo dõi nhiệt độ hằng ngày:** Mở trình duyệt truy cập bảng điều khiển (`http://localhost:5173`). Khi nhãn trạng thái góc phải hiển thị **`THIẾT BỊ: ONLINE`** màu xanh lá và thẻ nhiệt độ hiển thị **`ĐẠT CHUẨN GSP`**, kho lạnh đang vận hành an toàn.
2. **Cách xử lý khi còi hú báo động kêu tại xe/kho lạnh:**
   - Quan sát màn hình tại chỗ hoặc giao diện Web xem nhiệt độ đang vượt ngưỡng trên (`> 8.0°C`) hay dưới ngưỡng dưới (`< 2.0°C`).
   - Nhấn nút **`BTN2 (ACK/MUTE - GPIO6)`** trên hộp thiết bị để tắt tiếng còi hú tạm thời tại chỗ, sau đó kiểm tra cửa kho lạnh hoặc hệ thống cấp nguồn máy lạnh.
3. **Cách đổi mạng Wi-Fi cho hộp thiết bị:**
   - Nhấn giữ nút **`BOOT (GPIO0)`** trên mạch trong **5 giây** cho đến khi đèn báo nháy nhanh liên tục.
   - Dùng điện thoại/máy tính kết nối vào mạng Wi-Fi **`GSP-ESP32-XXXX`**, mở trình duyệt vào địa chỉ `http://192.168.4.1`, chọn tên Wi-Fi mới, nhập mật khẩu và bấm **LƯU CẤU HÌNH** -> **KHỞI ĐỘNG LẠI**.
4. **Cách kiểm tra tính toàn vẹn dữ liệu phục vụ Thanh tra GSP:**
   - Bấm nút màu xanh **`"Kiểm toán chuỗi chữ ký GSP"`** ở góc trên bên phải màn hình Web. Nếu kết quả hiện **`VERIFIED` (0 bản ghi Tampered)**, toàn bộ lịch sử nhiệt độ hoàn toàn nguyên vẹn.

## Phần 5: Hướng dẫn xử lý sự cố thường gặp

| Triệu chứng quan sát được | Nguyên nhân có thể | Cách kiểm tra và xử lý |
| :--- | :--- | :--- |
| Giao diện Web hiện trạng thái đỏ **`OFFLINE (LWT)`** | Thiết bị mất nguồn hoặc rớt mạng Wi-Fi quá 30 giây. | Kiểm tra nguồn cấp 5V / Pin dự phòng 18650 và bộ phát Wi-Fi. |
| Xuất hiện dòng cảnh báo đỏ **`TAMPERED`** trong bảng nhật ký | Gói tin bị sửa đổi trái phép trên đường truyền hoặc sai lệch cấu hình khóa công khai. | Bấm nút **"Kiểm toán chuỗi chữ ký GSP"**, ghi nhận số thứ tự gói tin `seq_no` bị can thiệp. |
| Trình duyệt hiển thị **`WS Reconnecting...`** màu vàng | Dịch vụ Backend đang khởi động lại hoặc cổng `8000` bị chặn. | Chạy lệnh `docker compose ps` và `docker compose restart backend` trên máy chủ. |

## Phần 6: Danh mục Kiểm tra An toàn thông tin
- [x] 1. Không lưu cứng mật khẩu Wi-Fi và khóa bí mật trong mã nguồn Git (sử dụng `.gitignore` và bộ nhớ NVS Flash qua `WifiConfig.h`). *(Đã áp dụng ở nhánh `walking-skeleton`)*
- [x] 2. Thiết kế kiến trúc 1 chiều (Unidirectional): Thiết bị biên không mở bất kỳ cổng hay kênh MQTT nào nhận lệnh điều khiển từ xa. *(Đã áp dụng)*
- [x] 3. Tính toán mã băm SHA-256 phần cứng (`mbedTLS`) trên chuỗi Canonical String tại biên trước khi truyền. *(Đã áp dụng)*
- [ ] 4. Ký số điện tử ECDSA (`secp256r1`) thật chống chối bỏ nguồn gốc thiết bị. *(Kế hoạch Tuần 9–10)*
- [x] 5. Đánh số thứ tự gói tin `seq_no` kết hợp nhãn thời gian `ts_rtc` chống tấn công phát lại (Replay Attack). *(Cần lưu thêm `seq_no` vào NVS/SD ở Tuần 9)*
- [x] 6. Khóa cứng thao tác `UPDATE` và `DELETE` trên bảng `telemetry_logs` ở tầng Database Engine bằng Trigger `trg_immutable_telemetry`. *(Đã áp dụng)*
- [ ] 7. Ghi nhật ký kiểm toán bất biến vào bảng `audit_logs` khi phát hiện gói tin giả mạo hoặc khi thanh tra. *(Kế hoạch Tuần 10–11)*
- [x] 8. Giới hạn CORS chỉ cho phép các phương thức đọc (`GET`, `OPTIONS`) trên REST API. *(Đã áp dụng)*
- [x] 9. Giới hạn số lượng bản ghi tối đa trên mỗi trang truy vấn (`limit <= 500`) chống tấn công DoS. *(Đã áp dụng)*
- [ ] 10. Cấu hình xác thực người dùng/thiết bị trên Mosquitto Broker và bật TLS khi đưa lên môi trường Production. *(Kế hoạch Tuần 11–12)*
- [ ] 11. Đổi toàn bộ mật khẩu mặc định trong file `.env` trên máy chủ triển khai thực tế. *(Thực hiện trước khi bàn giao Tuần 14)*
- [ ] 12. Thực hiện **Phép thử bàn giao** với một nhóm khác trong lớp ở Tuần 14 (dựng lại hệ thống chỉ từ hồ sơ này mà không hỏi câu nào). *(Thực hiện ở Tuần 14)*