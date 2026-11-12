# Sprint 1 — Đấu nối và kiểm thử ESP32 thường

Áp dụng cho **ESP32-WROOM-32 / ESP32 DevKit / NodeMCU-32S hai lõi**, cấu hình PlatformIO `esp32dev`, flash 4 MB, không cần PSRAM. Board S2/S3/C3/C6 có sơ đồ chân khác và không nằm trong cấu hình này. Nhánh triển khai: `feat/sprint1-esp32`, tạo từ `main`.

Phần mềm đã chuẩn bị để nạp; nghiệm thu mạch thật theo [checklist](sprint1_hardware_acceptance.md).
Không cần ESP32 để chạy bộ kiểm thử phần mềm tại mục cuối. Phiên bản platform và ba thư viện firmware được cố định trong `platformio.ini` theo bản đã build.

## 1. Bốn phần đã triển khai

| Backlog | Firmware làm gì | Cách quan sát |
|---|---|---|
| PBI-01 — SHT30 | Gửi lệnh đo `0x2400` tới `0x44` mỗi 2 giây, chờ ít nhất 20 ms, đọc 6 byte và kiểm tra CRC riêng cho nhiệt độ/độ ẩm. Không dùng số ngẫu nhiên. | OLED trang 1, dòng `[STATUS]`, telemetry trên Web |
| PBI-02 — DS3231 | Đọc ngày giờ UTC từ thanh ghi của `0x68`; kiểm tra OSF, BCD và ngày hợp lệ. Đặt SQW 1 Hz, đếm cạnh xuống bằng ngắt GPIO27. | `RTC=OK`, `epoch=...`, bộ đếm `SQW` tăng khoảng 1 mỗi giây |
| PBI-03 — OLED | SSD1306 I2C 128×64 tại `0x3C`, cập nhật mỗi 500 ms. Trang 1: nhiệt độ/ẩm/pin/cảnh báo. Trang 2: Wi-Fi/MQTT/IP/SQW. | BTN1 đổi trang; dòng cuối hiển thị UTC epoch hoặc lỗi RTC |
| PBI-04 — HMI | `attachInterrupt(..., CHANGE)` cho ba nút; chống dội bằng cách đợi mức ổn định 35 ms. ISR chỉ ghi sự kiện; task xử lý hiển thị/còi. Giữ nút không tự lặp. | BTN1 đổi trang, BTN2 ACK/Mute, BTN3 test 9,50°C trong 15 giây |

`firmware/include/HardwareConfig.h` là nơi khai báo chân và chu kỳ. `Sprint1Logic.h` chứa CRC, đổi ngày UTC và chống dội. `Sprint1Hardware.cpp` điều khiển ngoại vi. `src/main.cpp` giữ hai task: Core 0 đọc cảm biến/HMI/OLED và tạo bản tin; Core 1 quản lý Wi-Fi/MQTT.

Telemetry định kỳ 10 giây. Đổi trạng thái cảnh báo, bắt đầu/kết thúc test hoặc đặt RTC sẽ yêu cầu gửi sớm hơn; hai bản tin không dùng cùng một giây RTC. Hợp đồng JSON, MQTT topics và canonical string giữ nguyên. Chữ ký vẫn là **dummy của Sprint 0**; ECDSA thật và lưu SD thuộc Sprint 2.

## 2. Chuẩn bị và nối dây

Thử tối thiểu cần ESP32 thường, cáp USB **có dây dữ liệu**, SHT30, DS3231, OLED **SSD1306 128×64 I2C**, breadboard/dây nối. Ba nút nhấn giúp test HMI thật; khi chưa có nút, có thể dùng lệnh Serial để thử chức năng tương ứng. Relay, còi và bộ pin là tùy chọn ở bước đầu.

**Rút USB trước khi đấu dây.** Nhìn theo tên `GPIO/IO` in trên board, không đếm vị trí chân. Cấp cả ba module I2C từ **3V3**, nối chung GND. Không kéo SDA/SCL/SQW lên 5 V.

