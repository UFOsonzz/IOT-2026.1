# Tự kiểm thử repo

Chạy các lệnh bên dưới tại thư mục gốc `IOT-2026.1`. Đây là kiểm thử trên dịch vụ thật, cần Docker đang chạy. Bộ test cố ý kiểm tra cả các yêu cầu mà bản hiện tại còn vi phạm, nên `FAILED` cần được đọc cùng báo cáo, không có nghĩa mọi thành phần đều hỏng.

## 1. Khởi động hệ thống

Nếu chưa có `.env`, sao chép file mẫu (giữ file hiện tại nếu đã cấu hình):

```bash
cp -n .env.example .env
docker compose up -d --build
docker compose ps
```

Đợi đủ bốn dịch vụ hoạt động và TimescaleDB báo `healthy`. Mở:

- Web: http://localhost:5173
- Swagger: http://localhost:8000/docs
- API: http://localhost:8000/api/v1/devices

Lần đầu chưa phát dữ liệu thì thiết bị mẫu OFFLINE và bảng trống là bình thường.

Lượt chạy trên máy này gặp khởi tạo/fsync DB rất chậm khi ổ đĩa đang chịu tải lớn. Nếu DB chưa sẵn sàng, đọc `docker compose logs --tail=50 timescaledb`; sau khi DB sẵn sàng, chạy lại `docker compose up -d` nếu backend/web chưa được khởi động vì điều kiện healthcheck.

## 2. Chuẩn bị Python để test

Trên máy hiện tại, `.venv` đã được tạo và cài dependencies trong lượt kiểm tra; chỉ cần `source .venv/bin/activate`. Các lệnh tạo môi trường bên dưới dành cho máy mới/chưa có `.venv`.

Cách đã dùng trên máy này (`uv`):

```bash
uv venv --python 3.11 .venv
uv pip install --python .venv/bin/python -r tools/requirements-test.txt
source .venv/bin/activate
python -m playwright install chromium
```

Nếu không dùng `uv`, dùng Python 3.11 và `venv`:

```bash
python3.11 -m venv .venv
source .venv/bin/activate
python -m pip install -r tools/requirements-test.txt
python -m playwright install chromium
```

Windows: thay lệnh `source` bằng `.venv\Scripts\activate` trong CMD hoặc `.venv\Scripts\Activate.ps1` trong PowerShell. Các lệnh `python ...` sau khi activate giống nhau.

## 3. Chạy lại bộ test tự động

Test API + MQTT + DB + WebSocket + UDP, không dừng dịch vụ:

```bash
python tools/test_system.py
```

Chạy đầy đủ như lượt kiểm tra, gồm trình duyệt Chromium và tình huống dừng backend/DB:

```bash
python tools/test_system.py --browser --with-restarts
```

Tùy chọn `--with-restarts` tạm dừng rồi bật lại `backend` và `timescaledb`, nên chạy trên môi trường demo. `--browser` ghi một số bản tin demo vào thiết bị `GSP-TRUCK-01` để kiểm tra dashboard. Phần integration còn lại tạo thiết bị `TEST-...` riêng cho mỗi lượt. Bản ghi được giữ lại để đối chiếu vì bảng telemetry chặn UPDATE/DELETE.

Kết quả mặc định ở `playground/test-results.json`; ảnh chụp trình duyệt nằm cùng thư mục. Đổi vị trí bằng:

```bash
python tools/test_system.py --browser --with-restarts --output playground/my-run/results.json
```

`ok`: đạt; `FAIL`: hành vi thực tế không đạt tiêu chí trong test; `ERROR`: lỗi khi thực thi test; `skipped`: chưa bật tùy chọn cần thiết. Exit code 1 khi có FAIL/ERROR. Xem `test_results_2026-11-06.md` và thư mục `evidence/2026-11-06` cho lần chạy đã thực hiện.

Bản hiện tại đã tái hiện sáu ca FAIL về hành vi/phạm vi bảo vệ: 13, 14, 15, 17, 21, 22. Ca 02 vượt ngưỡng 2 giây ở lượt đầu nhưng đạt khi chạy lại riêng sau khởi động, nên số ca hiệu năng không đạt có thể khác giữa các lượt. Không đổi tiêu chí/ẩn FAIL để làm kết quả xanh.

Bộ test kiểm tra: REST, ghi telemetry và phát WebSocket, alert trong DB, sync/chống trùng, phát hiện sửa nhiệt độ, ONLINE/OFFLINE, LWT do broker phát sau ngắt socket thật, JSON lỗi, phân trang/validation, trigger SQLSTATE 42501, UDP discovery, 100 bản tin mới + 100 bản trùng, chữ ký giả, sửa pin, sai danh tính topic, khôi phục sau gián đoạn, và giao diện Chromium.

