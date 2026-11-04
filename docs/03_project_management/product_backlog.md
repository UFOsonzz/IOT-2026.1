# Product Backlog

> **Cập nhật lần cuối:** 2026-11-04 (sau khi merge `feat/walking-skeleton` → `main`)
>
> **Quy ước trạng thái:** ✅ Done | 🔄 In Progress | 📋 Planned | ❌ Blocked
>
> **Quy ước ưu tiên (MoSCoW):** **M**ust | **S**hould | **C**ould | **W**on't

---

## 1. Các hạng mục đã hoàn thành (Walking Skeleton — Sprint 0)

| ID | Hạng mục | Mô tả | Owner | Nhánh | Trạng thái |
|:---|:---------|:-------|:------|:------|:-----------|
| PBI-00a | Kiến trúc 4 lớp & Data Contract | Thiết kế Canonical String, MQTT topics, DB schema, API spec | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00b | Firmware khung FreeRTOS dual-core | Core 0 đọc pseudo-sensor + SHA-256; Core 1 WiFi/MQTT + LWT | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00c | WiFi 3 Options + UDP Discovery | Captive Portal, NVS Flash, AP Direct, UDP Broadcast 8888 | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00d | Backend MQTT Worker + FastAPI | Subscribe 4 topics, verify SHA-256, REST API, WebSocket push | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00e | TimescaleDB + Immutability Trigger | Hypertable, ON CONFLICT dedup, trg_immutable_telemetry | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00f | React Dashboard real-time | KPI cards, telemetry table, alert flash, audit button, WS | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00g | Docker Compose 4 containers | Mosquitto + TimescaleDB + Backend + Web, .env.example | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00h | Dummy Simulator 5 kịch bản | Normal, Alert, Sync, Tamper, LWT — kiểm thử toàn luồng E2E | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00i | Tài liệu kỹ thuật giai đoạn 1 | 8 tài liệu technical dossier + risk register + schedule | TV4 | `feat/walking-skeleton` | ✅ Done |

---

## 2. Backlog giai đoạn tiếp theo (Sprint 1 → Final)

### Sprint 1 — Tuần 8: Cảm biến thực I2C (Midterm Checkpoint)

| ID | Hạng mục | Mô tả chi tiết | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:----------------|:--------|:------|:------|:-----------|
| PBI-01 | Tích hợp SHT30 qua I2C | Đọc nhiệt độ & độ ẩm thực từ SHT30 (0x44), thay thế pseudo random. Kiểm tra sai số với nhiệt kế chuẩn. | **Must** | TV1 | `feat/fw-i2c-sensors` | 📋 Planned |
| PBI-02 | Tích hợp DS3231 RTC qua I2C | Đọc timestamp UTC thực từ DS3231 (0x68), thay thế `millis()`. Cấu hình SQW 1Hz interrupt trên GPIO14. | **Must** | TV1 | `feat/fw-i2c-sensors` | 📋 Planned |
| PBI-03 | Hiển thị OLED SSD1306 cơ bản | Trang 1: Nhiệt độ/Ẩm/Pin real-time. Trang 2: Trạng thái WiFi/MQTT. Chuyển trang bằng BTN1 (GPIO5). | **Should** | TV1 | `feat/fw-i2c-sensors` | 📋 Planned |
| PBI-04 | GPIO Interrupt cho 3 nút HMI | Chuyển từ polling sang `attachInterrupt()` với debounce. BTN1=OLED, BTN2=ACK/Mute, BTN3=Test GSP. | **Should** | TV1 | `feat/fw-i2c-sensors` | 📋 Planned |

### Sprint 2 — Tuần 9-10: Offline Buffering & Bảo mật thực

| ID | Hạng mục | Mô tả chi tiết | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:----------------|:--------|:------|:------|:-----------|
| PBI-05 | MicroSD đọc/ghi file CSV | Ghi đệm offline ra file `.CSV` trên thẻ MicroSD qua SPI2 (GPIO 10-13). Đáp ứng FR-02. | **Must** | TV1 | `feat/fw-sdcard-sync` | 📋 Planned |
| PBI-06 | Auto-Sync từ MicroSD khi có mạng | Đọc file CSV, đẩy bù qua topic `gsp/v1/+/sync`, cập nhật `sync_byte_offset`. Đáp ứng FR-03. | **Must** | TV1 | `feat/fw-sdcard-sync` | 📋 Planned |
| PBI-07 | Persist seq_no vào NVS/SD | Lưu `seq_no` vào NVS Flash hoặc SD card, tránh reset khi mất điện. | **Must** | TV1 + TV4 | `feat/fw-sdcard-sync` | 📋 Planned |
| PBI-08 | ECDSA secp256r1 ký số thực | Thay `DUMMY_SIG_OK_ESP32S3_SECP256R1` bằng chữ ký thật dùng mbedTLS. Sinh keypair, lưu private key an toàn. | **Must** | TV4 | `feat/sec-ecdsa-real` | 📋 Planned |
| PBI-09 | Backend verify ECDSA thực | Cập nhật `mqtt_worker.py` verify chữ ký ECDSA thực thay vì check dummy string. Lưu public key vào bảng `devices`. | **Must** | TV4 | `feat/sec-ecdsa-real` | 📋 Planned |

