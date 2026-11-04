# (8) Nhật Ký Lỗi Và Bài Học
*(Thành phần bắt buộc số 8 của Hồ sơ kỹ thuật dự án theo Mục 5.3.1 & 5.3.3g - Học phần IT4735)*

Tài liệu này ghi nhận trung thực các lỗi kỹ thuật về phần cứng, firmware, mạng và cơ sở dữ liệu mà nhóm đã gặp và trực tiếp gỡ lỗi theo từng giai đoạn phát triển hệ thống.

## 1. Các lỗi đã gặp và xử lý ở giai đoạn Chạy thông luồng

| Mã Lỗi | Giai đoạn | Hiện tượng lỗi | Nguyên nhân | Cách khắc phục | Bài học rút ra |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ERR-01** | Kiểm thử kết nối mạng Option 1 (`WifiConfig.h`) | Mạch ESP32-S3 kết nối vào Wi-Fi chung (Option 1) nhưng liên tục in log `[UDP DISCOVERY] Chưa thấy phản hồi UDP cổng 8888` và không kết nối được MQTT. | Firmware ở Option 1 phát gói tin UDP Broadcast `"WHO_IS_IOT_SERVER"` tới cổng `8888`, nhưng trên Backend chưa khởi tạo dịch vụ lắng nghe cổng `8888/udp` để trả lời `"IOT_SERVER_HERE"`. | Bổ sung `UdpDiscoveryProtocol` lắng nghe cổng `8888/udp` trong `backend/app/main.py` và mở port `"8888:8888/udp"` trong `docker-compose.yml`. | Mọi giao thức tự động khám phá (Auto-Discovery) đều phải được kiểm thử cả 2 đầu (Client & Server) và cần bổ sung thêm ô nhập IP tĩnh dự phòng ở các bản tiếp theo khi mạng chặn Broadcast. |
| **ERR-02** | Khởi chạy Docker bằng `.env.example` | Khi chạy `docker compose up` lần đầu rồi mới sửa file `.env`, container `gsp-backend` báo lỗi `password authentication failed for user "gsp_user"`. | Mật khẩu trong `.env.example` lúc đầu bị đặt lệch với giá trị fallback trong `docker-compose.yml` và `db.py`, trong khi Docker Volume `timescale_data` của PostgreSQL giữ mật khẩu của lần khởi tạo đầu tiên. | Đồng bộ toàn bộ mật khẩu mẫu trong `.env.example`, `docker-compose.yml` và `db.py` về cùng một giá trị thống nhất (`gsp_pass`) và hướng dẫn dùng `docker compose down -v` khi cần reset DB. | Cấu hình biến môi trường (`.env.example`) và `docker-compose.yml` phải đồng nhất tuyệt đối để đảm bảo tiêu chí "dựng lại hệ thống bằng một lệnh duy nhất". |
| **ERR-03** | Đối soát mã băm SHA-256 giữa C++ và Python | Mã băm SHA-256 tính trên ESP32-S3 (`main.cpp`) và trên Backend (`mqtt_worker.py`) đôi khi khác nhau dù cùng giá trị nhiệt độ (ví dụ `4.5` vs `4.50`). | Sai khác định dạng số thực dấu phẩy động (Floating-point formatting) khi ghép chuỗi trước khi băm SHA-256. | Định nghĩa **Chuỗi chuẩn hóa (Canonical String)** nghiêm ngặt: `<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>` ép cố định 2 chữ số thập phân (`%.2f` trong C++ và `:.2f` trong Python) kèm làm tròn trước khi băm. | Khi ký số và băm dữ liệu giữa thiết bị nhúng C/C++ và Server Python, không bao giờ băm trực tiếp chuỗi JSON thô (do thứ tự key và khoảng trắng có thể thay đổi) mà phải băm trên một Canonical String cố định. |

## 2. Nhật ký lỗi và Kiểm thử toàn hệ thống

| Mã Lỗi | Giai đoạn | Hiện tượng lỗi | Nguyên nhân | Cách khắc phục | Bài học rút ra |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ERR-04** | `[Cập nhật sau]` | `[Ghi nhận sau]` | `[...]` | `[...]` | `[...]` |
