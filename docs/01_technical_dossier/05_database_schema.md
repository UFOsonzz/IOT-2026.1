# (5) Lược Đồ Cơ Sở Dữ Liệu
*(Thành phần bắt buộc số 5 theo Mục 5.3.1 & 5.3.3g - Phụ trách chính: TV2)*

## 1. Sơ đồ quan hệ và Lý do tách 4 bảng độc lập
Hệ thống cơ sở dữ liệu trên **TimescaleDB (PostgreSQL 16)** (`infra/db/init.sql`) được tách thành 4 bảng:
1. `devices` **(có cập nhật)**: Lưu thông tin định danh thiết bị, ngưỡng chuẩn GSP (`temp_min_c`, `temp_max_c`), khóa công khai `ecdsa_pub_key` và trạng thái trực tuyến (`is_online`, `last_seen_at`).
2. `telemetry_logs` **(chỉ ghi thêm)**: Lưu chuỗi thời gian các bản ghi đo vi khí hậu kèm mã băm `sha256_hash` và chữ ký `ecdsa_sig`. Bảng này bị khóa cứng thao tác `UPDATE` và `DELETE` ở tầng Engine bằng Trigger `trg_immutable_telemetry` (trả về lỗi SQLSTATE `42501` nếu có can thiệp).
3. `alerts`: Bảng quản lý sự cố, lưu danh sách các đợt vi phạm ngưỡng nhiệt độ GSP và trạng thái xử lý (`is_resolved`).
4. `audit_logs`: Bảng nhật ký kiểm toán, lưu vết mọi sự kiện khởi tạo, kiểm toán chuỗi chữ ký và phát hiện gói tin giả mạo.

>Việc tách bảng bất biến (`telemetry_logs`) khỏi các bảng có cập nhật trạng thái (`devices`, `alerts`) giúp áp dụng chính sách khóa cứng `READONLY/Append-Only` cho dữ liệu gốc mà vẫn cập nhật được trạng thái `ONLINE/OFFLINE` của thiết bị.

## 2. Chi tiết cấu trúc trường

### 2.1. Bảng `devices`
| Tên trường | Kiểu dữ liệu | Ràng buộc | Ý nghĩa và Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `device_id` | `VARCHAR(32)` | `PRIMARY KEY` | Mã định danh duy nhất của thiết bị biên (VD: `GSP-TRUCK-01`). |
| `device_name` | `VARCHAR(100)` | `NOT NULL` | Tên kho lạnh hoặc xe tải được gắn thiết bị. |
| `storage_type` | `VARCHAR(20)` | `NOT NULL` | Phân loại chế độ bảo quản (`MEDICAL_2_8C`, `SEAFOOD_FROZEN`). |
| `temp_min_c`, `temp_max_c` | `NUMERIC(5,2)` | `NOT NULL` | Ngưỡng nhiệt độ bảo quản dưới và trên (`+2.00°C` đến `+8.00°C`). |
| `ecdsa_pub_key` | `TEXT` | `NOT NULL` | Khóa công khai ECDSA `secp256r1` dùng để xác minh chữ ký gói tin. |
| `mqtt_token_hash` | `VARCHAR(64)` | `NOT NULL` | Mã băm SHA-256 của mật khẩu xác thực MQTT thiết bị. |
| `fw_version` | `VARCHAR(16)` | `DEFAULT '1.0.0'` | Phiên bản Firmware hiện tại trên ESP32-S3. |
| `is_online` | `BOOLEAN` | `DEFAULT FALSE` | Trạng thái kết nối tức thời (cập nhật qua kênh `status` hoặc di chúc LWT). |
| `last_seen_at` | `TIMESTAMPTZ` | `NULL` | Thời điểm gần nhất nhận được bản tin từ thiết bị. |

### 2.2. Bảng `telemetry_logs`
| Tên trường | Kiểu dữ liệu | Ràng buộc | Ý nghĩa và Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `log_id` | `BIGSERIAL` | Tự tăng | Số thứ tự dòng nội bộ trong CSDL. |
| `device_id` | `VARCHAR(32)` | `PK, FK -> devices` | Thiết bị phát sinh bản ghi. |
| `seq_no` | `BIGINT` | `NOT NULL` | Số thứ tự gói tin tự tăng từ thiết bị biên (phát hiện rớt gói / Replay Attack). |
| `ts_rtc` | `TIMESTAMPTZ` | `PK, NOT NULL` | **Dấu thời gian thực gốc (UTC)** gán tại thiết bị biên bởi IC RTC DS3231. |
| `received_at` | `TIMESTAMPTZ` | `DEFAULT NOW()` | Thời điểm máy chủ tiếp nhận bản tin (độ lệch với `ts_rtc` phản ánh thời gian lưu đệm). |
| `is_backfill` | `BOOLEAN` | `DEFAULT FALSE` | `FALSE`: Gửi trực tiếp qua kênh `telemetry`; `TRUE`: Đẩy bù từ MicroSD qua kênh `sync`. |
| `temp_c`, `hum_pct`, `bat_v` | `NUMERIC` | `NOT NULL` | Nhiệt độ (`°C`), Độ ẩm (`%RH`) và Điện áp pin (`V`). |
| `alert_code` | `SMALLINT` | `DEFAULT 0` | `0`: Bình thường; `1`: Vượt ngưỡng trần (`>8°C`); `2`: Dưới ngưỡng sàn (`<2°C`). |
| `sha256_hash` | `CHAR(64)` | `NOT NULL` | Mã băm SHA-256 của chuỗi Canonical String tính tại ESP32-S3. |
| `ecdsa_sig` | `VARCHAR(160)` | `NOT NULL` | Chữ ký số ECDSA của thiết bị biên. |
| `sig_verified` | `BOOLEAN` | `NOT NULL` | Kết quả Backend đối soát chữ ký (`FALSE` = Dữ liệu bị sửa đổi `TAMPERED`). |

