# Phân công vai trò và kế hoạch tiến độ

Phụ trách chính: TV4

---

## 1. Phân công vai trò

| Thành viên | Vai trò | Phạm vi phụ trách | Hồ sơ chịu trách nhiệm |
| :--- | :--- | :--- | :--- |
| **TV1** | Thiết bị & Phần cứng | Mạch phần cứng và Firmware | `02_bom_hardware_selection.md`, `04_hardware_wiring.md`, lỗi phần cứng trong `08_error_and_lesson_log.md` |
| **TV2** | Backend & Dữ liệu | Mosquitto, TimescaleDB, FastAPI, REST/WS API, Telegram Bot | `05_database_schema.md`, `06_api_spec.md`, `06_postman_collection.json`, lỗi backend trong `08_error_and_lesson_log.md` |
| **TV3** | Frontend & Trực quan | React Dashboard, Biểu đồ thời gian thực, Bộ lọc lịch sử, Xuất CSV | `07_ui_and_demo_script.md`, hướng dẫn vận hành trong `handover_dossier.md` |
| **TV4** | Trưởng nhóm & Tích hợp | Kiến trúc tổng thể, Mật mã SHA-256/ECDSA, Simulator, Kiểm thử, Quản lý Git & Báo cáo | `01_requirements_and_econ.md`, `03_system_architecture.md`, toàn bộ thư mục `02_testing_and_handover/` và `03_project_management/` |

---

## 2. Kế hoạch tiến độ

| Mốc | Nội dung công việc | Người phụ trách | Trạng thái |
| :--- | :--- | :--- | :--- |
| **Khởi động** | Chốt đề tài GSP Cold-Chain, đặc tả FR/NFR, chọn linh kiện, thiết kế kiến trúc 4 lớp | Cả nhóm | `[x] Đã hoàn thành` |
| **Sprint 0** | Nhánh `feat/walking-skeleton`: chạy thông End-to-End từ ESP32 qua MQTT, FastAPI, TimescaleDB đến React UI và Simulator | TV4 | `[x] Đã hoàn thành` |
| **Sprint 1 (Checkpoint 1)** | Nhánh `feat/sprint1-esp32`: Tích hợp SHT30/SHT40, DS3231, OLED và nút ngắt trên ESP32 Classic và ESP32-S3 | TV1 + Cả nhóm | `[x] Đã hoàn thành` |
| **Sprint 2** | Nhánh `feat/fw-sdcard-sync` (ghi đệm MicroSD) và `feat/sec-ecdsa-real` (ký và xác thực ECDSA secp256r1 thật) | TV1, TV4 | `[ ] Kế hoạch` |
| **Sprint 3** | Nhánh `feat/be-telegram-audit` (Telegram Bot, lọc thời gian, audit_logs) và `feat/web-chart-export` (biểu đồ, xuất CSV) | TV2, TV3 | `[ ] Kế hoạch` |
| **Sprint 4** | Kiểm thử 3 mức, thực nghiệm 4 kịch bản bất lợi và tự chấm điểm Rubric | Cả nhóm | `[ ] Kế hoạch` |
| **Sprint 5** | Vận hành hệ thống liên tục 7 ngày, ghi nhật ký và tính 3 chỉ tiêu độ tin cậy | Cả nhóm | `[ ] Kế hoạch` |
| **Sprint 6** | Kiểm thử bàn giao chéo với nhóm khác, hoàn tất báo cáo và bảo vệ đồ án | Cả nhóm | `[ ] Kế hoạch` |