### Sprint 3 — Tuần 10-11: Telegram, Audit & Dashboard nâng cao

| ID | Hạng mục | Mô tả chi tiết | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:----------------|:--------|:------|:------|:-----------|
| PBI-10 | Telegram Bot cảnh báo | Tạo bot, gửi HTTPS POST khi vượt ngưỡng GSP hoặc phát hiện tamper. Hoàn thiện FR-04 đa kênh. | **Must** | TV2 | `feat/be-telegram-audit` | 📋 Planned |
| PBI-11 | Ghi audit_logs vào CSDL | INSERT sự kiện tampering, threshold violation, device online/offline vào bảng `audit_logs`. | **Should** | TV2 | `feat/be-telegram-audit` | 📋 Planned |
| PBI-12 | API lọc theo khoảng thời gian | Thêm query params `from` và `to` (ISO 8601) cho endpoint `/telemetry`. | **Should** | TV2 | `feat/be-telegram-audit` | 📋 Planned |
| PBI-13 | Biểu đồ nhiệt độ/ẩm real-time | Chart.js hoặc Recharts vẽ đồ thị time-series nhiệt độ, độ ẩm, pin theo thời gian. | **Must** | TV3 | `feat/web-chart-export` | 📋 Planned |
| PBI-14 | Xuất CSV báo cáo GSP | Nút "Xuất CSV" tải về file telemetry đã lọc theo khoảng thời gian cho thanh tra viên. | **Should** | TV3 | `feat/web-chart-export` | 📋 Planned |
| PBI-15 | Bộ lọc lịch sử & phân trang UI | Date picker, thiết bị selector, infinite scroll hoặc "Load more" cho bảng telemetry. | **Should** | TV3 | `feat/web-chart-export` | 📋 Planned |

### Sprint 4 — Tuần 11-12: Bảo mật hạ tầng & Kiểm thử hệ thống

| ID | Hạng mục | Mô tả chi tiết | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:----------------|:--------|:------|:------|:-----------|
| PBI-16 | Mosquitto authentication | Cấu hình username/password cho MQTT broker. Cập nhật firmware và backend. | **Should** | TV2 + TV4 | `feat/infra-security` | 📋 Planned |
| PBI-17 | Kiểm thử 3 mức (Unit/Integration/E2E) | Chạy đầy đủ test plan theo `test_plan_and_report.md`. Ghi kết quả cột "Week 12 Result". | **Must** | All | — | 📋 Planned |
| PBI-18 | Kiểm thử 4 kịch bản bất lợi | ADV-01→ADV-04 với phần cứng thực. Ghi evidence vào cột "Hardware Evidence". | **Must** | All | — | 📋 Planned |
| PBI-19 | Rubric tự đánh giá | Điền `rubric_self_assessment.md` theo rubric của học phần. | **Must** | TV4 | — | 📋 Planned |

### Sprint 5 — Tuần 13: Vận hành thử 7 ngày

| ID | Hạng mục | Mô tả chi tiết | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:----------------|:--------|:------|:------|:-----------|
| PBI-20 | Chạy thử 7 ngày liên tục | Vận hành hệ thống 24/7, ghi nhật ký hàng ngày vào `trial_run_7days_log.md`. | **Must** | All | — | 📋 Planned |
| PBI-21 | Tính 3 chỉ tiêu độ tin cậy | Packet Delivery Ratio ≥ 99%, Max Uptime ≥ 168h, số lần can thiệp thủ công. | **Must** | TV4 | — | 📋 Planned |

### Sprint 6 — Tuần 14-15: Bàn giao & Bảo vệ

| ID | Hạng mục | Mô tả chi tiết | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:----------------|:--------|:------|:------|:-----------|
| PBI-22 | Handover test với nhóm khác | Nhóm khác dựng lại hệ thống từ README → đánh giá tính reproducible. | **Must** | All | — | 📋 Planned |
| PBI-23 | Báo cáo LaTeX hoàn chỉnh | Tổng hợp tất cả tài liệu docs/ thành báo cáo LaTeX nộp bài. | **Must** | TV4 | — | 📋 Planned |
| PBI-24 | Video demo dự phòng | Quay video demo E2E đề phòng WiFi phòng thi không hoạt động (RR-01). | **Should** | All | — | 📋 Planned |
| PBI-25 | Đổi toàn bộ mật khẩu mặc định | Thay mật khẩu DB, MQTT trong `.env` trước bàn giao. | **Must** | TV4 | — | 📋 Planned |

---

## 3. Hướng dẫn sử dụng Product Backlog

1. **Trước mỗi Sprint:** TV4 (Coordinator) review backlog, xác nhận ưu tiên và phân công.
2. **Trong Sprint:** Mỗi thành viên tự cập nhật trạng thái (📋 → 🔄 → ✅) cho các PBI được giao.
3. **Khi phát sinh công việc mới:** Thêm PBI mới vào cuối Sprint tương ứng, gán ID tăng dần.
4. **Khi bị chặn:** Đổi trạng thái thành ❌ Blocked, ghi rõ lý do và báo trong buổi họp tuần.

