#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>

// Tự động nạp secrets.h nếu tồn tại, nếu chưa tạo sẽ dùng secrets.example.h
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// ============================================================================
// 1. CẤU HÌNH CHẾ ĐỘ CHẠY THÔNG (WALKING SKELETON DUMMY MODE)
// ============================================================================
#define DUMMY_MODE          1
#define SAMPLE_INTERVAL_MS  10000UL   // Chu kỳ 10 giây/lần khi test chạy thông
#define MQTT_KEEPALIVE_SEC  30        // Keep-Alive 30s phục vụ phát hiện LWT nhanh

// Ngưỡng nhiệt độ chuẩn bảo quản Vaccine GSP (+2.0°C đến +8.0°C)
#define GSP_TEMP_MIN_C      2.00f
#define GSP_TEMP_MAX_C      8.00f

// ============================================================================
// 2. BẢNG GÁN CHÂN PINOUT CHÍNH THỨC (ESP32-S3-WROOM-1-N16R8)
// CẤM TUYỆT ĐỐI chạm vào GPIO35, GPIO36, GPIO37 (Octal PSRAM 8MB),
// tránh Strapping Pins (0, 3, 45, 46) và tránh ADC2.
// ============================================================================

// Bus I2C0 dùng chung cho SHT30 (0x44), RTC DS3231 (0x68), OLED SSD1306 (0x3C)
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

// Ngắt đồng bộ giây từ IC thời gian thực DS3231 (SQW/INT)
#define PIN_RTC_SQW         14

// Bus SPI2 (FSPI) giao tiếp thẻ nhớ MicroSD lưu Offline
#define PIN_SD_CS           10
#define PIN_SD_MOSI          11
#define PIN_SD_SCLK          12
#define PIN_SD_MISO          13

// Đo điện áp Pin dự phòng 18650 qua cầu phân áp 2x100kOhm (ADC1_CH3)
#define PIN_BAT_ADC         4

// Điều khiển Module Relay 1 kênh bật Còi hú + Đèn chớp tại chỗ
#define PIN_RELAY           21

// 3 Nút nhấn giao diện người-máy HMI (INPUT_PULLUP - Tích cực mức THẤP)
#define PIN_BTN_WAKE        5         // BTN1: Bật sáng màn hình OLED / Chuyển trang
#define PIN_BTN_ACK         6         // BTN2: Xác nhận / Tắt tiếng còi tại chỗ
#define PIN_BTN_TEST        7         // BTN3: Giả lập vượt ngưỡng nhiệt độ / Đồng bộ

// LED trạng thái trên bo mạch ESP32-S3
#define PIN_STATUS_LED      38

// ============================================================================
// 3. CẤU TRÚC DỮ LIỆU & HÀNG ĐỢI FREERTOS GIỮA 2 LÕI (DUAL-CORE)
// ============================================================================
struct TelemetryPacket {
    char device_id[32];
    uint32_t seq_no;
    uint32_t ts_rtc;
    float temp_c;
    float hum_pct;
    float bat_v;
    uint8_t alert_code;
    char sha256_hash[65];
    char ecdsa_sig[64];
    bool is_backfill;
};

static QueueHandle_t xTelemetryQueue = NULL;
static volatile bool g_alarmMutedByUser = false;
static uint32_t g_sequenceNumber = 1000;

WiFiClient espWifiClient;
PubSubClient mqttClient(espWifiClient);

// Chủ đề (Topics) MQTT theo đúng Data Contract
static char TOPIC_TELEMETRY[64];
static char TOPIC_SYNC[64];
static char TOPIC_ALERT[64];
static char TOPIC_STATUS[64];

// ============================================================================
// 4. HÀM BĂM SHA-256 PHẦN CỨNG (MBEDTLS) & TIỆN ÍCH THỜI GIAN
// ============================================================================
void computeHardwareSha256Hex(const char* inputStr, char outputHex[65]) {
    uint8_t hashBytes[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (không phải SHA-224)
    mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char*>(inputStr), strlen(inputStr));
    mbedtls_sha256_finish(&ctx, hashBytes);
    mbedtls_sha256_free(&ctx);

    for (int i = 0; i < 32; i++) {
        snprintf(&outputHex[i * 2], 3, "%02x", hashBytes[i]);
    }
    outputHex[64] = '\0';
}

uint32_t getCurrentUtcEpoch() {
    time_t now;
    time(&now);
    // Nếu NTP chưa đồng bộ kịp trong vài giây đầu, dùng mốc epoch giả lập hợp lệ
    if (now < 1700000000L) {
        return 1761526800UL + (millis() / 1000UL);
    }
    return static_cast<uint32_t>(now);
}

