# Quy trình Git và phối hợp nhóm

---

## 1. Chiến lược phân nhánh

```text
main (luôn chạy ổn định)
 │
 ├── [Merge feat/walking-skeleton]       ← Mốc Baseline
 │
 ├── feat/sprint1-esp32                  ← TV1: Cảm biến I2C, RTC, OLED, Nút bấm
 ├── feat/fw-sdcard-sync                 ← TV1 + TV4: Ghi đệm MicroSD offline
 ├── feat/sec-ecdsa-real                 ← TV4: Chữ ký ECDSA thực
 ├── feat/be-telegram-audit              ← TV2: Telegram Bot, audit_logs
 └── feat/web-chart-export               ← TV3: Biểu đồ, Xuất CSV
```

### Quy tắc bắt buộc:

1. **Không commit trực tiếp vào `main`.** Mọi thay đổi đều phải qua Pull Request (PR).
2. **Quy ước đặt tên nhánh:**
   - Tính năng mới: `feat/<module>-<tên>` (VD: `feat/sprint1-esp32`)
   - Sửa lỗi: `fix/<module>-<tên>` (VD: `fix/be-mqtt-reconnect`)
   - Tài liệu: `docs/<nội-dung>` (VD: `docs/update-api-spec`)
3. **Nhánh `main` luôn phải chạy được bằng lệnh `docker compose up -d --build`.**

---

## 2. Quy trình thực hiện task

### Bước 1: Đồng bộ nhánh chính
```bash
git checkout main
git pull origin main
```

### Bước 2: Tạo nhánh tính năng
```bash
git checkout -b feat/<module>-<ten-nhanh>
```

### Bước 3: Phát triển và tự kiểm thử
- Viết code, tự kiểm tra qua PlatformIO build, Docker Compose và Simulator.
- **Quy tắc Data Contract:** Nếu cần sửa payload MQTT, Canonical String hoặc REST API: trao đổi trước với TV4 và TV2, cập nhật `06_api_spec.md` trước khi sửa code.

### Bước 4: Commit theo Conventional Commits
Định dạng: `<type>(<scope>): <mô tả>`

| Type | Mục đích |
|:-----|:---------|
| `feat` | Thêm tính năng mới |
| `fix` | Sửa lỗi |
| `docs` | Cập nhật tài liệu |
| `refactor` | Tái cấu trúc code (không đổi hành vi) |
| `chore` | Cập nhật cấu hình, thư viện, Docker |

Ví dụ:
```text
feat(firmware): doc cam bien SHT30 qua I2C
fix(backend): xu ly mat ket noi TimescaleDB
docs(api): bo sung endpoint loc telemetry
```

### Bước 5: Rebase với `main`
```bash
git checkout main
git pull origin main
git checkout feat/<ten-nhanh>
git rebase main
```

### Bước 6: Push và mở Pull Request
```bash
git push -u origin feat/<ten-nhanh>
```
Mở PR vào `main` trên GitHub, nêu rõ:
- Mục tiêu giải quyết PBI nào.
- Các thay đổi chính trong code.
- Bằng chứng kiểm thử (ảnh chụp log, serial monitor hoặc dashboard).

### Bước 7: Review và Merge
- Ít nhất 1 reviewer và TV4 approve:
  - Firmware (TV1) → TV4 review.
  - Backend (TV2) → TV4 hoặc TV3 review.
  - Frontend (TV3) → TV2 review.
- Sử dụng **Squash and Merge** hoặc **Rebase and Merge** để giữ lịch sử git gọn gàng.
- Xóa nhánh sau khi merge thành công.

---

## 3. Tiêu chuẩn hoàn thành (DoD)

Một PBI chỉ được đánh dấu **✅ Done** khi thỏa mãn đủ 6 tiêu chí:

| # | Tiêu chí | Người kiểm tra |
|:-:|:---------|:--------------:|
| 1 | Code build thành công, không có lỗi biên dịch | Tác giả |
| 2 | Không làm gãy luồng End-to-End (Simulator chạy đủ 5 kịch bản) | Tác giả |
| 3 | Chạy sạch bằng lệnh `docker compose down -v && docker compose up -d --build` | Tác giả |
| 4 | Cập nhật đầy đủ tài liệu liên quan trong `docs/` | Tác giả |
| 5 | PR được ít nhất 1 reviewer và TV4 approve | TV4 |
| 6 | Đã cập nhật trạng thái trong `product_backlog.md` | Tác giả |

---

## 4. Xử lý sự cố sau Merge

1. Đưa `main` về trạng thái chạy được là ưu tiên số một.
2. Nếu không sửa được trong 30 phút: chạy `git revert <commit-merge>` ngay lập tức.
3. Tạo nhánh `fix/...` riêng để khắc phục sự cố rồi mở PR mới.
