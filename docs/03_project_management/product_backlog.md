# Product Backlog

> **Cập nhật lần cuối:** 2026-11-15  
> **Trạng thái:** ✅ Done | 🔄 In Progress | 📋 Planned | ❌ Blocked  
> **Ưu tiên (MoSCoW):** **Must** | **Should** | **Could** | **Won't**

---

## 1. Hạng mục đã hoàn thành

### Sprint 0: Walking Skeleton

| ID | Hạng mục | Mô tả | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:------|:------|:-----------|
| PBI-00a | Kiến trúc 4 lớp và Data Contract | Thiết kế Canonical String, MQTT topics, DB schema, API spec | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00b | Firmware FreeRTOS dual-core | Core 0 đọc cảm biến giả lập và băm SHA-256; Core 1 quản lý WiFi, MQTT, LWT | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00c | WiFi 3 chế độ và Auto-Discovery | Cấu hình Captive Portal, lưu NVS Flash, AP Direct và UDP Discovery cổng 8888 | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00d | Backend MQTT Worker và FastAPI | Subscribe 4 topics, xác thực SHA-256, REST API, WebSocket push | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00e | TimescaleDB và Trigger bất biến | Cấu hình Hypertable, khử trùng lặp và khóa cứng `trg_immutable_telemetry` | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00f | React Dashboard thời gian thực | Hiển thị thẻ đo, bảng dữ liệu, nhấp nháy cảnh báo và kiểm toán chuỗi băm | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00g | Docker Compose 4 containers | Tự động dựng Mosquitto, TimescaleDB, Backend, Web kèm `.env.example` | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00h | Simulator kiểm thử 5 kịch bản | Chạy giả lập Normal, Alert, Sync, Tamper, LWT kiểm tra thông luồng E2E | TV4 | `feat/walking-skeleton` | ✅ Done |
| PBI-00i | Bộ tài liệu kỹ thuật ban đầu | Hoàn thành Technical Dossier, Risk Register và Schedule | TV4 | `feat/walking-skeleton` | ✅ Done |

### Sprint 1: Cảm biến thực I2C

| ID | Hạng mục | Mô tả | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:--------|:------|:------|:-----------|
| PBI-01 | Tích hợp SHT30 và SHT40 qua I2C | Đọc nhiệt độ và độ ẩm thực từ SHT30/SHT40 (0x44), kiểm tra CRC-8, cấu hình qua `platformio.ini` | **Must** | TV1 | `feat/sprint1-esp32` | ✅ Done |
| PBI-02 | Tích hợp RTC DS3231 qua I2C | Đọc timestamp UTC từ địa chỉ 0x68, cấu hình xung SQW 1Hz | **Must** | TV1 | `feat/sprint1-esp32` | ✅ Done |
| PBI-03 | Màn hình OLED SSD1306 | Hiển thị 2 trang: Thông số môi trường và Trạng thái mạng; chuyển trang bằng nút bấm | **Should** | TV1 | `feat/sprint1-esp32` | ✅ Done |
| PBI-04 | Ngắt GPIO cho 3 nút bấm | Bắt ngắt kèm Debounce 35ms: BTN1 đổi trang, BTN2 tắt còi, BTN3 giả lập quá nhiệt 9.5°C | **Should** | TV1 | `feat/sprint1-esp32` | ✅ Done |

---

## 2. Kế hoạch các Sprint tiếp theo

### Sprint 2: Lưu đệm offline và bảo mật

| ID | Hạng mục | Mô tả | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:--------|:------|:------|:-----------|
| PBI-05 | Đọc ghi thẻ MicroSD | Ghi đệm offline ra file CSV trên thẻ MicroSD qua giao tiếp SPI | **Must** | TV1 | `feat/fw-sdcard-sync` | 📋 Planned |
| PBI-06 | Auto-Sync khi có mạng lại | Đọc file CSV và đẩy bù qua topic `gsp/v1/+/sync`, cập nhật offset đồng bộ | **Must** | TV1 | `feat/fw-sdcard-sync` | 📋 Planned |
| PBI-07 | Lưu seq_no vào bộ nhớ | Lưu `seq_no` vào NVS Flash hoặc thẻ nhớ để tránh reset khi mất điện | **Must** | TV1 + TV4 | `feat/fw-sdcard-sync` | 📋 Planned |
| PBI-08 | Ký số ECDSA secp256r1 thật | Thay thế chữ ký dummy bằng mbedTLS ECDSA, tạo keypair và bảo vệ private key | **Must** | TV4 | `feat/sec-ecdsa-real` | 📋 Planned |
| PBI-09 | Backend xác thực ECDSA thật | Cập nhật `mqtt_worker.py` xác minh chữ ký ECDSA thật qua public key trong CSDL | **Must** | TV4 | `feat/sec-ecdsa-real` | 📋 Planned |

### Sprint 3: Cảnh báo Telegram và báo cáo

