# Sprint 1 — Báo cáo kiểm tra phần mềm

Ngày đối chiếu: **2026-11-12**. Board đích: **ESP32-WROOM-32 / esp32dev**, flash 4 MB, không PSRAM.

**Kết luận:** PBI-01→04 đã có code; build firmware/Web và test host đã qua.
**Chưa xác nhận phần mềm đạt toàn bộ:** lượt kiểm thử hệ thống có Chromium đã timeout,
chưa hoàn tất LWT, các kiểm tra DB/audit cuối luồng và trình duyệt.
Không có kết quả upload hay nghiệm thu phần cứng; người dùng chưa cắm ESP32.

## 1. Kết quả có bằng chứng

| Hạng mục | Kết quả thực tế | Bằng chứng |
|---|---|---|
| Cấu hình Compose | PASS, exit 0 | [compose_config.log](evidence/sprint1-2026-11-12/compose_config.log) |
| Test logic và driver trên host | PASS, 10 nhóm kiểm tra; bật AddressSanitizer và UndefinedBehaviorSanitizer | [host_tests.log](evidence/sprint1-2026-11-12/host_tests.log) |
| Build firmware esp32dev | PASS; RAM 51.268 / 327.680 byte (15,6%); flash 1.178.014 / 1.310.720 byte (89,9%) | [firmware_build.log](evidence/sprint1-2026-11-12/firmware_build.log) |
| Build Web (TypeScript + Vite) | PASS bằng lệnh npm run build thông thường | [web_build.log](evidence/sprint1-2026-11-12/web_build.log) |
| Liệt kê USB | Script chạy thành công; **không có ESP32**, không phải PASS phần cứng | [usb_ports.log](evidence/sprint1-2026-11-12/usb_ports.log) |
| Kiểm thử hệ thống | **INCOMPLETE / runner FAIL**: timeout 240 giây; log có 10 bước PASS trước khi dừng | [system_tests.log](evidence/sprint1-2026-11-12/system_tests.log) |
| Kiểm tra lại hệ thống | **BLOCKED ở môi trường**: Docker daemon hiện không chạy; chưa vào được các bài kiểm tra | [system_results.json](evidence/sprint1-2026-11-12-recheck/system_results.json) |
| Nạp và đo trên mạch thật | PENDING theo yêu cầu người dùng | [Checklist HW-01→14](sprint1_hardware_acceptance.md) |

Kết quả tổng hợp do runner ghi: [software_results.json](evidence/sprint1-2026-11-12/software_results.json),
`status=FAIL`, bước `system_tests` exit 1. Timeout không đủ để quy kết một lỗi sản phẩm cụ thể;
cũng không đủ để đánh dấu cả luồng đã đạt. Chưa có ảnh Dashboard của lượt test này.

Artifact firmware hiện có: `firmware/.pio/build/esp32dev/firmware.bin`.
Kích thước file 1.205.792 byte; SHA-256:
`9c3499e4e9d754a933e2ec95dec3405f1409e98b94197d7eac3d1f5b07574764`.
Kích thước file BIN và số byte flash do PlatformIO báo là hai số đo khác nhau.

## 2. Phạm vi đã kiểm tra

Host test chạy trực tiếp `Sprint1Logic.h` và `Sprint1Hardware.cpp` với I2C/GPIO/OLED mô phỏng:

- SHT30: CRC theo vector mẫu, đổi số đo, short read, lỗi CRC/NACK và phục hồi.
- DS3231: UTC, BCD, ngày nhuận, 12h/24h, OSF, khoảng năm 2024–2099, đặt giờ và cảnh báo SQW.
- HMI: ngắt đến debounce, giữ nút không lặp, đổi trang, TEST 9,50°C trong 15 giây, ACK giữ alert.
- OLED/pin/Serial: phục hồi màn, pin không đo dùng sentinel 0, parser có giới hạn, đồng bộ NTP vào RTC.

Log hệ thống trước timeout xác nhận 10 bước: tạo node mô phỏng riêng, UDP discovery,
ONLINE, telemetry định dạng ESP32 với pin=0, cảnh báo 9,50°C, ba bản ghi backfill và khử trùng,
phát hiện payload sửa đổi, đúng sáu bản ghi, OFFLINE, và khôi phục ONLINE.
ID test: `GSP-S1-f1265282208b`; dữ liệu không được ghi vào ID ESP32 `GSP-TRUCK-01`.

**Chưa có kết quả đạt** cho các bước tiếp theo trong bộ test mới: LWT do tiến trình thoát,
khôi phục sau LWT, alert trong DB, từ chối UPDATE/DELETE, chain audit và Chromium/WebSocket
(gồm chống sync cũ/trùng, hiển thị pin chưa đo, kiểm tra lỗi JavaScript).

## 3. Phần mềm đã chuẩn bị để người dùng thử

PBI-01→04 có driver SHT30, DS3231/SQW, hai trang OLED và ba nút ngắt.
Platform ESP32 và ba thư viện được cố định phiên bản trong `firmware/platformio.ini`.
Serial helper có đặt giờ UTC, STATUS định kỳ, lưu log thật và báo lỗi nếu không nhận xác nhận đặt RTC.
Web có Vite typings, volume dist riêng, hiển thị đúng GPIO34/pin chưa đo/chữ ký dummy,
và xử lý dữ liệu sync cũ/bản tin bị DB từ chối. Các thay đổi UI sau cùng cần hoàn tất regression trình duyệt.

Các thay đổi đang nằm trên nhánh `feat/sprint1-esp32`, chưa có commit Sprint 1 hoặc bằng chứng review/approve.
Không đánh dấu Done theo DoD trước khi hoàn tất xác nhận cần thiết.

## 4. Cách kiểm tra tiếp

Khi Docker daemon đã chạy, từ thư mục gốc repo:

```bash
docker compose up -d --build
.venv/bin/python tools/check_sprint1.py --browser --evidence-dir docs/02_testing_and_handover/evidence/sprint1-next-run
```

Nếu cần cài công cụ: dùng `tools/requirements-sprint1-browser.txt`, sau đó
`.venv/bin/python -m playwright install chromium`.
Giữ log thất bại cũ để đối chiếu; thư mục `sprint1-next-run` ghi kết quả lượt mới.
Chỉ chuyển kết luận phần mềm sang PASS khi toàn bộ bước được yêu cầu có exit 0.

Sau đó thực hiện [hướng dẫn nạp/test](sprint1_esp32_guide.md) và
[checklist phần cứng](sprint1_hardware_acceptance.md).
CSV đo chuẩn vẫn để trống tới khi đo thật. Review/approve và dựng sạch trên máy khác chưa được xác nhận.
MicroSD/offline sync từ ESP32, seq_no bền vững và ECDSA thật thuộc Sprint 2.
