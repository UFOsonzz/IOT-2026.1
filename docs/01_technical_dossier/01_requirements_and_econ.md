# (1) Đặc tả yêu cầu và bài toán kinh tế
*(Tài liệu thuộc Bước 1, Mục 1.6 và Mục 5.3.1 - Phụ trách chính: TV4)*

## 1. Bối cảnh, Đối tượng người dùng & Ranh giới pháp lý
- **Đối tượng người dùng:**
  1. Quản lý kho lạnh tại các công ty dược phẩm, tập đoàn chế biến nông/thủy sản và đơn vị vận chuyển hàng y tế tuyến huyện/vùng xa.
  2. Tài xế xe tải lạnh vận chuyển vaccine (+2.0°C đến +8.0°C).
  3. Thanh tra viên y tế và chuyên viên QA cần truy xuất báo cáo tuân thủ GSP (Good Storage Practices).
- **Ranh giới pháp lý:** Trong khuôn khổ học phần IT4735, đây là prototype phục vụ rèn luyện thiết kế hệ thống IoT có tính toàn vẹn dữ liệu. Khi triển khai thương mại phục vụ thanh tra GSP chính thức của Bộ Y tế, các cảm biến phải được kiểm định và hiệu chuẩn định kỳ tại đơn vị đo lường có thẩm quyền.

## 2. Bảng Yêu cầu Chức năng

| Mã YC | Tên chức năng | Mô tả chi tiết | Trạng thái |
| :--- | :--- | :--- | :--- |
| **FR-01** | Thu thập và xử lý vi khí hậu | Đọc dữ liệu nhiệt độ, độ ẩm từ cảm biến SHT30 và điện áp pin 18650 theo chu kỳ cài đặt. | Đã chạy thông khung |
| **FR-02** | Ghi đệm dữ liệu Offline | Khi mất kết nối mạng, tự động lưu bản ghi vào thẻ nhớ MicroSD tại biên kèm nhãn thời gian chuẩn UTC từ IC RTC DS3231. | `[TODO]` |
| **FR-03** | Đồng bộ hóa tự động | Khi có mạng lại, tự động đẩy dữ liệu Offline từ thẻ nhớ lên kênh `gsp/v1/+/sync` không làm gián đoạn luồng realtime và không trùng lặp bản ghi. | Đã có Dedup tại DB |
| **FR-04** | Cảnh báo khẩn cấp đa kênh | Kích hoạt Relay bật còi hú + đèn chớp tại chỗ tức thời (<= 3s), đẩy cảnh báo lên Web Dashboard và gọi API Telegram Bot khi vượt ngưỡng. | Đã có Relay & WS |
| **FR-05** | Đảm bảo tính bất biến | Vi điều khiển băm SHA-256 và ký số ECDSA cho mỗi gói tin; CSDL khóa cứng mọi lệnh `UPDATE` và `DELETE` (Append-Only). | Đã chạy thông |

## 3. Bảng Yêu cầu Phi chức năng

| Mã YC | Thuộc tính | Con số đo lường | Phương pháp kiểm chứng thực nghiệm | Kết quả nghiệm thu |
| :--- | :--- | :--- | :--- | :--- |
| **NFR-01** | Độ trễ cảnh báo | Còi hú tại chỗ kêu `<= 3 giây`; Cảnh báo tới Web và Telegram bot `<= 10 giây`. | Làm nóng cảm biến SHT30, dùng đồng hồ bấm giờ đo từ lúc vượt ngưỡng đến khi còi hú và nhận tin báo. | `[Điền sau]` |
| **NFR-02** | Chịu lỗi rớt mạng | Lưu trữ offline `100%` bản ghi phát sinh trong tối thiểu `7 ngày` mất mạng. | Ngắt Wi-Fi, sau đó cấp lại mạng; đối chiếu tổng số dòng trên file `.CSV` thẻ nhớ với số bản ghi `is_backfill=true` trên CSDL. | `[Điền sau]` |
| **NFR-03** | Độ chính xác | Nhiệt độ sai số `<= +-0.3°C`; Độ ẩm sai số `<= +-2.0% RH`. | Đặt đầu dò SHT30 cùng buồng đo với nhiệt kế chuẩn y tế đã kiểm định và điểm cân bằng nước đá đang tan (`0.0°C`). | `[Điền sau]` |
| **NFR-04** | Nguồn dự phòng | Hoạt động liên tục tối thiểu `>= 8 giờ` khi mất điện lưới toàn kho. | Rút nguồn USB 5V để mạch chạy bằng pin 18650; lấy hiệu số `ts_rtc` của bản ghi cuối cùng và bản ghi lúc rút nguồn. | `[Điền sau]` |