### 2.3. Bảng `alerts`
| Tên trường | Kiểu dữ liệu | Ràng buộc | Ý nghĩa và Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `alert_id` | `BIGSERIAL` | `PRIMARY KEY` | Mã định danh tự tăng của sự cố cảnh báo. |
| `device_id` | `VARCHAR(32)` | `NOT NULL, FK -> devices` | Thiết bị biên phát sinh cảnh báo vượt ngưỡng GSP. |
| `seq_no` | `BIGINT` | `NULL` | Số thứ tự gói tin đo tương ứng tại thời điểm xảy ra vi phạm (liên kết logic với `telemetry_logs`). |
| `alert_code` | `SMALLINT` | `NOT NULL` | Mã loại vi phạm (`1`: Vượt ngưỡng trần trên `8.00°C`; `2`: Dưới ngưỡng sàn dưới `2.00°C`). |
| `severity` | `VARCHAR(20)` | `DEFAULT 'CRITICAL'` | Mức độ nghiêm trọng của cảnh báo (`WARNING`, `CRITICAL`). |
| `temp_c`, `hum_pct` | `NUMERIC(5,2)` | `NOT NULL` | Giá trị nhiệt độ (`°C`) và độ ẩm (`%RH`) ghi nhận tại thời điểm kích hoạt báo động. |
| `message` | `TEXT` | `NOT NULL` | Nội dung mô tả chi tiết cảnh báo gửi tới người vận hành và Telegram Bot. |
| `ts_rtc` | `TIMESTAMPTZ` | `NOT NULL` | Dấu thời gian thực gốc (UTC) tại thiết bị khi phát hiện vượt ngưỡng. |
| `created_at` | `TIMESTAMPTZ` | `DEFAULT NOW()` | Thời điểm máy chủ lưu bản ghi cảnh báo vào cơ sở dữ liệu. |
| `is_resolved` | `BOOLEAN` | `DEFAULT FALSE` | Trạng thái xử lý sự cố (`FALSE`: Đang cảnh báo; `TRUE`: Đã khắc phục). |
| `resolved_at` | `TIMESTAMPTZ` | `NULL` | Thời điểm sự cố được xác nhận xử lý xong. |

### 2.4. Bảng `audit_logs`
| Tên trường | Kiểu dữ liệu | Ràng buộc | Ý nghĩa và Nghiệp vụ |
| :--- | :--- | :--- | :--- |
| `audit_id` | `BIGSERIAL` | `PRIMARY KEY` | Số thứ tự dòng nhật ký kiểm toán tự tăng. |
| `event_type` | `VARCHAR(50)` | `NOT NULL` | Phân loại sự kiện kiểm toán (VD: `SYSTEM_INIT`, `TAMPER_DETECTED`, `CHAIN_VERIFIED`). |
| `actor` | `VARCHAR(64)` | `DEFAULT 'SYSTEM'` | Tác nhân thực hiện hành động (`DB_INIT_SCRIPT`, `MQTT_WORKER`, `INSPECTOR_API`). |
| `device_id` | `VARCHAR(32)` | `NULL` | Mã thiết bị liên quan đến sự kiện kiểm toán (nếu có). |
| `description` | `TEXT` | `NOT NULL` | Mô tả chi tiết sự kiện phục vụ truy vết và thanh tra GSP. |
| `metadata` | `JSONB` | `NULL` | Dữ liệu ngữ cảnh bổ sung dạng JSON (lưu mã băm nhận được, mã băm tính toán, `seq_no` bị sửa đổi). |
| `created_at` | `TIMESTAMPTZ` | `DEFAULT NOW()` | Dấu thời gian chuẩn UTC ghi nhận sự kiện kiểm toán. |

## 3. Bảng tổng hợp các Chỉ số và truy vấn được tối ưu

| Tên Chỉ số | Bảng áp dụng | Cột đánh chỉ số | Phục vụ trực tiếp cho Truy vấn / Cơ chế nào? |
| :--- | :--- | :--- | :--- |
| `pk_telemetry` | `telemetry_logs` | `(device_id, ts_rtc DESC)` | 1. Phân mảnh hypertable theo thời gian.<br>2. Tối ưu truy vấn phân trang và lấy bản ghi mới nhất.<br>3. Thực thi cơ chế khử trùng lặp khi đẩy bù từ thẻ nhớ. |
| `idx_telemetry_device_seq` | `telemetry_logs` | `(device_id, seq_no DESC)` | Đối soát nhanh tính liên tục của chuỗi gói tin `seq_no` khi thanh tra GSP. |
| `idx_alerts_device_time` | `alerts` | `(device_id, ts_rtc DESC)` | Truy vấn lịch sử cảnh báo vi phạm nhiệt độ của từng xe tải/kho lạnh. |
| `idx_audit_logs_created_at` | `audit_logs` | `(created_at DESC)` | Truy xuất nhanh nhật ký kiểm toán hệ thống từ mới nhất về cũ nhất. |