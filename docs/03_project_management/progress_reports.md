# Báo Cáo Tiến Độ Dự Án

> **Quy ước:** Cập nhật sau mỗi Sprint (mỗi 1-2 tuần). Mỗi mục ghi rõ: đã làm gì, kết quả đo được, vướng mắc, và kế hoạch tiếp theo.

---

## Sprint 0 — Tuần 2-7: Walking Skeleton

**Thời gian:** Tuần 2 → Tuần 7 (6 tuần)
**Mục tiêu:** Xây dựng khung chạy thông toàn bộ luồng End-to-End từ thiết bị biên đến giao diện Web, dùng dữ liệu giả lập (pseudo-sensor) để chứng minh kiến trúc hoạt động đúng.

### Kết quả đạt được

| Lớp | Thành phần | Kết quả |
|:----|:-----------|:--------|
| **Firmware** | `main.cpp` (367 dòng) + `WifiConfig.h` (400 dòng) | FreeRTOS dual-core: Core 0 đọc pseudo-sensor + SHA-256 mbedTLS, Core 1 WiFi/MQTT/LWT. 3 chế độ WiFi + Captive Portal + UDP Discovery. |
| **Infra** | `docker-compose.yml` + `init.sql` + `mosquitto.conf` | 4 Docker containers chạy ổn định. TimescaleDB Hypertable + Immutability Trigger hoạt động đúng. |
| **Backend** | `main.py` + `db.py` + `mqtt_worker.py` (tổng ~802 dòng) | FastAPI subscribe 4 MQTT topics, verify SHA-256, cursor pagination, WebSocket push, UDP Discovery listener. |
| **Frontend** | `App.tsx` (678 dòng) + TypeScript types | React Dashboard: KPI cards, telemetry table, alert flash đỏ, nút audit GSP verify-chain. Real-time qua WebSocket. |
| **Simulator** | `dummy_simulator.py` (340 dòng) | 5 kịch bản: Normal, Alert >8°C, Sync backfill, Tamper (sửa hash), LWT offline. |
| **Docs** | 8 tài liệu technical dossier + PM docs | Requirements, BOM, Architecture, Wiring, DB Schema, API Spec, Demo Script, Error Log, Risk Register, Schedule. |

### Các lỗi đã gặp và khắc phục

| Lỗi | Nguyên nhân | Cách sửa |
|:----|:-----------|:---------|
| ERR-01: UDP Discovery không hoạt động | Backend thiếu UDP listener port 8888 | Thêm `UdpDiscoveryProtocol` + mở port trong docker-compose |
| ERR-02: PostgreSQL auth failed | Mật khẩu không đồng bộ giữa .env, docker-compose, db.py | Thống nhất mật khẩu, document `docker compose down -v` |
| ERR-03: SHA-256 mismatch C++ vs Python | Floating-point format khác nhau (4.5 vs 4.50) | Định nghĩa Canonical String cố định `%.2f` |

### Đánh giá

- **Tiến độ:** ✅ Đúng kế hoạch — Walking skeleton chạy thông hoàn chỉnh.
- **Rủi ro phát sinh:** Không có rủi ro mới ngoài Risk Register.
- **Bài học chính:** Canonical String là hợp đồng dữ liệu bắt buộc khi hash cross-platform (C++ ↔ Python). Phải lock format trước khi code.

---

## Sprint 1 — Tuần 8: Cảm biến thực I2C

**Thời gian:** Tuần 8
**Mục tiêu:** Thay pseudo-sensor bằng SHT30 + DS3231 thực qua I2C. Hiển thị OLED cơ bản.
**Owner chính:** TV1 (Hardware Lead)

### Kết quả

| PBI | Hạng mục | Trạng thái | Ghi chú |
|:----|:---------|:-----------|:--------|
| PBI-01 | SHT30 qua I2C | Phần mềm hoàn tất; chờ test mạch thật | Đọc 0x44, CRC hai trường, chu kỳ 2 giây; xử lý NACK/short read/lỗi CRC; bỏ dữ liệu lỗi |
| PBI-02 | DS3231 RTC qua I2C | Phần mềm hoàn tất; chờ test mạch thật | UTC từ RTC, OSF/BCD/ngày hợp lệ, SQW1Hz trên GPIO27, đặt giờ qua Serial/NTP |
| PBI-03 | OLED SSD1306 cơ bản | Phần mềm hoàn tất; chờ test mạch thật | Hai trang cập nhật 500ms, cảm biến/RTC/network; pin N/A khi chỉ USB; phục hồi khi có màn |
| PBI-04 | GPIO Interrupt 3 nút | Phần mềm hoàn tất; chờ test mạch thật | Ngắt CHANGE GPIO32/33/25, debounce 35ms, đổi trang/ACK/test15s; giữ nút không lặp |

**Bàn giao phần mềm ngày 2026-11-12:** nhánh `feat/sprint1-esp32`, board ESP32-WROOM-32 (`esp32dev`).
Đã chuẩn bị test host có AddressSanitizer/UndefinedBehaviorSanitizer, build PlatformIO/Vite,
kiểm thử hệ thống trên thiết bị giả lập có ID riêng và kiểm tra Chromium nhận dữ liệu WebSocket.
Runner `tools/check_sprint1.py --browser` ghi log, mã thoát, JSON kết quả và ảnh vào thư mục evidence.
Kết quả thực thi chi tiết tại [báo cáo Sprint 1](../02_testing_and_handover/sprint1_test_report.md).

Hỗ trợ luồng thử thật: sửa Vite typings và quyền output build, tách `dist` container khỏi host,
chặn bản tin trùng/dữ liệu sync cũ ghi đè KPI, hiển thị đúng pin chưa đo và chữ ký dummy.

### Vướng mắc & Hỗ trợ cần thiết

- Người dùng chưa cắm ESP32; chưa upload, đo sai số, xem OLED hay nhấn nút trên mạch thật.
- Dùng [checklist HW-01→14](../02_testing_and_handover/sprint1_hardware_acceptance.md) khi có mạch;
  mẫu CSV đo chuẩn được giữ trống cho đến khi đo thật.
- Chưa có kết quả review/approve và dựng sạch trên máy khác. Giữ trạng thái nghiệm thu Sprint 1
  là `In Progress` theo DoD, dù phạm vi phần mềm PBI-01→04 đã hoàn tất.
- SD/offline sync/seq_no bền vững/ECDSA thật chuyển sang Sprint 2 đúng backlog.

