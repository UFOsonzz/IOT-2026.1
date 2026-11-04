# Nhật Ký Vận Hành Thử 7 Ngày Và Ba Chỉ Tiêu Độ Tin Cậy (Bước 10)
*(Tài liệu thuộc Mục 5.2.10a,b & Bài tập 5.3 - Thực hiện ở Tuần 13)*

## 1. Bảng Nhật ký Vận hành thử 7 ngày liên tục (4 nội dung bắt buộc mỗi ngày)

| Ngày chạy thử | (1) Số bản tin gửi Thành công / Thất bại | (2) Các lần mất kết nối (Thời điểm & Thời lượng) | (3) Thao tác can thiệp thủ công (nếu có) | (4) Hiện tượng bất thường quan sát được |
| :--- | :--- | :--- | :--- | :--- |
| **Ngày 1** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |
| **Ngày 2** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |
| **Ngày 3** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |
| **Ngày 4** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |
| **Ngày 5** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |
| **Ngày 6** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |
| **Ngày 7** | `... / ...` | `[Ghi nhận ở Tuần 13]` | `0 lần` | `[Ghi nhận ở Tuần 13]` |

## 2. Tổng kết Ba Chỉ tiêu Độ tin cậy của Hệ thống (Mục 5.2.10b)

1. **Tỷ lệ bản tin đến đích (Packet Delivery Ratio):**
   $$\text{Tỷ lệ đến đích} = \frac{\text{Tổng số bản ghi lưu trong TimescaleDB}}{\text{Tổng số bản tin thiết bị đã phát sinh (theo seq\_no)}} \times 100\% = \dots\% \quad (\text{Mục tiêu} \ge 99\%)$$
2. **Thời gian hoạt động liên tục dài nhất không phải can thiệp (Max Continuous Uptime):** `... giờ` (Mục tiêu `>= 168 giờ`).
3. **Tổng số lần can thiệp thủ công trong toàn đợt vận hành thử:** `... lần`.