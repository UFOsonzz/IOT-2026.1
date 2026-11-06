# Kết quả kiểm thử thực tế ngày 06/11/2026

Mã nguồn gốc: `e3bbcaf82a3885f16b654bf40c64cf35f25b2487`. Đã chạy Mosquitto, TimescaleDB, backend và web bằng Docker; kiểm tra web bằng Chromium thật. Không sửa logic ứng dụng trong lượt kiểm thử này.

**Kết quả: 22 ca, 15 đạt, 7 không đạt; không có ERROR hoặc ca bị bỏ qua.** Thời gian chạy suite: 105,759 giây, kết thúc lúc 12:26:19 (UTC+7). Một ca hiệu năng chạy lại riêng sau khi hệ thống đã khởi động xong đạt ngưỡng; kết quả gốc vẫn được giữ nguyên.

Các ca không đạt bao gồm chức năng chưa hoàn thiện ở bản walking skeleton, giới hạn phạm vi bảo vệ dữ liệu và lỗi hành vi UI/khôi phục. Không nên hiểu cả bảy ca là lỗi hồi quy mới.

## Môi trường và bằng chứng

- Python host 3.11.17; Node trong container 20.20.2; Docker Compose 5.6.0.
- PostgreSQL 16.15; TimescaleDB 2.31.0; broker Mosquitto 2.1.2.
- [Thông tin môi trường và thống kê fixture](evidence/2026-11-06/environment.json).
- [Kết quả máy đọc được](evidence/2026-11-06/results.json), [toàn bộ đầu ra suite](evidence/2026-11-06/system-tests.log), [log backend](evidence/2026-11-06/backend.log), [trạng thái bốn dịch vụ](evidence/2026-11-06/services.txt).

Lệnh chạy:

```bash
python tools/test_system.py --browser --with-restarts --output docs/02_testing_and_handover/evidence/2026-11-06/results.json
```

Bộ test tạo thiết bị `TEST-059d4ad5a7d8` riêng cho integration; các ca Chromium ghi dữ liệu demo vào `GSP-TRUCK-01`. Dữ liệu được giữ lại vì telemetry là bảng bất biến. Các ca gián đoạn tạm dừng/bật lại dịch vụ trên môi trường demo.

## Kết quả từng ca

| Ca | Nội dung | Kết quả |
|---|---|---|
| 01 | REST danh sách thiết bị | Đạt |
| 02 | MQTT → DB → WebSocket, truy vấn lại và ngưỡng <2 giây | Không đạt lần đầu: 3,319 giây; đạt khi chạy lại riêng |
| 03 | Alert được lưu DB và phát WebSocket | Đạt |
| 04 | Sync, gửi lại cùng bản ghi, DB chỉ giữ một | Đạt |
| 05 | Sửa nhiệt độ làm sai hash, đánh dấu TAMPERED và audit COMPROMISED | Đạt |
| 06 | ONLINE/OFFLINE qua MQTT, WS và API nhất quán | Đạt |
| 07 | Broker phát LWT khi đóng socket không gửi MQTT DISCONNECT | Đạt |
| 08 | JSON lỗi/null/list/thiếu trường không làm worker ngừng xử lý | Đạt |
| 09 | Phân trang, limit không hợp lệ và cursor lỗi | Đạt |
| 10 | UPDATE/DELETE bị trigger từ chối với SQLSTATE 42501 | Đạt |
| 11 | UDP WHO_IS_IOT_SERVER → IOT_SERVER_HERE | Đạt |
| 12 | 100 bản mới + 100 bản trùng, chỉ lưu đủ 100 bản mới | Đạt |
| 13 | Chữ ký tùy ý dài 64 ký tự phải bị từ chối | Không đạt |
| 14 | Thay đổi điện áp pin phải bị phát hiện nếu bảo vệ toàn bộ telemetry | Không đạt; trường pin nằm ngoài canonical string hiện tại |
| 15 | device_id trong payload phải khớp device_id trong topic | Không đạt |
| 16 | Backend dừng, gửi 10 bản QoS1, bật lại và nhận đủ | Đạt |
| 17 | DB dừng, gửi bản tin, DB trở lại không được mất bản | Không đạt |
| 18 | Chromium: tải dashboard, realtime và ONLINE | Đạt |
| 19 | Chromium: vượt ngưỡng hiển thị cảnh báo | Đạt |
| 20 | Chromium: TAMPERED và nút kiểm toán trả COMPROMISED | Đạt |
| 21 | Chromium: sync dữ liệu cũ không thay chỉ số mới nhất | Không đạt |
| 22 | Chromium: bản trùng bị DB từ chối không thay giá trị UI | Không đạt |