float readBatteryVoltage() {
    uint32_t mv = analogReadMilliVolts(PIN_BAT_ADC);
    float measuredV = (mv / 1000.0f) * 2.0f; // Cầu phân áp 2x100kOhm (hệ số nhân 2)
    if (measuredV < 2.50f || measuredV > 4.35f) {
        // Giá trị giả lập ổn định khi chưa cắm cell pin 18650 vào chân ADC
        return 4.08f - ((g_sequenceNumber % 10) * 0.01f);
    }
    return measuredV;
}

// ============================================================================
// 5. CORE 0: ĐỌC CẢM BIẾN, KIỂM TRA NGƯỠNG GSP, BẬT RELAY & KÝ SỐ BẢN TIN
// ============================================================================
void TaskCore0_SensingAndSign(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLE_INTERVAL_MS);

    Serial.printf("[Core %d] Khởi chạy TaskCore0_SensingAndSign (Chu kỳ: %lu ms)\n",
                  xPortGetCoreID(), SAMPLE_INTERVAL_MS);

    for (;;) {
        // Kiểm tra nút nhấn ACK/MUTE (GPIO6) để tắt tiếng còi tại chỗ
        if (digitalRead(PIN_BTN_ACK) == LOW) {
            g_alarmMutedByUser = true;
            digitalWrite(PIN_RELAY, LOW);
            Serial.println("[HMI] Người vận hành đã nhấn nút ACK (GPIO6) -> Tắt Relay còi báo.");
        }

        g_sequenceNumber++;
        uint32_t tsUtc = getCurrentUtcEpoch();

        // Mặc định nhiệt độ dao động ổn định quanh 4.50°C (đạt chuẩn +2.0°C .. +8.0°C)
        float pseudoNoise = (static_cast<int>(g_sequenceNumber % 7) - 3) * 0.08f;
        float tempC = 4.50f + pseudoNoise;
        float humPct = 62.50f + ((static_cast<int>(g_sequenceNumber % 5) - 2) * 0.35f);

        // Nếu nhấn giữ nút TEST (GPIO7 - tích cực THẤP), giả lập nhiệt độ vọt lên 9.50°C
        if (digitalRead(PIN_BTN_TEST) == LOW) {
            tempC = 9.50f;
            g_alarmMutedByUser = false;
            Serial.println("[TEST] Phát hiện nhấn nút GPIO7 -> Giả lập vượt ngưỡng nhiệt độ (9.50°C)!");
        }

        // Làm tròn đúng 2 chữ số thập phân để khớp tuyệt đối với chuỗi Canonical
        tempC = roundf(tempC * 100.0f) / 100.0f;
        humPct = roundf(humPct * 100.0f) / 100.0f;
        float batV = roundf(readBatteryVoltage() * 100.0f) / 100.0f;

        // Đánh giá ngưỡng GSP và điều khiển Relay còi + đèn tại chỗ (FR-04 <= 3s)
        uint8_t alertCode = 0;
        if (tempC > GSP_TEMP_MAX_C) {
            alertCode = 1; // Vượt ngưỡng nhiệt độ cao
        } else if (tempC < GSP_TEMP_MIN_C) {
            alertCode = 2; // Vượt ngưỡng nhiệt độ thấp
        }

        if (alertCode > 0) {
            if (!g_alarmMutedByUser) {
                digitalWrite(PIN_RELAY, HIGH); // Bật còi hú + đèn chớp ngay lập tức
            }
            digitalWrite(PIN_STATUS_LED, HIGH);
        } else {
            g_alarmMutedByUser = false;
            digitalWrite(PIN_RELAY, LOW);
            digitalWrite(PIN_STATUS_LED, LOW);
        }

        // Tạo chuỗi chuẩn hóa ký số (Canonical String):
        // "<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>"
        char canonicalStr[160];
        snprintf(
            canonicalStr,
            sizeof(canonicalStr),
            "%s|%lu|%lu|%.2f|%.2f|%u",
            DEVICE_ID,
            static_cast<unsigned long>(g_sequenceNumber),
            static_cast<unsigned long>(tsUtc),
            tempC,
            humPct,
            alertCode
        );

        // Đóng gói cấu trúc bản tin đẩy sang Core 1
        TelemetryPacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        strncpy(pkt.device_id, DEVICE_ID, sizeof(pkt.device_id) - 1);
        pkt.seq_no = g_sequenceNumber;
        pkt.ts_rtc = tsUtc;
        pkt.temp_c = tempC;
        pkt.hum_pct = humPct;
        pkt.bat_v = batV;
        pkt.alert_code = alertCode;
        pkt.is_backfill = false;

        // Tính mã băm SHA-256 thật bằng phần cứng mbedTLS trên ESP32-S3
        computeHardwareSha256Hex(canonicalStr, pkt.sha256_hash);

        // Gán chữ ký số Dummy theo đúng Data Contract của bản chạy thông
        strncpy(
            pkt.ecdsa_sig,
            "DUMMY_SIG_OK_ESP32S3_SECP256R1",
            sizeof(pkt.ecdsa_sig) - 1
        );

        Serial.printf("[Core 0] Canonical: %s\n", canonicalStr);
        Serial.printf("[Core 0] SHA-256  : %s\n", pkt.sha256_hash);

        if (xQueueSend(xTelemetryQueue, &pkt, pdMS_TO_TICKS(200)) != pdPASS) {
            Serial.println("[Core 0] CẢNH BÁO: Hàng đợi xTelemetryQueue đã đầy!");
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ============================================================================
// 6. CORE 1: QUẢN LÝ KẾT NỐI WI-FI, NTP, MQTT LWT & ĐẨY BẢN TIN LÊN BROKER
// Nguyên tắc Kiến trúc Pháp lý: Thiết bị biên CHỈ GỬI (Unidirectional),
// tuyệt đối KHÔNG đăng ký callback hay subscribe nhận lệnh điều khiển từ xa.
// ============================================================================
void ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    Serial.printf("[Core 1] Đang kết nối Wi-Fi tới SSID: %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Bật chế độ tiết kiệm năng lượng Modem-sleep theo yêu cầu NFR-04
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Core 1] Đã kết nối Wi-Fi! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    } else {
        Serial.println("[Core 1] Chưa kết nối được Wi-Fi. Sẽ thử lại ở chu kỳ sau.");
    }
}