| Chân module | Chân ESP32 thường | Ghi chú |
|---|---|---|
| VCC/VIN của SHT30, DS3231, OLED | 3V3 | Kiểm tra đúng thứ tự VCC/GND trên từng module |
| GND của cả ba module | GND | GND chung |
| SDA của cả ba module | GPIO21 | Ba dây nối song song |
| SCL của cả ba module | GPIO22 | OLED đôi khi ghi `SCK` thay `SCL` |
| SQW/INT của DS3231 | GPIO27 | Không nối nhầm chân `32K` |
| BTN1 | GPIO32 ↔ nút ↔ GND | Đổi trang OLED |
| BTN2 | GPIO33 ↔ nút ↔ GND | Tắt còi tại chỗ |
| BTN3 | GPIO25 ↔ nút ↔ GND | Test GSP |
| Relay IN, nếu có | GPIO26 | Module kích HIGH, nhận được logic 3,3 V |
| LED trạng thái | GPIO2 | Dùng LED có sẵn nếu board có; không bắt buộc |

```text
ESP32           SHT30            DS3231           OLED SSD1306
3V3 ----------- VCC ------------- VCC ------------- VCC
GND ----------- GND ------------- GND ------------- GND
GPIO21 -------- SDA ------------- SDA ------------- SDA
GPIO22 -------- SCL ------------- SCL ------------- SCL
GPIO27 ------------------------- SQW

GPIO32 ---- [BTN1] ---- GND
GPIO33 ---- [BTN2] ---- GND
GPIO25 ---- [BTN3] ---- GND
```

Nút tact 4 chân có hai cặp chân nối sẵn bên trong: chọn hai chân chỉ thông nhau khi nhấn, có thể kiểm tra bằng đồng hồ. Không đưa VCC vào nút. Firmware đã bật pull-up nội. SDA/SCL cần pull-up lên 3,3 V; nhiều breakout có sẵn. SQW là open-drain, dùng pull-up nội; nếu dây dài/nhiễu, dùng thêm trở 4,7–10 kΩ lên 3V3.

Chưa cần lắp pin, thẻ SD hay tải công suất để test Sprint 1. Có thể quan sát ngõ alarm bằng LED nối **GPIO26 → điện trở 330 Ω → anode LED**, cathode xuống GND. Không nối trực tiếp còi công suất/cuộn relay vào GPIO. Relay module cấp nguồn theo thông số module, GND chung, tải còi qua COM/NO; không dùng điện lưới trong bài test này.

## 3. Cài công cụ, build và nạp

Chạy tại thư mục gốc repo:

```bash
cd /home/tam/Projects/IOT-2026.1
python -m venv .venv
.venv/bin/python -m pip install -r tools/requirements-sprint1.txt

# Chỉ liệt kê cổng thiết bị, chưa nạp
.venv/bin/python tools/sprint1_serial.py --list

# Build đúng ESP32 thường
.venv/bin/pio run -d firmware -e esp32dev
```

Nếu môi trường hiện tại đã có `.venv`, bỏ bước tạo lại. Có thể dùng `uv pip install --python .venv/bin/python -r tools/requirements-sprint1.txt` khi máy dùng uv. Trên Windows, dùng `.venv\Scripts\python.exe` và `.venv\Scripts\pio.exe` thay các đường dẫn `.venv/bin/...`.

Cắm ESP32 vào USB, chạy lại `--list`; ví dụ cổng mới là `/dev/ttyUSB0`. Thay cổng trong lệnh sau bằng cổng **thực tế**:

```bash
.venv/bin/pio run -d firmware -e esp32dev -t upload --upload-port /dev/ttyUSB0
.venv/bin/pio device monitor -d firmware -p /dev/ttyUSB0 -b 115200
```

Windows thường là `COM3`, macOS thường là `/dev/cu.usbserial-*`. Không chạy hai chương trình cùng mở một cổng. Thoát monitor bằng `Ctrl+C` trước khi dùng script Serial hoặc nạp lại.

