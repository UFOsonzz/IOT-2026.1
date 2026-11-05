# (6a) Giao Diện API Và Hợp Đồng Dữ Liệu
*(Thành phần bắt buộc số 6 theo Mục 5.3.1 & 5.3.3g - Phụ trách chính: TV2)*

## 1. Hợp đồng Giao tiếp MQTT

| Kênh MQTT Topic | QoS | Retain | Chiều dữ liệu | Mục đích và xử lý tại Backend |
| :--- | :--- | :--- | :--- | :--- |
| `gsp/v1/{device_id}/telemetry` | `1` | `False` | Thiết bị -> Server | Gửi bản ghi đo thời gian thực. |
| `gsp/v1/{device_id}/sync` | `1` | `False` | Thiết bị -> Server | Đẩy bù bản ghi Offline từ thẻ MicroSD, tự động khử trùng bằng `ON CONFLICT DO NOTHING`. |
| `gsp/v1/{device_id}/alert` | `1` | `False` | Thiết bị -> Server | Gửi cảnh báo vượt ngưỡng GSP tức thời, lưu vào bảng `alerts` và bắn lên WebSocket/Telegram. |
| `gsp/v1/{device_id}/status` | `1` | `True` | Thiết bị/Broker -> Server | Báo `{"state":"ONLINE"}` khi kết nối hoặc phát Di chúc LWT `{"state":"OFFLINE"}` khi mất mạng. |

### Ghép chuỗi chuẩn hóa (Canonical String) để băm SHA-256 và ký số ECDSA:
```text
<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>
Ví dụ: GSP-TRUCK-01|1042|1761526800|4.25|62.50|0
```

## 2. Danh sách Điểm cuối REST API & WebSocket

| **Phương thức** | **Đường dẫn** | **Tham số đầu vào** | **Mã trạng thái** | **Quyền truy cập** | **Mô tả chức năng** |
| --------------- | ------------------------------------------ | ---------------------------------------------------- | ------------------------- | ------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `GET` | `/api/v1/devices` | Không | `200 OK`, `500` | Người vận hành, thanh tra GSP  | Trả về danh sách thiết bị kèm thông số đo mới nhất.|
| `GET` | `/api/v1/devices/{device_id}/telemetry`    | `limit` (1..500), `cursor` (ISO/Epoch), `from`, `to` | `200 OK`, `400`, `500`    | Người vận hành , thanh tra GSP  | Truy vấn lịch sử bản ghi bất biến theo phân trang con trỏ (`ts_rtc < cursor`). |
| `GET` | `/api/v1/devices/{device_id}/verify-chain` | `device_id` (Path) | `200 OK`, `500` | Thanh tra GSP, quản trị viên | Kiểm toán toàn bộ chuỗi chữ ký số của thiết bị, thống kê số bản ghi hợp lệ (`valid_records`) và bị sửa đổi (`tampered_records`). |
| `WS` | `/ws/realtime` | Kết nối WebSocket | `101 Switching Protocols` | Giao diện Dashboard | Đẩy trực tiếp sự kiện `telemetry`, `sync`, `alert`, `status` xuống trình duyệt theo thời gian thực. |