Nếu đổi `.env`, script không tự nạp `.env`; cấu hình riêng các biến `TEST_DATABASE_URL` (kết nối qua localhost), `TEST_MQTT_HOST`, `TEST_MQTT_PORT`, `TEST_API_URL`, `TEST_WS_URL`, `TEST_WEB_URL` tương ứng. Chỉ dùng test trên DB của dự án.

## 4. Tự test bằng simulator và quan sát trên web

Giữ dashboard mở rồi chạy trong terminal đã activate `.venv`:

```bash
python tools/dummy_simulator.py --mode interactive
```

Chọn từng mục, cách nhau ít nhất **2 giây**: khóa DB hiện là `(device_id, ts_rtc)` theo giây; thao tác quá nhanh có thể đụng timestamp.

| Chọn | Quan sát trên web / API |
|---|---|
| `1` | Có bản ghi mới, nhiệt độ khoảng 3.8–5.4°C, thiết bị ONLINE |
| `2` | Nhiệt độ >9.2°C, cảnh báo đỏ; backend nhận cả telemetry và alert |
| `3` | Có ba bản ghi AUTO-SYNC; gói thứ tư trùng nên DB chỉ thêm ba |
| `4` | Bản ghi có nhãn TAMPERED; bấm **Kiểm toán chuỗi chữ ký GSP** thấy COMPROMISED |
| `5` | Thiết bị OFFLINE; mục này chủ động gửi trạng thái, chưa phải mất mạng thật |
| `6` | Thiết bị ONLINE trở lại |
| `0` | Thoát, gửi OFFLINE và đóng kết nối |

Lịch sử của lần kiểm tra trước được giữ nguyên: sau khi đã có một bản TAMPERED, kiểm toán toàn bộ lịch sử vẫn trả COMPROMISED dù bản tin mới là bình thường. Nút **Đã xem** dùng để xác nhận banner cảnh báo đã nhận.

Bộ test tự động có kiểm tra LWT thật bằng cách đóng socket mà không gửi MQTT DISCONNECT. Không dùng riêng lựa chọn `5` để kết luận LWT thật đã chạy.

Có thể chạy vòng demo:

```bash
python tools/dummy_simulator.py --mode auto --interval 3
```

Nhấn Ctrl+C để dừng. Simulator có SHA-256 thật nhưng chữ ký ECDSA giả lập; nhãn ECDSA Verified hiện chưa chứng minh xác thực bằng khóa công khai.

Muốn dùng GUI, chạy `./run_simulator_mac.sh` trên Linux/macOS hoặc `run_simulator_win.bat` trên Windows. Có thể dùng môi trường đã chuẩn bị: `uv pip install --python .venv/bin/python customtkinter==5.2.2`, sau đó `python tools/gui_simulator.py`. Lượt kiểm tra đã mở/đóng GUI và xác nhận kết nối MQTT; chưa kiểm thử tự động toàn bộ nút bấm của GUI.

## 5. Kiểm tra REST bằng Swagger hoặc Postman

Trong Swagger, thử `GET /api/v1/devices`, `GET /api/v1/devices/GSP-TRUCK-01/telemetry?limit=2`, rồi dùng `next_cursor` làm `cursor` để lấy trang tiếp theo; thử `limit=0` phải trả 422 và `cursor=invalid` phải trả 400. `verify-chain` phải tăng `tampered_records` sau kịch bản sửa nhiệt độ.

Hoặc import `docs/01_technical_dossier/06_postman_collection.json` vào Postman rồi Run collection. Bộ Postman có 6 assertions cho 3 endpoint, chủ yếu kiểm tra HTTP/schema; bộ Python kiểm tra sâu hơn luồng dữ liệu và các trường hợp lỗi.

## 6. Kiểm tra biên dịch độc lập

Frontend, dùng môi trường Node trong container giống repo:

```bash
docker compose exec -T web npm run build
```

Firmware ESP32-S3:

```bash
uv pip install --python .venv/bin/python platformio==6.2.0
python -m platformio run -d firmware
```

Hoặc nếu đã cài PlatformIO CLI: `pio run -d firmware`. Build thành công chỉ chứng minh biên dịch; cần mạch thật để kiểm tra cảm biến, lưu SD, còi, Wi-Fi, RTC và thời gian phản hồi.

## 7. Đọc log và dừng hệ thống

```bash
docker compose logs --tail=100 backend
docker compose logs --tail=100 timescaledb
docker compose ps
docker compose down
```

`docker compose down` giữ dữ liệu để lần sau kiểm tra tiếp. Không thêm `-v` nếu còn cần dữ liệu hoặc bằng chứng của lần test.
