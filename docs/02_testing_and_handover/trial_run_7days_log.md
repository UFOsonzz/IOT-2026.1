# Nhật Ký Vận Hành Thử 7 Ngày Và Ba Chỉ Tiêu Độ Tin Cậy
*(Tài liệu thuộc Mục 5.2.10a,b & Bài tập 5.3 - Thực hiện ở Tuần 13)*

## 1. Bảng Nhật ký Vận hành thử 7 ngày liên tục

| Ngày chạy thử | (1) Số bản tin gửi Thành công / Thất bại | (2) Các lần mất kết nối (Thời điểm & Thời lượng) | (3) Thao tác can thiệp thủ công (nếu có) | (4) Hiện tượng bất thường quan sát được |
| :--- | :--- | :--- | :--- | :--- |
| **Ngày 1** | `... / ...` | `[Ghi nhận ở ...]` | `0 lần` | `[Ghi nhận ở ...]` |
| **Ngày 2** | `... / ...` | `[Ghi nhận ở ...]` | `0 lần` | `[Ghi nhận ở ...]` |

## 2. Tổng kết Ba Chỉ tiêu Độ tin cậy của Hệ thống

1. **Tỷ lệ bản tin đến đích (Packet Delivery Ratio):**
   $$\text{Tỷ lệ đến đích} = \frac{\text{Tổng số bản ghi lưu trong TimescaleDB}}{\text{Tổng số bản tin thiết bị đã phát sinh (theo seq\_no)}} \times 100\% = \dots\% \quad (\text{Mục tiêu} \ge 99\%)$$
2. **Thời gian hoạt động liên tục dài nhất không phải can thiệp (Max Continuous Uptime):** `... giờ` (Mục tiêu `>= 168 giờ`).
3. **Tổng số lần can thiệp thủ công trong toàn đợt vận hành thử:** `... lần`.