## Những vấn đề đã tái hiện

### 1. Mất bản tin khi DB gián đoạn

Trong ca 17, publisher nhận PUBACK nhưng worker không ghi được bản tin `seq_no=123` do DB đang dừng. Sau khi DB trở lại, bản `124` ghi được, bản `123` không được phát lại. Log backend ghi lỗi kết nối DB; fixture cuối có 122 telemetry rows. Hàng đợi persistent MQTT xử lý được tình huống **backend dừng** ở ca 16, nhưng chưa giải quyết được tình huống **worker còn chạy trong lúc DB lỗi**.

Điểm cần xem: `backend/app/mqtt_worker.py`, `_on_message()` bắt ngoại lệ rồi trả về; chưa có cơ chế retry việc lưu DB. Vì vậy QoS1 tới broker không đồng nghĩa dữ liệu đã lưu thành công.

### 2. UI hiển thị khác DB khi nhận bản trùng

Ca 22 gửi bản đầu 4,25°C, sau đó gửi cùng timestamp với giá trị 7,75°C. DB giữ 4,25°C đúng theo `ON CONFLICT DO NOTHING`, còn UI đổi thành 7,75°C. Sự kiện WS vẫn chứa dữ liệu bản trùng và React xử lý cả khi `inserted=false`.

Bằng chứng: [ảnh UI bản trùng](evidence/2026-11-06/ui-duplicate.png), assertion đối chiếu DB trong JSON/log suite. Tải lại trang sẽ lấy lại giá trị trong DB; việc tải lại không khắc phục logic realtime.

### 3. Dữ liệu sync cũ trở thành chỉ số hiện tại

Ca 21 gửi bản mới 4,25°C, sau đó sync bản 3,25°C có timestamp cũ hơn 10 phút. Dashboard chuyển KPI sang 3,25°C và đặt bản cũ lên đầu. `web/src/App.tsx` đang ưu tiên thứ tự nhận WS, chưa sắp theo timestamp đo khi cập nhật danh sách.

Bằng chứng: [ảnh UI sync cũ](evidence/2026-11-06/ui-old-sync.png).

### 4. Chưa xác thực ECDSA thật

Ca 13 tạo SHA-256 đúng nhưng đặt `ecdsa_sig` thành 64 ký tự `X`; backend vẫn trả `sig_verified=true`. Hàm `verify_dummy_signature()` chỉ kiểm tra hash và tiền tố/độ dài chữ ký. Đây là logic dummy đã được chú thích trong mã, chưa kiểm tra bằng khóa công khai. `verify-chain` hiện đếm cờ đã lưu, không tự thực hiện xác minh ECDSA lại.

Do đó các nhãn **ECDSA VERIFIED** trong ảnh chỉ mô tả cờ của bản giả lập, chưa chứng minh xác thực mật mã đầy đủ.

### 5. Phạm vi băm và danh tính thiết bị

Ca 14 sửa `bat_v` mà không đổi hash và vẫn được verified. Canonical string hiện chỉ gồm device, sequence, timestamp, nhiệt độ, độ ẩm và alert code; đây là giới hạn của data contract hiện tại nếu muốn bảo vệ toàn bộ bản tin.

Ca 15 dùng topic `OTHER-DEVICE` nhưng payload mang ID của fixture; backend vẫn ghi vào fixture. `_handle_telemetry_or_sync()` hiện ưu tiên `device_id` do payload cung cấp và chưa bắt buộc khớp topic.

### 6. Hiệu năng lần đầu chưa đạt ngưỡng

Ca 02 lần đầu mất 3,319 giây cho đường kiểm tra MQTT → DB → WS và truy vấn lại API. Máy đang có tải I/O lớn, vừa hoàn tất khởi tạo/phục hồi DB. Chạy lại riêng cùng ca sau khi hệ thống đã khởi động xong thì đạt `<2s`: [log chạy lại](evidence/2026-11-06/telemetry-warm-retest.log).