void ensureMqttConnected() {
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) {
        return;
    }

    String clientId = String("esp32s3-") + String(DEVICE_ID);
    const char* lwtPayload = "{\"state\":\"OFFLINE\"}";

    Serial.printf("[Core 1] Đang kết nối MQTT Broker %s:%d (ClientID: %s)...\n",
                  MQTT_HOST, MQTT_PORT, clientId.c_str());

    // Kết nối với cleanSession = true, khai báo Di chúc LWT (QoS = 1, Retain = true)
    bool connected = mqttClient.connect(
        clientId.c_str(),
        NULL,
        NULL,
        TOPIC_STATUS,
        1,
        true,
        lwtPayload
    );

    if (connected) {
        Serial.println("[Core 1] Kết nối MQTT thành công! Đang gửi bản tin ONLINE (Retained=true)...");
        const char* onlinePayload = "{\"state\":\"ONLINE\"}";
        mqttClient.publish(TOPIC_STATUS, onlinePayload, true);
    } else {
        Serial.printf("[Core 1] Kết nối MQTT thất bại, rc=%d\n", mqttClient.state());
    }
}

bool publishTelemetryPacket(const TelemetryPacket& pkt) {
    JsonDocument doc;
    doc["device_id"] = pkt.device_id;
    doc["seq_no"] = pkt.seq_no;
    doc["ts_rtc"] = pkt.ts_rtc;
    doc["temp_c"] = serialized(String(pkt.temp_c, 2));
    doc["hum_pct"] = serialized(String(pkt.hum_pct, 2));
    doc["bat_v"] = serialized(String(pkt.bat_v, 2));
    doc["alert_code"] = pkt.alert_code;
    doc["sha256_hash"] = pkt.sha256_hash;
    doc["ecdsa_sig"] = pkt.ecdsa_sig;

    char jsonBuffer[512];
    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

    const char* targetTopic = pkt.is_backfill ? TOPIC_SYNC : TOPIC_TELEMETRY;
    bool pubOk = mqttClient.publish(targetTopic, reinterpret_cast<const uint8_t*>(jsonBuffer), len, false);

    // Nếu có vi phạm ngưỡng GSP (alert_code > 0), bắn đồng thời lên kênh khẩn cấp alert
    if (pubOk && pkt.alert_code > 0) {
        JsonDocument alertDoc;
        alertDoc["device_id"] = pkt.device_id;
        alertDoc["seq_no"] = pkt.seq_no;
        alertDoc["ts_rtc"] = pkt.ts_rtc;
        alertDoc["alert_code"] = pkt.alert_code;
        alertDoc["severity"] = "CRITICAL";
        alertDoc["temp_c"] = serialized(String(pkt.temp_c, 2));
        alertDoc["hum_pct"] = serialized(String(pkt.hum_pct, 2));
        alertDoc["message"] = "Canh bao vuot nguong nhiet do GSP tai thiet bi bien!";

        char alertBuf[384];
        size_t alertLen = serializeJson(alertDoc, alertBuf, sizeof(alertBuf));
        mqttClient.publish(TOPIC_ALERT, reinterpret_cast<const uint8_t*>(alertBuf), alertLen, false);
    }

    return pubOk;
}

