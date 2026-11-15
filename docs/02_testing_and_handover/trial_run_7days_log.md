# Nhật ký vận hành 7 ngày và chỉ tiêu độ tin cậy

Kế hoạch thực hiện tại Tuần 13 (Sprint 5).

---

## 1. Nhật ký vận hành 7 ngày liên tục

| Ngày | Số bản tin Thành công / Thất bại | Sự cố mất kết nối (Thời điểm & Thời lượng) | Can thiệp thủ công | Hiện tượng bất thường |
| :--- | :--- | :--- | :--- | :--- |
| **Ngày 1** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |
| **Ngày 2** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |
| **Ngày 3** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |
| **Ngày 4** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |
| **Ngày 5** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |
| **Ngày 6** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |
| **Ngày 7** | `... / ...` | `[Ghi nhận ở ...]` | 0 lần | `[Ghi nhận ở ...]` |

---

## 2. Ba chỉ tiêu độ tin cậy

1. **Tỷ lệ nhận gói tin (Packet Delivery Ratio):**
   $$\text{PDR} = \frac{\text{Tổng bản ghi trong TimescaleDB}}{\text{Tổng bản tin thiết bị phát sinh (theo seq\_no)}} \times 100\% = \dots\% \quad (\text{Mục tiêu} \ge 99\%)$$
2. **Thời gian chạy liên tục không cần can thiệp (Max Continuous Uptime):** `... giờ` (Mục tiêu ≥ 168 giờ).
3. **Số lần can thiệp thủ công trong toàn đợt thử nghiệm:** `... lần` (Mục tiêu 0 lần).
