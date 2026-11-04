# Phân công vai trò và kế hoạch tiến độ
*(Tài liệu thuộc Mục 5.2.11, 5.4.1 và 5.6.1 - Phụ trách chính: TV4 - Lead)*

## 1. Phân công vai trò

| Thành viên | Vai trò | Phạm vi phụ trách | Hồ sơ chịu trách nhiệm |
| :--- | :--- | :--- | :--- |
| **TV1** | Phụ trách Thiết bị | Mạch phần cứng và Firmware | `02_bom_hardware_selection.md`, `04_hardware_wiring.md`, mục lỗi phần cứng trong `08_error_and_lesson_log.md` |
| **TV2** | Phụ trách Backend | Mosquitto Broker, TimescaleDB Schema, FastAPI Worker, REST/WS API, Telegram Bot | `05_database_schema.md`, `06_api_spec.md`, `06_postman_collection.json`, mục lỗi DB/API trong `08_error_and_lesson_log.md` |
| **TV3** | Phụ trách Giao diện | React + TypeScript Dashboard, Biểu đồ thời gian thực, Bộ lọc lịch sử, Xuất CSV | `07_ui_and_demo_script.md`, Phần 4 (HDSD người vận hành) trong `handover_dossier.md` |
| **TV4** | Điều phối và Tích hợp | Kiến trúc tổng thể, Bảo mật SHA-256/ECDSA, Simulator, Kiểm thử, Quản lý Git & Báo cáo | `01_requirements_and_econ.md`, `03_system_architecture.md`, toàn bộ thư mục `02_testing_and_handover/` và `03_project_management/` |

## 2. Kế hoạch tiến độ

| Tuần | Mốc quy trình | Công việc | Người phụ trách | Trạng thái |
| :--- | :--- | :--- | :--- | :--- |
| **Tuần 2–4** | Bước 1, 2, 3, 4 | Chốt đề tài GSP Cold-Chain, đặc tả FR/NFR, đặt mua linh kiện, thiết kế kiến trúc 4 lớp và chọn công nghệ. | Cả nhóm | `[x] Đã hoàn thành` |
| **Tuần 5–7** | Bước 5, 6, 7 (Chạy thông) | Xây dựng nhánh `feat/walking-skeleton` chạy thông End-to-End từ ESP32-S3 -> MQTT -> FastAPI -> TimescaleDB -> React UI + Simulator. | TV4 | `[x] Đã hoàn thành` |
| **Tuần 8** | **Bài tập 5.2 (Giữa kỳ)** | Nhánh `feat/fw-i2c-sensors`: Kết nối cảm biến SHT30 & DS3231 thật qua I2C; Demo luồng dữ liệu thật giữa kỳ. | TV1 + Cả nhóm | `[ ] Đang thực hiện` |
| **Tuần 9–10** | Bước 5 & 6 hoàn thiện | Nhánh `feat/fw-sdcard-sync` (ghi/đẩy bù thẻ MicroSD) và nhánh `feat/sec-ecdsa-real` (ký & xác thực ECDSA `secp256r1` thật). | TV1, TV4 | `[ ] Kế hoạch` |
| **Tuần 10–11** | Bước 6 & 7 hoàn thiện | Nhánh `feat/be-telegram-audit` (cảnh báo Telegram Bot, lọc `from/to`, ghi `audit_logs`) và nhánh `feat/web-chart-export` (vẽ biểu đồ nhiệt độ, xuất CSV). | TV2, TV3 | `[ ] Kế hoạch` |
| **Tuần 12** | Bước 8 (Kiểm thử) | Thực hiện kiểm thử 3 mức + 4 kịch bản bất lợi (`test_plan_and_report.md`) và **Tự chấm điểm theo Rubric 100 điểm**. | Cả nhóm | `[ ] Kế hoạch` |
| **Tuần 13** | Bước 10 (Vận hành thử) | Cho hệ thống chạy liên tục 7 ngày, ghi nhật ký hằng ngày và tính 3 chỉ tiêu độ tin cậy (`trial_run_7days_log.md`). | Cả nhóm | `[ ] Kế hoạch` |
| **Tuần 14–15** | Bước 9 & **Bài tập 5.3** | Thực hiện Phép thử bàn giao với nhóm khác, hoàn tất báo cáo LaTeX, quay video demo dự phòng và Bảo vệ đồ án. | Cả nhóm | `[ ] Kế hoạch` |

