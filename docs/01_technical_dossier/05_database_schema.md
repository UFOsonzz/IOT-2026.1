# Lược đồ cơ sở dữ liệu

Phụ trách: TV2

---

## 1. Cấu trúc 4 bảng trên TimescaleDB

Hệ thống cơ sở dữ liệu TimescaleDB (PostgreSQL 16) tại `infra/db/init.sql` gồm 4 bảng:

1. `devices`: Lưu định danh thiết bị, dải nhiệt GSP chuẩn, public key ECDSA và trạng thái kết nối.
2. `telemetry_logs`: Lưu chuỗi thời gian dữ liệu đo, mã băm SHA-256 và chữ ký ECDSA. Bảng append-only, khóa cứng lệnh `UPDATE` và `DELETE` bằng trigger `trg_immutable_telemetry` (mã lỗi SQLSTATE `42501`).
3. `alerts`: Lưu sự cố vượt ngưỡng nhiệt GSP và trạng thái xử lý.
4. `audit_logs`: Lưu vết kiểm toán, phát hiện dữ liệu giả mạo và lịch sử kiểm tra chuỗi chữ ký.

> Tách bảng append-only `telemetry_logs` độc lập giúp bảo vệ tính toàn vẹn của dữ liệu đo, đồng thời vẫn cho phép cập nhật trạng thái hoạt động ở các bảng nghiệp vụ khác.

---

## 2. Chi tiết cấu trúc trường

### 2.1. Bảng `devices`

| Trường | Kiểu dữ liệu | Ràng buộc | Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `device_id` | `VARCHAR(32)` | `PRIMARY KEY` | Mã định danh duy nhất (VD: `GSP-TRUCK-01`). |
| `device_name` | `VARCHAR(100)` | `NOT NULL` | Tên kho lạnh hoặc xe tải gắn thiết bị. |
| `storage_type` | `VARCHAR(20)` | `NOT NULL` | Chế độ bảo quản (`MEDICAL_2_8C`, `SEAFOOD_FROZEN`). |
| `temp_min_c`, `temp_max_c` | `NUMERIC(5,2)` | `NOT NULL` | Ngưỡng nhiệt độ bảo quản chuẩn (+2.00°C đến +8.00°C). |
| `ecdsa_pub_key` | `TEXT` | `NOT NULL` | Public key ECDSA `secp256r1` để xác minh chữ ký gói tin. |
| `mqtt_token_hash` | `VARCHAR(64)` | `NOT NULL` | Mã băm SHA-256 của token xác thực MQTT. |
| `fw_version` | `VARCHAR(16)` | `DEFAULT '1.0.0'` | Phiên bản firmware trên vi điều khiển. |
| `is_online` | `BOOLEAN` | `DEFAULT FALSE` | Trạng thái kết nối (cập nhật qua topic `status` hoặc LWT). |
| `last_seen_at` | `TIMESTAMPTZ` | `NULL` | Thời điểm gần nhất nhận bản tin từ thiết bị. |

### 2.2. Bảng `telemetry_logs`

| Trường | Kiểu dữ liệu | Ràng buộc | Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `log_id` | `BIGSERIAL` | Tự tăng | Số thứ tự dòng nội bộ. |
| `device_id` | `VARCHAR(32)` | `PK, FK -> devices` | Thiết bị phát sinh dữ liệu. |
| `seq_no` | `BIGINT` | `NOT NULL` | Số thứ tự gói tin tự tăng (chống rớt gói và Replay Attack). |
| `ts_rtc` | `TIMESTAMPTZ` | `PK, NOT NULL` | Timestamp UTC gốc từ RTC DS3231. |
| `received_at` | `TIMESTAMPTZ` | `DEFAULT NOW()` | Thời điểm máy chủ tiếp nhận bản tin. |
| `is_backfill` | `BOOLEAN` | `DEFAULT FALSE` | `FALSE`: Gửi trực tiếp; `TRUE`: Đẩy bù từ thẻ nhớ qua kênh sync. |
| `temp_c`, `hum_pct`, `bat_v` | `NUMERIC` | `NOT NULL` | Nhiệt độ (°C), Độ ẩm (%RH), Điện áp pin (V). |
| `alert_code` | `SMALLINT` | `DEFAULT 0` | 0: Bình thường; 1: Vượt ngưỡng trên; 2: Dưới ngưỡng sàn. |
| `sha256_hash` | `CHAR(64)` | `NOT NULL` | Mã băm SHA-256 của chuỗi chuẩn hóa Canonical String. |
| `ecdsa_sig` | `VARCHAR(160)` | `NOT NULL` | Chữ ký số ECDSA từ thiết bị. |
| `sig_verified` | `BOOLEAN` | `NOT NULL` | Kết quả xác minh chữ ký (FALSE nếu dữ liệu bị sửa đổi). |