Không dùng một lượt này để kết luận luôn đạt độ trễ <2 giây, và đây chưa phải phép đo từ cảm biến thật đến màn hình.

## Build và các kiểm tra bổ sung

**Frontend production build: không đạt**, tái hiện cả trên host và trong container Node 20 bằng `npm run build`:

```text
src/App.tsx(27,15): error TS2339: Property 'env' does not exist on type 'ImportMeta'.
src/App.tsx(29,15): error TS2339: Property 'env' does not exist on type 'ImportMeta'.
```

Chưa khai báo kiểu Vite client cho TypeScript. Chế độ Vite dev của Docker vẫn chạy và đã được kiểm tra bằng Chromium. Bằng chứng: [build container](evidence/2026-11-06/web-build-container.log), [build host](evidence/2026-11-06/web-build-host.log).

**Canonical string/SHA-256 Python: đạt** với vector `GSP-TRUCK-01|1042|1761526800|4.25|62.50|0`; simulator và backend cùng cho `2d2744bd1cdef2facfb2c88ac7fbf361e9ab74a9cb4c5cc0dffdeed4aa7b207b`. [Log](evidence/2026-11-06/canonical-vector.log). Chưa dùng vector này để khẳng định kết quả C++ trên mạch.

**GUI Simulator: đạt kiểm tra khởi động/kết nối MQTT**, cửa sổ 1220×820, không lỗi callback trong lượt mở/đóng. [Log](evidence/2026-11-06/gui-startup.log). Chưa tự động kiểm tra toàn bộ nút bấm, thao tác thẻ SD và Tampering Lab của GUI.

**CLI Simulator:** đã chạy menu 1 → 2 → 3 → 4 → 5 → 6 → 0, cách nhau 3 giây và thoát sạch. [Log](evidence/2026-11-06/cli-simulator.log). Các assertion dữ liệu nằm trong suite Python, không chỉ dựa vào dòng in “hoàn tất” của simulator.

**Firmware build: đạt**, `pio run -d firmware` tạo thành công `firmware/.pio/build/esp32-s3-devkitc-1/firmware.bin`. PlatformIO 6.2.0, espressif32 8.0.0, Arduino ESP32 3.3.12. RAM tĩnh 48.268/327.680 bytes (14,7%); firmware 1.094.751/3.342.336 bytes (32,8% vùng app). Có cảnh báo `ARDUINO_USB_MODE` bị định nghĩa lại. [Log build đầy đủ](evidence/2026-11-06/firmware-build.log). Chưa flash/kiểm tra trên ESP32-S3 thật vì không có thiết bị serial được kết nối; build không xác nhận cấu hình bộ nhớ hay linh kiện thật trên mạch.

Đọc mã firmware cũng cho thấy nhiệt độ/độ ẩm đang được tạo giả lập, chữ ký dùng chuỗi `DUMMY_SIG_OK...`; nhánh mất mạng chỉ in thông báo chờ đồng bộ SD, chưa ghi/lấy dữ liệu từ thẻ SD thật. Kết quả simulator không thay thế nghiệm thu các chức năng phần cứng này.

## Giới hạn và cách chạy lại

Lần khởi động đầu rất chậm do tải ổ đĩa; PostgreSQL gặp timeout tắt server tạm khi init và sau đó tự phục hồi. Cả bốn dịch vụ đã chạy được trước khi hoàn tất kiểm tra UI. Không xóa/reset volume để che tình trạng này.

Chưa kiểm tra cảm biến, RTC, SD, còi, thời lượng pin thật, độ trễ phần cứng, cắt nguồn máy chủ hoặc vận hành 7 ngày. Không chạy bộ Postman bằng Postman/Newman trong lượt này; các endpoint và các tình huống sâu hơn được kiểm tra bằng script Python.

Hướng dẫn từng bước, các lệnh và cách quan sát kết quả: [Tự kiểm thử repo](how_to_test.md).

Các tệp bổ sung phục vụ kiểm tra: `tools/test_system.py`, `tools/requirements-test.txt`, hướng dẫn, báo cáo và evidence. README được thêm liên kết tới tài liệu mới; `web/.dockerignore` loại `node_modules`/`dist` khỏi build context để quá trình test/build không đóng gói lại dependencies của host. Không sửa các lỗi ứng dụng đã nêu trong báo cáo.
