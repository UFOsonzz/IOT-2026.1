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

## Sprint 1 — Tuần 8: Cảm biến thực I2C (Midterm)

**Thời gian:** Tuần 8
**Mục tiêu:** Thay pseudo-sensor bằng SHT30 + DS3231 thực qua I2C. Hiển thị OLED cơ bản.
**Owner chính:** TV1 (Hardware Lead)

### Kết quả

| PBI | Hạng mục | Trạng thái | Ghi chú |
|:----|:---------|:-----------|:--------|
| PBI-01 | SHT30 qua I2C | `[Cập nhật sau]` | |
| PBI-02 | DS3231 RTC qua I2C | `[Cập nhật sau]` | |
| PBI-03 | OLED SSD1306 cơ bản | `[Cập nhật sau]` | |
| PBI-04 | GPIO Interrupt 3 nút | `[Cập nhật sau]` | |

### Vướng mắc & Hỗ trợ cần thiết

- `[Cập nhật sau mỗi buổi họp tuần]`

