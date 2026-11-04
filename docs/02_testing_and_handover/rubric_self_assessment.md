# Bản Tự Đánh Giá Theo Rubric Môn Học (Rubric Self-Assessment)

> **Học phần:** IT4735 - IoT và Ứng dụng (Đại học Bách Khoa Hà Nội)  
> **Đề tài:** Hệ thống IoT Giám sát chuỗi cung ứng lạnh tuân thủ GSP (Vaccine & Thủy sản đông lạnh)  
> **Thời điểm đánh giá:** Kết thúc giai đoạn Chạy thông luồng (Walking Skeleton / Sprint 0 — Chuẩn bị Giữa kỳ Tuần 8)

---

## 1. Bảng Tự Đánh Giá Chi Tiết Theo Tiêu Chí Rubric

| Tiêu chí | Trọng số | Mô tả yêu cầu của học phần | Hiện trạng tại mốc Chạy thông (Walking Skeleton) | Điểm tự đánh giá hiện tại | Kế hoạch hoàn thiện để đạt điểm tối đa (Cuối kỳ) |
|:---|:---:|:---|:---|:---:|:---|
| **1. Ý tưởng & Bài toán kinh tế kỹ thuật** | 10% | Bài toán thực tế, có phân tích đối tượng người dùng, tính khả thi, phân tích CapEx/OpEx, thời gian hoàn vốn và tính toán rủi ro. | Đã hoàn thành chi tiết trong `01_requirements_and_econ.md`: bài toán bảo quản vaccine/thủy sản GSP, CapEx 55tr, OpEx 40tr/năm, thời gian hoàn vốn 2.5 tháng, phân tích độ nhạy 2 kịch bản. | **9.5 / 10** | Bổ sung số liệu thực tế từ khảo sát giá vật tư công nghiệp khi đóng hộp mẫu thử. |
| **2. Kiến trúc hệ thống & Chuẩn truyền thông** | 15% | Kiến trúc tối thiểu 4 lớp, giải thích rõ ràng lý do chọn/bỏ công nghệ, giao thức mạng và giao thức ứng dụng phù hợp bài toán. | Hoàn thành trong `03_system_architecture.md`: Kiến trúc 4 lớp; luồng 1 chiều bảo vệ tính pháp lý; MQTT QoS 1 + LWT; TimescaleDB Hypertable; WebSocket push; bảng đối chiếu lập luận chọn/loại công nghệ. | **9.5 / 10** | Giữ vững kiến trúc, triển khai tiếp kênh Telegram Bot và xác thực MQTT TLS. |
| **3. Thiết kế Phần cứng & Firmware Biên** | 20% | Chọn linh kiện hợp lý; sơ đồ đấu nối chi tiết, chống xung đột chân; firmware đa nhiệm, xử lý rớt mạng và tiết kiệm năng lượng. | Khung firmware chạy FreeRTOS dual-core trên ESP32-S3-N16R8 (`main.cpp`, `WifiConfig.h`); băm SHA-256 phần cứng mbedTLS; 3 chế độ WiFi + UDP Discovery; sơ đồ chân `04_hardware_wiring.md` đã chốt và bảo vệ chân PSRAM/ADC2. | **7.5 / 10** | Nối cảm biến thực SHT30 + RTC DS3231 (Tuần 8); tích hợp đọc ghi MicroSD CSV khi offline và auto-sync (Tuần 9-10). |
| **4. Backend & Cơ sở dữ liệu Bất biến** | 20% | Đóng gói container hóa; lưu trữ chuỗi thời gian tối ưu; đảm bảo tính toàn vẹn và bất biến của dữ liệu; API đầy đủ. | Đã hoàn thành 100% trong `infra/db/init.sql` và `backend/`: TimescaleDB khóa cứng `UPDATE/DELETE` bằng trigger; khóa kép dedup; FastAPI REST cursor pagination; WebSocket push; API `/verify-chain` kiểm toán chữ ký số; Docker Compose 4 service. | **8.5 / 10** | Tích hợp thư viện chữ ký số ECDSA secp256r1 thực (thay thế dummy); kích hoạt ghi log bảng `audit_logs`. |
| **5. Giao diện Người dùng & Trực quan hóa** | 15% | Giao diện hiển thị trực quan dữ liệu thời gian thực; cảnh báo sự cố; hỗ trợ truy vấn lịch sử và xuất báo cáo. | Web Dashboard React 18 + TS (`web/src/App.tsx`): KPI cards, bảng telemetry real-time, nhấp nháy đỏ khi vượt ngưỡng hoặc bị TAMPERED, nút bấm kiểm toán chữ ký số GSP tức thời. | **8.0 / 10** | Tích hợp biểu đồ Recharts đường cong nhiệt độ/độ ẩm theo thời gian, bộ lọc lịch sử DatePicker và nút xuất file CSV. |
| **6. Thử nghiệm, Độ tin cậy & Kịch bản Bất lợi** | 10% | Kế hoạch kiểm thử ở các cấp độ; thử nghiệm các tình huống lỗi mạng, lỗi nguồn, tấn công dữ liệu; chạy thử dài ngày. | Đã có `dummy_simulator.py` giả lập 5 kịch bản (Normal, Alert, Sync, Tamper, LWT); đã lên kế hoạch kiểm thử 3 mức và 4 kịch bản bất lợi (`test_plan_and_report.md`). Đã xác nhận cơ chế dedup và trigger bất biến. | **7.5 / 10** | Thực hiện kiểm thử trên phần cứng thực ở Tuần 12; tiến hành chạy thử 7 ngày liên tục (`trial_run_7days_log.md`) ở Tuần 13. |
| **7. Quản lý Dự án & Hồ sơ Kỹ thuật** | 10% | Hồ sơ tài liệu đầy đủ, phân công rõ ràng, theo dõi tiến độ chặt chẽ, quản trị rủi ro và quản lý mã nguồn Git chuẩn mực. | Thư mục `docs/` phân chia khoa học 3 phân nhóm; đầy đủ `product_backlog.md`, `schedule_and_roles.md`, `risk_register.md`, `git_workflow_and_collaboration.md`, `meeting_minutes.md`. | **9.0 / 10** | Tiếp tục duy trì cập nhật tiến độ sau từng Sprint và hoàn thiện tài liệu nghiệm thu bàn giao (Tuần 14-15). |

