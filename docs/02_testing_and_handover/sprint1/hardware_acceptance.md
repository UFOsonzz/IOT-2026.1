# Sprint 1 — Checklist kiểm tra mạch thật

Ngày chuẩn bị: **2026-11-12**. Trạng thái ban đầu: **PENDING toàn bộ**, chưa kết nối ESP32.
Đây là biểu mẫu dành cho người test, không phải kết quả đã đo. Hướng dẫn dây và xử lý lỗi:
[sprint1_esp32_guide.md](sprint1_esp32_guide.md).

## 1. Nạp và lấy log đầu tiên

Đấu dây khi đã rút nguồn: SDA21, SCL22, SQW27; BTN1/2/3=32/33/25; các module I2C dùng 3V3, GND chung.
Thay `/dev/ttyUSB0` bằng cổng mới xuất hiện sau khi cắm board. Đóng Serial Monitor trước khi nạp/mở script.

```bash
.venv/bin/python tools/sprint1_serial.py --list
.venv/bin/pio run -d firmware -e esp32dev -t upload --upload-port /dev/ttyUSB0
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --set-time --command SCAN --command STATUS --seconds 30 --log docs/02_testing_and_handover/evidence/hardware/initial_serial.log
```

## 2. Kết quả nghiệm thu

Điền `PASS` hoặc `FAIL`, tên file log/ảnh và nhận xét sau mỗi phép thử. Không đánh dấu PASS bằng kết quả host/mock.

| Mã | PBI | Phép thử và điều kiện đạt | Kết quả | Bằng chứng / ghi chú |
|---|---|---|---|---|
| HW-01 | Chung | Nạp thành công đúng ESP32 classic; Serial 115200 đọc được | PENDING | |
| HW-02 | Chung | SCAN thấy 0x3C, 0x44, 0x68 | PENDING | |
| HW-03 | 01 | SHT30=OK; số đo thay đổi với môi trường; không có lỗi CRC lặp | PENDING | |
| HW-04 | 01 | Đo cạnh thiết bị chuẩn; sai số nhiệt độ ≤0,3°C và độ ẩm ≤2%RH; ghi vào CSV | PENDING | |
| HW-05 | 02 | TIME được xác nhận; RTC=OK; epoch tăng khoảng 10 sau 10 giây | PENDING | |
| HW-06 | 02 | SQW tăng khoảng 10 sau 10 giây; không có SQW WARN | PENDING | |
| HW-07 | 02 | RTC giữ giờ khi rút nguồn 1 phút; có pin backup; kiểm tra trước NTP/TIME | PENDING | |
| HW-08 | 03 | OLED trang 1 đúng nhiệt độ/ẩm; pin N/A khi chỉ dùng USB; trang 2 đúng Wi-Fi/MQTT/IP | PENDING | |
| HW-09 | 04 | BTN1 đổi trang đúng một lần mỗi nhấn; giữ 2 giây không tự lặp | PENDING | |
| HW-10 | 04 | BTN3 cho 9,50°C/TEST/alert=1, ngõ GPIO26 bật; hết 15 giây trả về số đo thật | PENDING | |
| HW-11 | 04 | BTN2 tắt ngõ báo động nhưng alert vẫn giữ; cảnh báo mới bật lại | PENDING | |
| HW-12 | Chung | Thiếu SHT30/RTC: báo lỗi, không phát telemetry giả; thiếu OLED: Serial/đọc cảm biến vẫn chạy | PENDING | |
| HW-13 | E2E | Dữ liệu từ ESP32 GSP-TRUCK-01 xuất hiện ở API và Web; thời gian và số đo khớp Serial | PENDING | |
| HW-14 | E2E | BTN3/ACK/hết TEST phản ánh đúng trên Web; ACK chỉ tắt còi tại chỗ | PENDING | |

Tắt nguồn trước khi thay đổi dây để thử HW-12. Cảm biến ở phòng >8°C báo động là đúng ngưỡng 2–8°C.
Lệnh WAKE/ACK/TEST trên Serial chỉ kiểm tra chức năng; HW-09→11 cần nhấn nút thật để xác nhận dây/ngắt.
Đo nhiệt độ và độ ẩm bằng thiết bị chuẩn thích hợp; chưa có thiết bị chuẩn thì giữ HW-04=PENDING.
Mẫu ghi đo: [sprint1_measurements.csv](sprint1_measurements.csv).

## 3. Chạy luồng thật

```bash
docker compose up -d --build
curl 'http://localhost:8000/api/v1/devices/GSP-TRUCK-01/telemetry?limit=5'
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --command STATUS --seconds 60 --log docs/02_testing_and_handover/evidence/hardware/e2e_serial.log
```

Cấu hình Option 1 bằng AP của ESP32, dùng Wi-Fi 2,4 GHz cùng LAN với server; cho phép UDP8888/TCP1883.
Mở `http://localhost:5173/` và chụp Dashboard, đối chiếu `seq_no`/UTC/số đo với log Serial và API.
Không chạy simulator với ID GSP-TRUCK-01 trong phiên này. `Publish OK` chưa đủ để xác nhận đã lưu DB.

## 4. Chốt Sprint

Người thực hiện: **chưa ghi nhận**. Board/module thực tế: **chưa ghi nhận**. Ngày kiểm tra: **chưa ghi nhận**.

Khi các phép thử đạt, cập nhật báo cáo Sprint 1, backlog và progress report với bằng chứng tương ứng.
Review/approve và phép thử dựng sạch trên máy khác theo DoD vẫn do nhóm xác nhận; không suy ra từ kiểm thử trên máy phát triển.
MicroSD, offline sync từ ESP32 và ECDSA thật thuộc Sprint 2; không cần triển khai các phần đó để chạy checklist này.
