# Đặc tả yêu cầu và bài toán kinh tế

Phụ trách: TV4

---

## 1. Bối cảnh và người dùng

- **Người dùng mục tiêu:**
  1. Quản lý kho lạnh dược phẩm, nông sản, thủy sản và điều phối vận chuyển y tế.
  2. Tài xế xe lạnh vận chuyển vaccine (+2.0°C đến +8.0°C).
  3. Thanh tra y tế và QA kiểm tra tuân thủ GSP.
- **Ranh giới pháp lý:** Hệ thống là prototype học phần IT4735, tập trung vào thiết kế toàn vẹn dữ liệu IoT. Khi triển khai thương mại cho thanh tra GSP của Bộ Y tế, cảm biến cần được hiệu chuẩn định kỳ tại cơ quan đo lường có thẩm quyền.

---

## 2. Yêu cầu chức năng

| Mã | Chức năng | Mô tả chi tiết | Trạng thái |
| :--- | :--- | :--- | :--- |
| **FR-01** | Thu thập vi khí hậu | Đọc nhiệt độ, độ ẩm từ SHT30/SHT40 và điện áp pin theo chu kỳ. | Đã hoàn thành trên mạch thật |
| **FR-02** | Lưu đệm offline | Khi mất mạng, lưu bản ghi vào thẻ MicroSD kèm timestamp UTC từ RTC DS3231. | Kế hoạch Sprint 2 |
| **FR-03** | Tự động đồng bộ | Khi có mạng, đẩy bù dữ liệu từ thẻ nhớ lên topic `gsp/v1/+/sync`, tự khử trùng lặp tại CSDL. | Đã xử lý Dedup tại DB |
| **FR-04** | Cảnh báo đa kênh | Kích hoạt Relay bật còi và đèn tại chỗ (≤ 3s), đẩy cảnh báo lên Web Dashboard và Telegram Bot khi vượt ngưỡng. | Đã có Relay và WebSocket |
| **FR-05** | Toàn vẹn dữ liệu | Vi điều khiển băm SHA-256 và ký ECDSA từng gói tin; CSDL append-only chặn lệnh `UPDATE` và `DELETE`. | Đã chạy thông |

---

## 3. Yêu cầu phi chức năng

| Mã | Thuộc tính | Tiêu chuẩn | Phương pháp kiểm chứng | Kết quả nghiệm thu |
| :--- | :--- | :--- | :--- | :--- |
| **NFR-01** | Độ trễ cảnh báo | Còi hú tại chỗ ≤ 3s; cảnh báo Web và Telegram ≤ 10s. | Gia nhiệt SHT30/SHT40, đo thời gian từ lúc vượt ngưỡng đến khi kích hoạt còi và nhận tin báo. | Điền sau |
| **NFR-02** | Chịu lỗi mất mạng | Lưu offline 100% dữ liệu phát sinh trong tối thiểu 7 ngày mất kết nối. | Ngắt WiFi, cấp lại mạng và đối chiếu số dòng CSV trên thẻ nhớ với số bản ghi `is_backfill=true` trong CSDL. | Điền sau |
| **NFR-03** | Độ chính xác | Nhiệt độ sai số ≤ ±0.3°C; độ ẩm sai số ≤ ±2.0% RH. | Đặt đầu dò SHT30/SHT40 cùng nhiệt kế chuẩn y tế và điểm đá tan (0.0°C). | Điền sau |
| **NFR-04** | Nguồn dự phòng | Hoạt động liên tục ≥ 8 giờ khi mất nguồn chính. | Rút nguồn USB 5V cho mạch chạy pin 18650, so sánh `ts_rtc` bản ghi cuối cùng với thời điểm rút nguồn. | Điền sau |

---

## 4. Bài toán kinh tế

- **Hiện trạng chưa có hệ thống:** Giá trị vaccine trung bình 2 tỷ đồng/xe (20 tỷ đồng cho 10 xe). Rủi ro hỏng hàng do sự cố lạnh khoảng 1.5%/năm (300 triệu đồng/năm), chi phí ghi sổ tay thủ công 3 ca/ngày là 120 triệu đồng/năm. Tổng tổn thất: **420 triệu đồng/năm**.

### 4.1. Chi phí đầu tư (CapEx) và vận hành hàng năm (OpEx)

| Phân loại | Khoản mục | Quy mô tính toán (10 điểm đo) | Thành tiền (Triệu đồng) |
| :--- | :--- | :--- | :--- |
| **CapEx** | Thiết bị biên (ESP32/ESP32-S3, SHT30/SHT40, vỏ IP65, pin) | 2.5 triệu x 10 bộ | 25.0 |
| **CapEx** | Máy chủ, CSDL TimescaleDB và phần mềm | Gói hoàn thiện | 20.0 |
| **CapEx** | Lắp đặt, hiệu chuẩn ban đầu và đào tạo | 1.0 triệu x 10 điểm | 10.0 |
| **Tổng CapEx** | | | **55.0** |
| **OpEx/năm** | Cước SIM 4G và truyền dẫn | 30.000 đ/tháng x 10 điểm x 12 tháng | 3.6 |
| **OpEx/năm** | Thuê Cloud VPS và backup dữ liệu GSP | 1.5 triệu/tháng x 12 tháng | 18.0 |
| **OpEx/năm** | Kiểm định và hiệu chuẩn cảm biến định kỳ theo chuẩn GSP | 1.2 triệu/điểm/năm x 10 điểm | 12.0 |
| **OpEx/năm** | Bảo trì thiết bị, thay pin 18650 và xử lý sự cố | ~15% giá trị phần cứng | 6.4 |
| **Tổng OpEx** | | | **40.0/năm** |

- **Tổng chi phí sở hữu 5 năm (TCO 5 năm):** `TCO = 55 + (5 x 40) = 255 triệu đồng` (OpEx chiếm 78.4%).

### 4.2. Phân tích độ nhạy

| Kịch bản | Giả định cải thiện | Lợi ích thu hồi (Triệu/năm) | Dòng tiền ròng (Lợi ích - OpEx) | Thời gian hoàn vốn (CapEx / Dòng tiền ròng) | Đánh giá khả thi |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Cơ sở** | Giảm 80% rủi ro hỏng thuốc (240 tr) + giảm 50% công ghi sổ (60 tr) | 300.0 | 260.0 triệu/năm | **0.21 năm (~2.5 tháng)** | **Khả thi** |
| **Thận trọng** | Giảm 15% rủi ro hỏng thuốc (45 tr), không giảm nhân công | 45.0 | 5.0 triệu/năm | **11.0 năm** | **Không khả thi** (vượt tuổi thọ thiết bị) |