Trong VS Code: cài extension **PlatformIO IDE**, dùng **Open Project** chọn thư mục `firmware/`; trong Project Tasks chọn `esp32dev → General → Build`, rồi `Upload`, rồi `Monitor`. Không chọn môi trường N16R8 hay Arduino Nano. Nếu dùng Arduino IDE, mã nguồn này là project PlatformIO có nhiều file, nên hướng dẫn kiểm thử sử dụng PlatformIO để lấy đúng thư viện/cờ build.

Nếu nạp dừng ở `Connecting...`, giữ nút **BOOT**, bấm Upload, nhả BOOT khi bắt đầu ghi flash; nạp xong bấm **EN/RESET** nếu cần. Baud nạp đã để 115200 để tăng khả năng tương thích USB. Không tự đoán hoặc ghi firmware khác khi esptool báo chip không phải ESP32 classic.

## 4. Test cục bộ — chưa cần Docker hay Wi-Fi

### 4.1. Kiểm tra I2C

Sau reset, Serial phải có dòng quét tương tự bên dưới. Đây là **ví dụ kết quả mong đợi**, không phải log của thiết bị đã đo:

```text
[I2C] Found: 0x3C 0x44 0x68
[RTC] SQW configured: 1Hz
[READY] Sprint1. HELP for commands; RTC must store UTC.
```

Một số module DS3231 có EEPROM phụ, có thể thấy thêm địa chỉ `0x57`; không ảnh hưởng. Nếu chỉ thấy `0x45`, kiểm tra chân ADDR của SHT30 để đưa về `0x44`. Màn SH1106 hoặc SSD1306 128×32 cần driver/cấu hình khác; không thể kết luận màn hỏng chỉ vì không hiện đúng trên cấu hình 128×64 này.

Trong monitor có gửi lệnh, dùng kết thúc dòng LF/Newline. Bộ lệnh:

| Lệnh | Tác dụng |
|---|---|
| `HELP` | In danh sách lệnh |
| `SCAN` | Quét I2C và cấu hình lại SQW 1 Hz |
| `STATUS` | In trạng thái cảm biến, RTC, SQW, OLED, pin, alert, mute và test |
| `TIME <epoch UTC>` | Đặt DS3231 theo UTC, chấp nhận năm 2024–2099 |
| `WAKE` | Thử chức năng BTN1 qua Serial |
| `ACK` | Thử chức năng BTN2 qua Serial |
| `TEST` | Thử chức năng BTN3 qua Serial |

Hoặc đóng monitor rồi dùng script:

```bash
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --command SCAN --command STATUS --seconds 15
```

### 4.2. Đặt giờ UTC lần đầu và test RTC

RTC mới hoặc mất pin có thể báo `RTC=INVALID`; khi đó thiết bị vẫn đọc SHT30, hiển thị OLED và xử lý nút nhưng **không gửi telemetry có thời gian giả**. Cách đặt giờ không cần Internet:

```bash
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --set-time --command STATUS --seconds 15 --log docs/02_testing_and_handover/evidence/hardware/rtc_serial.log
```

Script lấy Unix epoch hiện tại của máy tính; không cộng thêm 7 giờ. Giờ máy tính cần đúng. Xác nhận `[RTC SET OK]` và `RTC=OK`. Khi Wi-Fi có Internet, NTP đồng bộ DS3231 tự động một lần mỗi lần khởi động; mọi timestamp telemetry sau đó vẫn lấy từ DS3231. Mất Internet không làm RTC ngừng chạy.

Script gửi lại `STATUS` mỗi 5 giây và lưu các dòng Serial nhận được khi dùng `--log`.
`--status-interval 0` tắt việc hỏi lặp. Khi dùng `--set-time`, không nhận được `[RTC SET OK]` trong thời gian theo dõi sẽ trả exit code 1; không tự coi gửi lệnh là đã đặt giờ thành công.