### 2.3. Bảng `alerts`

| Trường | Kiểu dữ liệu | Ràng buộc | Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `alert_id` | `BIGSERIAL` | `PRIMARY KEY` | Định danh cảnh báo tự tăng. |
| `device_id` | `VARCHAR(32)` | `NOT NULL, FK -> devices` | Thiết bị phát sinh cảnh báo vượt ngưỡng. |
| `seq_no` | `BIGINT` | `NULL` | Số thứ tự gói tin đo tương ứng lúc vi phạm. |
| `alert_code` | `SMALLINT` | `NOT NULL` | 1: Vượt ngưỡng trên > 8.00°C; 2: Dưới ngưỡng sàn < 2.00°C. |
| `severity` | `VARCHAR(20)` | `DEFAULT 'CRITICAL'` | Mức độ cảnh báo (`WARNING`, `CRITICAL`). |
| `temp_c`, `hum_pct` | `NUMERIC(5,2)` | `NOT NULL` | Giá trị nhiệt độ và độ ẩm lúc kích hoạt cảnh báo. |
| `message` | `TEXT` | `NOT NULL` | Nội dung mô tả gửi cho người vận hành và Telegram Bot. |
| `ts_rtc` | `TIMESTAMPTZ` | `NOT NULL` | Timestamp UTC gốc tại thời điểm phát hiện vượt ngưỡng. |
| `created_at` | `TIMESTAMPTZ` | `DEFAULT NOW()` | Thời điểm ghi nhận cảnh báo vào CSDL. |
| `is_resolved` | `BOOLEAN` | `DEFAULT FALSE` | Trạng thái xử lý (FALSE: Chưa xử lý; TRUE: Đã xong). |
| `resolved_at` | `TIMESTAMPTZ` | `NULL` | Thời điểm xử lý xong sự cố. |

### 2.4. Bảng `audit_logs`

| Trường | Kiểu dữ liệu | Ràng buộc | Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `audit_id` | `BIGSERIAL` | `PRIMARY KEY` | Định danh sự kiện kiểm toán tự tăng. |
| `event_type` | `VARCHAR(50)` | `NOT NULL` | Loại sự kiện (`SYSTEM_INIT`, `TAMPER_DETECTED`, `CHAIN_VERIFIED`). |
| `actor` | `VARCHAR(64)` | `DEFAULT 'SYSTEM'` | Nguồn thực hiện (`DB_INIT_SCRIPT`, `MQTT_WORKER`, `INSPECTOR_API`). |
| `device_id` | `VARCHAR(32)` | `NULL` | Thiết bị liên quan đến sự kiện (nếu có). |
| `description` | `TEXT` | `NOT NULL` | Mô tả chi tiết sự kiện phục vụ thanh tra GSP. |
| `metadata` | `JSONB` | `NULL` | Dữ liệu ngữ cảnh bổ sung (mã băm đối chiếu, gói tin bị sửa). |
| `created_at` | `TIMESTAMPTZ` | `DEFAULT NOW()` | Dấu thời gian ghi nhận sự kiện. |

---

## 3. Danh sách Index

| Index | Bảng | Cột | Mục đích tối ưu |
| :--- | :--- | :--- | :--- |
| `pk_telemetry` | `telemetry_logs` | `(device_id, ts_rtc DESC)` | Phân mảnh Hypertable, phân trang nhanh, khử trùng lặp khi đẩy bù. |
| `idx_telemetry_device_seq` | `telemetry_logs` | `(device_id, seq_no DESC)` | Đối soát tính liên tục của chuỗi gói tin khi kiểm toán. |
| `idx_alerts_device_time` | `alerts` | `(device_id, ts_rtc DESC)` | Truy vấn lịch sử sự cố theo từng thiết bị. |
| `idx_audit_logs_created_at` | `audit_logs` | `(created_at DESC)` | Truy xuất nhanh nhật ký kiểm toán từ mới đến cũ. |