| ID | Hạng mục | Mô tả | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:--------|:------|:------|:-----------|
| PBI-10 | Telegram Bot cảnh báo | Gửi thông báo khẩn qua HTTPS khi vượt ngưỡng nhiệt hoặc phát hiện giả mạo | **Must** | TV2 | `feat/be-telegram-audit` | 📋 Planned |
| PBI-11 | Ghi nhật ký vào audit_logs | Lưu sự kiện tampering, vi phạm ngưỡng và kết nối thiết bị vào bảng `audit_logs` | **Should** | TV2 | `feat/be-telegram-audit` | 📋 Planned |
| PBI-12 | API lọc theo thời gian | Bổ sung query params `from` và `to` (ISO 8601) cho endpoint `/telemetry` | **Should** | TV2 | `feat/be-telegram-audit` | 📋 Planned |
| PBI-13 | Biểu đồ chuỗi thời gian | Vẽ đồ thị biến thiên nhiệt độ, độ ẩm và điện áp pin theo thời gian thực | **Must** | TV3 | `feat/web-chart-export` | 📋 Planned |
| PBI-14 | Xuất báo cáo CSV | Cho phép tải về file CSV dữ liệu đã lọc phục vụ thanh tra GSP | **Should** | TV3 | `feat/web-chart-export` | 📋 Planned |
| PBI-15 | Phân trang và bộ lọc lịch sử | Bổ sung date picker, bộ chọn thiết bị và phân trang con trỏ trên giao diện | **Should** | TV3 | `feat/web-chart-export` | 📋 Planned |

### Sprint 4: Bảo mật hạ tầng và kiểm thử

| ID | Hạng mục | Mô tả | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:--------|:------|:------|:-----------|
| PBI-16 | Xác thực Mosquitto | Cấu hình username/password cho MQTT Broker, cập nhật firmware và backend | **Should** | TV2 + TV4 | `feat/infra-security` | 📋 Planned |
| PBI-17 | Kiểm thử 3 mức | Chạy toàn bộ test plan (Unit, Integration, E2E) theo `test_plan_and_report.md` | **Must** | Cả nhóm | — | 📋 Planned |
| PBI-18 | Kiểm thử 4 kịch bản bất lợi | Thực nghiệm ADV-01 đến ADV-04 trên phần cứng thực tế và ghi nhận log | **Must** | Cả nhóm | — | 📋 Planned |
| PBI-19 | Bản tự đánh giá Rubric | Điền điểm tự đánh giá vào `rubric_self_assessment.md` theo tiêu chuẩn môn học | **Must** | TV4 | — | 📋 Planned |

### Sprint 5: Vận hành thử 7 ngày

| ID | Hạng mục | Mô tả | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:--------|:------|:------|:-----------|
| PBI-20 | Vận hành liên tục 7 ngày | Chạy hệ thống 24/7 và ghi nhật ký hàng ngày vào `trial_run_7days_log.md` | **Must** | Cả nhóm | — | 📋 Planned |
| PBI-21 | Đánh giá 3 chỉ tiêu tin cậy | Đo tỷ lệ nhận gói (PDR ≥ 99%), thời gian chạy liên tục (≥ 168h), số lần can thiệp | **Must** | TV4 | — | 📋 Planned |

### Sprint 6: Bàn giao và bảo vệ

| ID | Hạng mục | Mô tả | Ưu tiên | Owner | Nhánh | Trạng thái |
|:---|:---------|:------|:--------|:------|:------|:-----------|
| PBI-22 | Kiểm thử bàn giao chéo | Nhóm khác dựng lại hệ thống độc lập theo README để kiểm chứng tính tái lập | **Must** | Cả nhóm | — | 📋 Planned |
| PBI-23 | Báo cáo hoàn chỉnh | Tổng hợp toàn bộ hồ sơ kỹ thuật thành báo cáo đồ án | **Must** | TV4 | — | 📋 Planned |
| PBI-24 | Video demo dự phòng | Ghi hình demo E2E đề phòng sự cố mạng khi thuyết trình | **Should** | Cả nhóm | — | 📋 Planned |
| PBI-25 | Thu hồi mật khẩu mặc định | Đổi mật khẩu CSDL và MQTT sang chuẩn bảo mật trước khi bàn giao | **Must** | TV4 | — | 📋 Planned |

---

## 3. Quy trình làm việc với Backlog

1. **Trước Sprint:** TV4 review backlog, xác nhận mức độ ưu tiên và phân công nhiệm vụ.
2. **Trong Sprint:** Thành viên chủ động cập nhật trạng thái (📋 → 🔄 → ✅) cho PBI phụ trách.
3. **Phát sinh việc mới:** Bổ sung PBI vào cuối Sprint tương ứng và đánh số ID kế tiếp.
4. **Khi bị chặn (Blocked):** Đổi trạng thái ❌ Blocked, ghi rõ lý do và thảo luận trong buổi họp nhóm.