Hai dòng STATUS cách nhau 10 giây: `epoch` và `SQW` đều tăng khoảng 10. Nếu epoch tăng nhưng SQW đứng yên, kiểm tra dây SQW→GPIO27; firmware báo `[SQW WARN]` và vẫn đọc thanh ghi RTC mỗi giây để chẩn đoán. **Có cảnh báo SQW chưa được coi là đạt test dây/ngắt RTC.** Sau khi đấu lại an toàn, dùng `SCAN` để bật lại SQW nếu cần.

Kiểm tra giữ giờ: lắp pin backup đúng loại cho module RTC, đặt giờ, rút nguồn ESP32 khoảng 1 phút, cấp nguồn lại và đọc STATUS **không dùng `--set-time`, không cho NTP đồng bộ**. Giờ phải tiếp tục từ thời điểm trước mất điện. Module có mạch sạc pin cần dùng pin phù hợp hoặc vô hiệu hóa mạch sạc trước khi dùng CR2032 không sạc.

### 4.3. Test SHT30

OLED trang 1 cập nhật nhiệt độ/ẩm khoảng mỗi 2 giây. Đặt đầu dò ở vị trí khác hoặc hơ tay gần đầu dò để thấy số đo đổi; tránh chạm tay vào vùng cảm biến. Không kỳ vọng luôn thấy 4,5°C: trong phòng 25–30°C, `alert=1` và relay bật là **đúng** với ngưỡng bảo quản 2–8°C.

Kiểm tra sai số PBI-01/NFR-03 cần đo thật: đặt SHT30 và nhiệt kế chuẩn cạnh nhau trong cùng môi trường, cách xa ESP32/nguồn nóng; chờ ổn định, ghi ít nhất 10 cặp số đo cách nhau 10 giây ở nhiệt độ phòng và môi trường lạnh. Tính `sai số = SHT30 − giá trị chuẩn`, đối chiếu yêu cầu `|sai số nhiệt độ| ≤ 0,3°C`. Độ ẩm cần ẩm kế chuẩn và tiêu chí `≤ 2%RH`; chỉ có nhiệt kế thì chưa kiểm chứng được phần độ ẩm. Không nhúng cảm biến trần vào nước/đá; phép thử nước đá cần đầu dò/bao bảo vệ thích hợp và đánh giá thời gian cân bằng nhiệt.

Mẫu ghi thực nghiệm: [sprint1_measurements.csv](sprint1_measurements.csv). Các ô đo để trống cho đến khi có thiết bị thật; không tự thêm bù nhiệt để làm kết quả đạt.

### 4.4. Test ba nút và OLED

1. **BTN1, GPIO32:** nhấn nhả một lần → trang 1 sang trang 2; nhấn lần nữa → trang 1. Giữ 2 giây chỉ đổi một lần. Serial in một dòng `[HMI] BTN1 page=...` cho mỗi lần nhấn ổn định.
2. **BTN3, GPIO25:** nhấn nhả → OLED hiện `TEST`, nhiệt độ **9.50°C**, `alert=1`, relay GPIO26 bật; độ ẩm vẫn là SHT30 thật. Không cần giữ nút. Sau 15 giây, tự trả về nhiệt độ SHT30 và yêu cầu gửi mẫu cập nhật.
3. **BTN2, GPIO33:** nhấn trong lúc báo động → relay tắt, OLED hiện `MUTE`; dữ liệu vẫn giữ `alert=1` khi nhiệt độ đang cao. Cảnh báo mới khác loại hoặc sau khi hết rồi tái phát sẽ bật còi trở lại. BTN3 cũng chủ động bỏ mute để test lại.
4. Nếu phòng vẫn >8°C, kết thúc TEST vẫn có cảnh báo cao; để thử trạng thái bình thường, đặt cảm biến trong môi trường **2–8°C**. Không sửa ngưỡng hoặc dùng số giả để làm mất cảnh báo phòng nóng.

