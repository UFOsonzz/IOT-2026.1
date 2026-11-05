# Quy Trình Làm Việc

> **Áp dụng:** khi hoàn thành xong luồng chạy thông.  

---

## 1. Chiến lược phân nhánh

```text
main (luôn ở trạng thái chạy được)
 │
 ├── [Merge feat/walking-skeleton]       ← Mốc Baseline
 │
 ├── feat/fw-i2c-sensors                 ← TV1: SHT30, DS3231, OLED, nút nhấn
 ├── feat/fw-sdcard-sync                 ← TV1 + TV4: MicroSD offline buffer
 ├── feat/sec-ecdsa-real                 ← TV4: Chữ ký ECDSA thực
 ├── feat/be-telegram-audit              ← TV2: Telegram Bot, audit_logs
 └── feat/web-chart-export               ← TV3: Biểu đồ, CSV export
```

### Quy tắc bắt buộc:

1. **KHÔNG commit trực tiếp vào `main`.** Mọi thay đổi đều qua Pull Request (PR).
2. **Quy ước đặt tên nhánh:**
   - Tính năng mới: `feat/<module>-<tên-ngắn>` (VD: `feat/fw-i2c-sensors`)
   - Sửa lỗi: `fix/<module>-<tên-lỗi>` (VD: `fix/be-mqtt-reconnect`)
   - Tài liệu: `docs/<nội-dung>` (VD: `docs/update-api-spec`)
3. **Nhánh `main` luôn phải khởi động được bằng `docker compose up -d --build`.**

---

## 2. Quy trình từng bước cho mỗi nhiệm vụ

### Bước 1: Kéo code mới nhất
```bash
git checkout main
git pull origin main
```

### Bước 2: Tạo nhánh tính năng
```bash
git checkout -b feat/<module>-<ten-tinh-nang>
```

### Bước 3: Phát triển & Tự kiểm thử
- Code tính năng, tự chạy PlatformIO build / Docker Compose / Simulator để xác nhận.
- **Quy tắc Data Contract:** Nếu cần sửa cấu trúc bản tin MQTT, Canonical String, hoặc REST API → **Dừng lại, thảo luận với TV4 và TV2 trước**, cập nhật `06_api_spec.md` trước khi sửa code.

### Bước 4: Commit theo chuẩn Conventional Commits
Format: `<type>(<scope>): <mô tả ngắn>`

| Type | Khi nào dùng |
|:-----|:-------------|
| `feat` | Thêm chức năng mới |
| `fix` | Sửa lỗi |
| `docs` | Sửa tài liệu |
| `refactor` | Tái cấu trúc (không đổi hành vi) |
| `chore` | Cập nhật config, thư viện, Docker |

Ví dụ:
```
feat(firmware): doc cam bien SHT30 qua I2C0 GPIO8/9
fix(backend): xu ly ngoai le mat ket noi TimescaleDB
docs(api): bo sung endpoint loc telemetry theo thoi gian
```

### Bước 5: Rebase với `main` trước khi tạo PR
```bash
git checkout main
git pull origin main
git checkout feat/<ten-nhanh>
git rebase main
# Giải quyết conflict nếu có, rồi kiểm tra lại docker compose up
```

### Bước 6: Push và tạo Pull Request
```bash
git push -u origin feat/<ten-nhanh>
```
Mở PR trên GitHub hướng vào `main`, điền:
- **Mục tiêu:** Giải quyết PBI nào trong Backlog?
- **Các thay đổi chính:** Liệt kê file/hàm đã sửa.
- **Bằng chứng kiểm thử:** Ảnh chụp Terminal, Log Serial Monitor, hoặc ảnh Dashboard.

### Bước 7: Code Review & Merge
- **Bắt buộc ít nhất 1 reviewer + TV4 approve:**
  - Firmware (TV1 code) → TV4 review.
  - Backend (TV2 code) → TV4 hoặc TV3 review.
  - Frontend (TV3 code) → TV2 review.
- Dùng **Squash and Merge** hoặc **Rebase and Merge** để giữ `main` gọn.
- **Xóa nhánh sau khi merge thành công.**

---

## 3. Tiêu chuẩn Hoàn thành (Definition of Done)

Một PBI chỉ được đánh dấu **✅ Done** khi thỏa mãn **tất cả** các điều kiện:

| # | Tiêu chí | Ai kiểm tra |
|:-:|:---------|:----------:|
| 1 | Code build không lỗi (PlatformIO / Vite / Python) | Tác giả |
| 2 | Không phá vỡ luồng E2E hiện tại (Simulator vẫn chạy đúng 5 kịch bản) | Tác giả |
| 3 | `docker compose down -v && docker compose up -d --build` chạy sạch trên máy khác thành công | Tác giả |
| 4 | Cập nhật docs liên quan (API spec, wiring, schema, ...) | Tác giả |
| 5 | PR được ít nhất 1 người review + TV4 approve | TV4 |
| 6 | Đã cập nhật trạng thái trong `product_backlog.md` | Tác giả |

---

## 4. Xử lý khi merge vào `main` gây lỗi

1. **Ưu tiên tuyệt đối** là đưa `main` về trạng thái chạy được, không code thêm tính năng mới.
2. Nếu không fix được trong **30 phút**: chạy `git revert <commit-vừa-merge>` ngay lập tức.
3. Tác giả tạo nhánh `fix/...` riêng để sửa lỗi và tạo PR lại.