## 4. Bài toán kinh tế

- **Hiện trạng khi chưa có hệ thống:** Giá trị vaccine trung bình `2 tỷ đồng/xe` (`20 tỷ đồng` cho 10 xe). Rủi ro hỏng lô hàng do sự cố máy lạnh không phát hiện kịp trung bình `1,5%/năm` (`300 triệu đồng/năm`), cộng chi phí nhân công kiểm tra và ghi sổ tay 3 ca/ngày (`120 triệu đồng/năm`) -> Tổng chi phí thiệt hại & thủ công = **`420 triệu đồng/năm`**.

### 4.1. Chi phí đầu tư ban đầu (CapEx) và vận hành hằng năm (OpEx)

| Nhóm chi phí | Khoản mục chi tiết | Cách tính (Quy mô 10 điểm đo) | Thành tiền (Triệu đồng) |
| :--- | :--- | :--- | :--- |
| **CapEx** | Thiết bị biên công nghiệp (ESP32-S3, SHT30 có kiểm định, vỏ IP65, pin) | `2,5 triệu x 10 bộ` | `25,0` |
| **CapEx** | Thiết lập nền tảng Server, CSDL TimescaleDB & Tích hợp phần mềm | `1 gói trọn gói` | `20,0` |
| **CapEx** | Nhân công lắp đặt trên xe/kho, hiệu chuẩn ban đầu & Đào tạo | `1,0 triệu x 10 điểm` | `10,0` |
| **Tổng CapEx** | | | **`55,0 triệu đồng`** |
| **OpEx (năm)** | Cước SIM 4G / Truyền dẫn mạng | `30.000 đ/tháng x 10 điểm x 12 tháng` | `3,6 / năm` |
| **OpEx (năm)** | Thuê máy chủ Cloud VPS & Sao lưu dữ liệu bất biến GSP | `1,5 triệu/tháng x 12 tháng` | `18,0 / năm` |
| **OpEx (năm)** | Kiểm định & hiệu chuẩn cảm biến định kỳ hằng năm theo chuẩn GSP | `1,2 triệu/điểm/năm x 10 điểm` | `12,0 / năm` |
| **OpEx (năm)** | Bảo trì thiết bị, thay thế pin 18650 & xử lý sự cố | `~15% giá trị phần cứng + phụ phí` | `6,4 / năm` |
| **Tổng OpEx** | | | **`40,0 triệu đồng/năm`** |

- **Tổng chi phí sở hữu 5 năm (TCO 5 năm):** `TCO = 55 + (5 x 40) = 255 triệu đồng` (trong đó OpEx 5 năm chiếm `78,4%`).

### 4.2. Phân tích độ nhạy 2 kịch bản (Sensitivity Analysis)

| Kịch bản | Giả định mức cải thiện đạt được | Lợi ích thu hồi (Triệu/năm) | Dòng tiền ròng (= Lợi ích - OpEx) | Thời gian hoàn vốn (= CapEx / Dòng tiền ròng) | Đánh giá tính khả thi (Tuổi thọ TB 5-7 năm) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Kịch bản cơ sở** | Giảm 80% rủi ro hỏng thuốc (240 tr) + giảm 50% công ghi sổ (60 tr) | `300,0` | `260,0 triệu/năm` | **`0,21 năm (~2,5 tháng)`** | **Khả thi** |
| **Kịch bản thận trọng** | Chỉ giảm 15% rủi ro hỏng thuốc (45 tr) + không giảm nhân công | `45,0` | `5,0 triệu/năm` | **`11,0 năm`** | **Không khả thi** (Vượt tuổi thọ thiết bị) |