Test bằng lệnh trước khi có nút:

```bash
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --command TEST --seconds 20
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --command ACK --seconds 5
.venv/bin/python tools/sprint1_serial.py --port /dev/ttyUSB0 --command WAKE --seconds 5
```

Lệnh Serial kiểm tra chức năng; vẫn cần nhấn nút vật lý để xác nhận dây/ngắt/debounce. Để test ACK trong cùng phiên TEST, gửi ACK trên monitor trong 15 giây đó hoặc nhấn BTN2 thật.

### 4.5. Kiểm tra phát hiện module lỗi

Ngắt nguồn trước khi tháo/lắp dây. Khi khởi động thiếu SHT30, firmware báo NACK/`SENSOR ERROR`, bật báo lỗi tại chỗ và không gửi telemetry. RTC không có hoặc OSF bật → `RTC INVALID`, không gửi telemetry. OLED thiếu → Serial vẫn hoạt động; cảm biến/RTC/MQTT vẫn chạy. Khi module được kết nối đúng và cấp nguồn lại, chức năng phục hồi. Thử nghiệm mô phỏng rút module trong unit test kiểm tra nhánh xử lý lỗi; không thay thế kiểm tra an toàn đấu dây thật.

## 5. Test ESP32 → MQTT → Web

Chạy server tại thư mục gốc repo, giữ nguyên dữ liệu đang có:

```bash
# Chỉ tạo .env nếu chưa có; không ghi đè file cấu hình hiện tại
test -f .env || cp .env.example .env
docker compose up -d --build
docker compose ps
```