---

## 2. Tổng Hợp Điểm Tự Đánh Giá

- **Điểm quy đổi tại mốc Walking Skeleton (Hiện tại):** **`8.3 / 10.0`** (Đạt mức Giỏi - Luồng chạy thông hoàn chỉnh, hồ sơ kiến trúc và công cụ kiểm thử đầy đủ).
- **Mục tiêu điểm số khi Nghiệm thu & Bảo vệ Đồ án (Tuần 15):** **`9.5 - 10.0 / 10.0`** (Xuất sắc - Phần cứng thực tế chạy ổn định 7 ngày, chữ ký số ECDSA hoàn chỉnh, báo cáo LaTeX chỉn chu).

---

## 3. Các Điểm Nổi Bật Tạo Sự Khác Biệt (Selling Points Của Nhóm)

1. **Tuân thủ Chuẩn GSP Thực sự:** Không chỉ đơn thuần đo nhiệt độ, hệ thống tập trung giải quyết bài toán cốt lõi trong y tế: **tính pháp lý và toàn vẹn của dữ liệu chuỗi thời gian**. Bằng chứng băm SHA-256 từ phần cứng mbedTLS kết hợp CSDL cấm sửa/xóa bằng Engine Trigger.
2. **Kiến trúc Khả dụng Cao (Fault-tolerant):** Hỗ trợ đệm dữ liệu ngoại tuyến (MicroSD) và cơ chế tự động bù dữ liệu (Auto-sync) không gây trùng lặp bản ghi nhờ khóa kép và `ON CONFLICT DO NOTHING`.
3. **Mạng Linh Hoạt Thông Minh:** Firmware hỗ trợ 3 chế độ mạng (Mạng chung kèm UDP Broadcast tự dò server, Mạng Hotspot máy tính, và Mạng AP độc lập) giúp việc demo tại phòng học không bao giờ bị phụ thuộc vào WiFi trường.
4. **Bộ Giả Lập Kiểm Thử Toàn Diện:** Bộ script `dummy_simulator.py` cho phép tái lập mọi kịch bản cực đoan (tấn công sửa số liệu, đứt cáp mạng, vượt ngưỡng nhiệt) ngay trên máy tính mà không cần chờ tác động vật lý.