void TaskCore1_NetworkAndSync(void* pvParameters) {
    Serial.printf("[Core %d] Khởi chạy TaskCore1_NetworkAndSync\n", xPortGetCoreID());

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SEC);
    mqttClient.setBufferSize(1024);

    TelemetryPacket rxPkt;
    uint32_t lastReconnectAttemptMs = 0;

    for (;;) {
        uint32_t nowMs = millis();
        if ((WiFi.status() != WL_CONNECTED || !mqttClient.connected()) &&
            (nowMs - lastReconnectAttemptMs >= 5000UL)) {
            lastReconnectAttemptMs = nowMs;
            ensureWiFiConnected();
            ensureMqttConnected();
        }

        if (mqttClient.connected()) {
            mqttClient.loop();
        }

        // Chờ nhận bản tin đã ký số từ Core 0 gửi sang qua Queue
        if (xQueueReceive(xTelemetryQueue, &rxPkt, pdMS_TO_TICKS(100)) == pdPASS) {
            if (mqttClient.connected()) {
                bool ok = publishTelemetryPacket(rxPkt);
                Serial.printf("[Core 1] Publish MQTT seq_no=%lu (temp=%.2fC, alert=%u) -> %s\n",
                              static_cast<unsigned long>(rxPkt.seq_no),
                              rxPkt.temp_c,
                              rxPkt.alert_code,
                              ok ? "THÀNH CÔNG" : "THẤT BẠI");
            } else {
                Serial.printf("[Core 1] Mất kết nối MQTT -> Bản ghi seq_no=%lu sẽ lưu vào SD ở phiên bản đầy đủ.\n",
                              static_cast<unsigned long>(rxPkt.seq_no));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================================
// 7. KHỞI TẠO HỆ THỐNG (SETUP) & VÒNG LẶP CHÍNH (LOOP)
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n================================================================");
    Serial.println("  GSP COLD-CHAIN IOT EDGE NODE - ESP32-S3-WROOM-1-N16R8 (MVP)");
    Serial.println("  Chế độ: Unidirectional & Append-Only Telemetry Architecture");
    Serial.println("================================================================");

    // Khởi tạo các chân GPIO theo đúng bảng Pinout đã chốt
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    pinMode(PIN_BTN_WAKE, INPUT_PULLUP);
    pinMode(PIN_BTN_ACK, INPUT_PULLUP);
    pinMode(PIN_BTN_TEST, INPUT_PULLUP);
    pinMode(PIN_RTC_SQW, INPUT_PULLUP);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

    // Khởi tạo chuỗi MQTT Topics theo DEVICE_ID
    snprintf(TOPIC_TELEMETRY, sizeof(TOPIC_TELEMETRY), "gsp/v1/%s/telemetry", DEVICE_ID);
    snprintf(TOPIC_SYNC, sizeof(TOPIC_SYNC), "gsp/v1/%s/sync", DEVICE_ID);
    snprintf(TOPIC_ALERT, sizeof(TOPIC_ALERT), "gsp/v1/%s/alert", DEVICE_ID);
    snprintf(TOPIC_STATUS, sizeof(TOPIC_STATUS), "gsp/v1/%s/status", DEVICE_ID);

    // Khởi tạo hàng đợi FreeRTOS chứa tối đa 32 bản tin giữa Core 0 và Core 1
    xTelemetryQueue = xQueueCreate(32, sizeof(TelemetryPacket));
    if (xTelemetryQueue == NULL) {
        Serial.println("[LỖI NGHIÊM TRỌNG] Không thể cấp phát bộ nhớ cho xTelemetryQueue!");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Ghim tác vụ Đọc cảm biến & Ký số vào Core 0
    xTaskCreatePinnedToCore(
        TaskCore0_SensingAndSign,
        "TaskCore0_Sense",
        8192,
        NULL,
        2,
        NULL,
        0
    );

    // Ghim tác vụ Truyền thông Wi-Fi/MQTT & Đồng bộ vào Core 1
    xTaskCreatePinnedToCore(
        TaskCore1_NetworkAndSync,
        "TaskCore1_Net",
        8192,
        NULL,
        1,
        NULL,
        1
    );
}

void loop() {
    // Toàn bộ luồng xử lý đã được phân bổ về 2 tác vụ FreeRTOS riêng biệt trên Core 0 và Core 1.
    // Tuyệt đối không dùng delay() dài chiếm dụng CPU trong loop().
    vTaskDelete(NULL);
}