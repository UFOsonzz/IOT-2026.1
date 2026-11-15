# Giao diện API và hợp đồng dữ liệu

Phụ trách: TV2

---

## 1. Giao thức MQTT

| Topic | QoS | Retain | Chiều | Mục đích |
| :--- | :--- | :--- | :--- | :--- |
| `gsp/v1/{device_id}/telemetry` | `1` | `False` | Thiết bị → Server | Gửi dữ liệu đo thời gian thực. |
| `gsp/v1/{device_id}/sync` | `1` | `False` | Thiết bị → Server | Đẩy bù dữ liệu offline từ thẻ MicroSD, tự khử trùng lặp tại CSDL. |
| `gsp/v1/{device_id}/alert` | `1` | `False` | Thiết bị → Server | Báo động tức thời khi vượt ngưỡng GSP, lưu bảng `alerts` và phát lên WebSocket/Telegram. |
| `gsp/v1/{device_id}/status` | `1` | `True` | Thiết bị/Broker → Server | Báo `{"state":"ONLINE"}` khi kết nối hoặc gửi LWT `{"state":"OFFLINE"}` khi mất mạng. |

### Cấu trúc chuỗi chuẩn hóa (Canonical String)

Dùng để tính mã băm SHA-256 và chữ ký ECDSA:

```text
<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>
Ví dụ: GSP-TRUCK-01|1042|1761526800|4.25|62.50|0
```

---

## 2. REST API và WebSocket

| Phương thức | Đường dẫn | Tham số | Mã phản hồi | Quyền | Mô tả |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/v1/devices` | Không | `200`, `500` | Người dùng, QA | Lấy danh sách thiết bị kèm thông số đo mới nhất. |
| `GET` | `/api/v1/devices/{device_id}/telemetry` | `limit` (1..500), `cursor`, `from`, `to` | `200`, `400`, `500` | Người dùng, QA | Truy vấn lịch sử dữ liệu đo, hỗ trợ phân trang theo con trỏ (`ts_rtc < cursor`). |
| `GET` | `/api/v1/devices/{device_id}/verify-chain` | `device_id` (Path) | `200`, `500` | Quản trị, QA | Kiểm toán toàn bộ chuỗi chữ ký số của thiết bị, trả về số bản ghi hợp lệ và bị sửa đổi. |
| `WS` | `/ws/realtime` | WebSocket connection | `101` | Dashboard | Đẩy trực tiếp sự kiện `telemetry`, `sync`, `alert`, `status` theo thời gian thực. |