Mở [Dashboard](http://localhost:5173) và [API docs](http://localhost:8000/docs). Dừng simulator nếu đang chạy cùng ID `GSP-TRUCK-01`, để dữ liệu quan sát chỉ đến từ ESP32.

Lần đầu ESP32 phát Wi-Fi `GSP-ESP32-XXXX`. Kết nối máy tính/điện thoại vào mạng đó, mở `http://192.168.4.1`, chọn **Option 1**, nhập Wi-Fi **2,4 GHz**, lưu và khởi động lại. Máy tính chạy Docker và ESP32 cần ở cùng mạng; không dùng mạng khách chặn thiết bị liên lạc nhau. Firmware tìm backend qua UDP 8888 rồi kết nối MQTT TCP 1883; firewall cần cho phép hai cổng này từ mạng LAN.

OLED trang 2 cần báo `WiFi: ON`, `MQTT: ON`. Serial có `[MQTT] Đã kết nối Broker ...` và `Publish ... -> OK`. Web hiển thị dữ liệu mỗi khoảng 10 giây; nhấn BTN3 để quan sát 9,50°C và cảnh báo. Xem bản ghi thật trong API:

```bash
curl 'http://localhost:8000/api/v1/devices/GSP-TRUCK-01/telemetry?limit=5'
docker compose logs --tail=80 backend mosquitto
```

`sig_verified=true` ở Sprint 1 chỉ chứng minh hash phù hợp cơ chế **dummy signature** hiện tại, không chứng minh ECDSA thật. `Publish OK` cũng chưa chứng minh DB đã lưu; kiểm tra API như trên.

Option 2 dùng IP gateway làm broker khi máy tính phát hotspot. Option 3 kết nối trực tiếp AP của ESP32 và dùng broker cố định `192.168.4.2`; máy tính phải thực sự có địa chỉ này. Cách dễ thử đầu tiên là Option 1. Giữ **BOOT GPIO0** 5 giây trong khi firmware đang chạy để xóa Wi-Fi đã lưu và trở về AP. BOOT khác BTN1/2/3.

## 6. Đo pin khi đã lắp mạch pin

Mặc định `GSP_BATTERY_CONNECTED=0`: OLED ghi `N/A`, JSON dùng `bat_v=0.00` nghĩa là không đo pin trong cấu hình test USB. Không có điện áp pin giả 4,08 V.

Muốn đo pin: nối pin+ → 100 kΩ → **GPIO34** → 100 kΩ → GND; thêm tụ 100 nF giữa GPIO34 và GND, GND chung. Không đưa pin 4,2 V trực tiếp vào GPIO. Đổi cờ trong `firmware/platformio.ini` thành `-DGSP_BATTERY_CONNECTED=1`, build và nạp lại. Firmware lấy `analogReadMilliVolts(34) × 2 / 1000`, chỉ chấp nhận 2,50–4,35 V. Đối chiếu đồng hồ trước khi dùng số đo; thử vận hành nguồn/pin nhiều giờ thuộc các sprint sau.

## 7. Các lỗi thường gặp và tự kiểm tra lại

| Hiện tượng | Kiểm tra |
|---|---|
| Không có cổng USB mới | Cáp dữ liệu, cổng USB, chip USB-UART CP210x/CH340 và driver của máy; chạy lại `--list` |
| Permission denied khi mở cổng Linux | Xem `ls -l /dev/ttyUSB0` và `id`; cấp quyền theo nhóm sở hữu thiết bị của bản Linux đang dùng, đăng nhập lại khi vừa thêm nhóm |
| Không nạp được vì cổng đang bận | Đóng monitor/script Serial khác và chọn đúng cổng |
| Địa chỉ I2C thiếu | Nguồn 3V3, GND chung, đúng SDA21/SCL22, không đấu theo sơ đồ S3 cũ |
| `RTC=INVALID` | Thực hiện `--set-time`, kiểm tra `0x68` và pin backup; không sửa firmware để dùng `millis()` thay thế |
| `SQW WARN` | Dây SQW27, đúng chân SQW thay vì 32K; thử SCAN, kiểm tra pull-up |
| OLED trắng nhưng có `0x3C` | Kiểm tra đúng controller SSD1306 và độ phân giải 128×64; kiểm tra nguồn/dây |
| Còi kêu ngay khi chạy trong phòng | Nhiệt độ >8°C hoặc SHT30 lỗi; đọc STATUS để phân biệt, BTN2 mute |
| Có Wi-Fi nhưng MQTT OFF | Server/Docker đã chạy, UDP8888 và TCP1883 thông, cùng LAN; kiểm tra log discovery |
| Serial ký tự rác | Đặt 115200 baud |

Chạy kiểm thử phần mềm trên Linux có `g++`:

```bash
.venv/bin/python tools/test_sprint1.py
.venv/bin/pio run -d firmware -e esp32dev
```

Các test host dùng I2C/GPIO/OLED mô phỏng để chạy chính driver và logic; không phải kết quả đo trên ESP32. Xem [báo cáo kiểm thử](sprint1_test_report.md) để biết phần đã thực thi và phần cần nghiệm thu thiết bị thật.

Chạy đủ build/host/server và kiểm tra trình duyệt tự động:

```bash
.venv/bin/python -m pip install -r tools/requirements-sprint1-browser.txt
.venv/bin/python -m playwright install chromium
docker compose up -d --build
.venv/bin/python tools/check_sprint1.py --browser
```

Nếu chỉ muốn kiểm tra phần mềm không có Chromium, bỏ `--browser`; JSON sẽ ghi rõ `browser_requested=false`.
Khi có lỗi, runner trả mã khác 0 và lưu log của từng bước cùng `software_results.json`.
System test cấp phát một thiết bị giả lập `GSP-S1-*`, kiểm tra cả LWT thật bằng việc dừng tiến trình MQTT mà không gửi DISCONNECT.
Không dừng hoặc xóa database hiện có. Log/JSON/ảnh nằm trong `evidence/sprint1-YYYY-MM-DD/`.
Ảnh Dashboard là dữ liệu mô phỏng; mở `http://localhost:5173/` để theo dõi ESP32 thật.

Build Web trong container xuất ra volume `/app/dist` riêng để không tạo file `root` trong `web/dist` của máy host.
