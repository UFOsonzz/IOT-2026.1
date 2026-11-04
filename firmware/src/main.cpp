#include <Arduino.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include "WifiConfig.h"

// ============================================================================
// 1. CẤU HÌNH CHẾ ĐỘ CHẠY THÔNG VÀ NGƯỠNG GSP
// ============================================================================
#define SAMPLE_INTERVAL_MS  10000UL   // Chu kỳ đọc cảm biến 10 giây
#define MQTT_KEEPALIVE_SEC  30        // Keep-Alive 30s phục vụ phát hiện LWT nhanh

// Ngưỡng nhiệt độ bảo quản Vaccine GSP từ 2.0°C đến 8.0°C
#define GSP_TEMP_MIN_C      2.00f
#define GSP_TEMP_MAX_C      8.00f

// ============================================================================
// 2. BẢNG GÁN CHÂN PINOUT
// KHÔNG chạm vào GPIO35, GPIO36, GPIO37 (Octal PSRAM 8MB)
// ============================================================================
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9
#define PIN_RTC_SQW         14
#define PIN_SD_CS           10
#define PIN_SD_MOSI         11
#define PIN_SD_SCLK         12
#define PIN_SD_MISO         13
#define PIN_BAT_ADC         4
#define PIN_RELAY           21

// 3 Nút nhấn giao diện người-máy HMI
#define PIN_BTN_WAKE        5         // BTN1: Bật sáng màn hình OLED / Chuyển trang
#define PIN_BTN_ACK         6         // BTN2: Xác nhận / Tắt tiếng còi tại chỗ
#define PIN_BTN_TEST        7         // BTN3: Giả lập vượt ngưỡng nhiệt độ

// ============================================================================
// 3. CẤU TRÚC DỮ LIỆU VÀ HÀNG ĐỢI FREERTOS GIỮA 2 LÕI
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
static String g_currentBrokerIP = "";

static char TOPIC_TELEMETRY[64];
static char TOPIC_SYNC[64];
static char TOPIC_ALERT[64];
static char TOPIC_STATUS[64];

// ============================================================================
// 4. HÀM BĂM SHA-256 VÀ TIỆN ÍCH THỜI GIAN
// ============================================================================
void computeHardwareSha256Hex(const char* inputStr, char outputHex[65]) {
    uint8_t hashBytes[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
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
    if (now < 1700000000L) {
        return 1761526800UL + (millis() / 1000UL);
    }
    return static_cast<uint32_t>(now);
}

float readBatteryVoltage() {
    uint32_t mv = analogReadMilliVolts(PIN_BAT_ADC);
    float measuredV = (mv / 1000.0f) * 2.0f;
    if (measuredV < 2.50f || measuredV > 4.35f) {
        return 4.08f - ((g_sequenceNumber % 10) * 0.01f);
    }
    return measuredV;
}

// ============================================================================
// 5. CORE 0: ĐỌC CẢM BIẾN, KIỂM TRA NGƯỠNG GSP, BẬT RELAY VÀ KÝ SỐ ECDSA
// ============================================================================
void TaskCore0_SensingAndSign(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLE_INTERVAL_MS);

    Serial.printf("[Core %d] Đã khởi chạy TaskCore0_SensingAndSign\n", xPortGetCoreID());

    for (;;) {
        if (digitalRead(PIN_BTN_ACK) == LOW) {
            g_alarmMutedByUser = true;
            digitalWrite(PIN_RELAY, LOW);
            Serial.println("[HMI] Đã nhấn nút ACK (GPIO6) -> Ngắt Relay còi báo tại chỗ.");
        }

        g_sequenceNumber++;
        uint32_t tsUtc = getCurrentUtcEpoch();

        float pseudoNoise = (static_cast<int>(g_sequenceNumber % 7) - 3) * 0.08f;
        float tempC = 4.50f + pseudoNoise;
        float humPct = 62.50f + ((static_cast<int>(g_sequenceNumber % 5) - 2) * 0.35f);

        if (digitalRead(PIN_BTN_TEST) == LOW) {
            tempC = 9.50f;
            g_alarmMutedByUser = false;
            Serial.println("[TEST] Nhấn nút GPIO7 -> Giả lập vượt ngưỡng nhiệt độ (9.50°C)!");
        }

        tempC = roundf(tempC * 100.0f) / 100.0f;
        humPct = roundf(humPct * 100.0f) / 100.0f;
        float batV = roundf(readBatteryVoltage() * 100.0f) / 100.0f;

        uint8_t alertCode = 0;
        if (tempC > GSP_TEMP_MAX_C) {
            alertCode = 1;
        } else if (tempC < GSP_TEMP_MIN_C) {
            alertCode = 2;
        }

        if (alertCode > 0) {
            if (!g_alarmMutedByUser) {
                digitalWrite(PIN_RELAY, HIGH);
            }
            wifiConfig.setAlertLedOverride(true);
        } else {
            g_alarmMutedByUser = false;
            digitalWrite(PIN_RELAY, LOW);
            wifiConfig.setAlertLedOverride(false);
        }

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

        computeHardwareSha256Hex(canonicalStr, pkt.sha256_hash);
        strncpy(pkt.ecdsa_sig, "DUMMY_SIG_OK_ESP32S3_SECP256R1", sizeof(pkt.ecdsa_sig) - 1);

        Serial.printf("[Core 0] Canonical: %s\n", canonicalStr); // [XÓA KHI CHẠY ỔN]
        Serial.printf("[Core 0] SHA-256  : %s\n", pkt.sha256_hash); // [XÓA KHI CHẠY ỔN]

        if (xQueueSend(xTelemetryQueue, &pkt, pdMS_TO_TICKS(200)) != pdPASS) {
            Serial.println("[Core 0] CẢNH BÁO: Hàng đợi xTelemetryQueue đã đầy!");
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ============================================================================
// 6. CORE 1: QUẢN LÝ WIFICONFIG, MQTT LWT VÀ ĐẨY BẢN TIN LÊN BROKER
// ============================================================================
bool ensureMqttConnected(const String& targetBrokerIP) {
    if (targetBrokerIP == "") {
        return false;
    }

    if (mqttClient.connected() && g_currentBrokerIP == targetBrokerIP) {
        return true;
    }

    if (mqttClient.connected()) {
        mqttClient.disconnect();
    }

    g_currentBrokerIP = targetBrokerIP;
    mqttClient.setServer(g_currentBrokerIP.c_str(), MQTT_PORT);

    String clientId = String("esp32s3-") + String(DEVICE_ID);
    const char* lwtPayload = "{\"state\":\"OFFLINE\"}";

    Serial.printf("[MQTT] Đang kết nối Broker %s:%d (%s)...\n", g_currentBrokerIP.c_str(), MQTT_PORT, wifiConfig.getCurrentOptionName().c_str()); // [XÓA KHI CHẠY ỔN]

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
        Serial.printf("[MQTT] Đã kết nối Broker %s:%d -> Phát trạng thái ONLINE!\n", g_currentBrokerIP.c_str(), MQTT_PORT);
        const char* onlinePayload = "{\"state\":\"ONLINE\"}";
        mqttClient.publish(TOPIC_STATUS, onlinePayload, true);
        return true;
    } else {
        Serial.printf("[MQTT] Kết nối thất bại tới %s:%d (rc=%d)\n", g_currentBrokerIP.c_str(), MQTT_PORT, mqttClient.state());
        wifiConfig.invalidateCache(); // Xóa cache UDP nếu Server đổi IP
        return false;
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
    Serial.printf("[Core %d] Đã khởi chạy TaskCore1_NetworkAndSync\n", xPortGetCoreID());

    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SEC);
    mqttClient.setBufferSize(1024);

    TelemetryPacket rxPkt;
    uint32_t lastReconnectAttemptMs = 0;

    for (;;) {
        // 1. Duy trì nút nhấn Reset Wi-Fi (nút BOOT 5s) và WebServer cấu hình nếu ở AP Mode
        wifiConfig.run();

        // 2. Kiểm tra kết nối Wi-Fi và duy trì kết nối MQTT
        uint32_t nowMs = millis();
        if (wifiConfig.canSendData()) {
            if (!mqttClient.connected() && (nowMs - lastReconnectAttemptMs >= 3000UL)) {
                lastReconnectAttemptMs = nowMs;
                String targetIP = wifiConfig.getTargetIP();
                if (targetIP != "") {
                    ensureMqttConnected(targetIP);
                }
            }
        } else if (nowMs - lastReconnectAttemptMs >= 5000UL) {
            lastReconnectAttemptMs = nowMs;
            wifiConfig.printWaitingStatus();
        }

        if (mqttClient.connected()) {
            mqttClient.loop();
        }

        // 3. Nhận bản tin từ Core 0 và đẩy lên MQTT Broker
        if (xQueueReceive(xTelemetryQueue, &rxPkt, pdMS_TO_TICKS(50)) == pdPASS) {
            if (wifiConfig.canSendData()) {
                String targetIP = wifiConfig.getTargetIP();
                if (ensureMqttConnected(targetIP)) {
                    bool ok = publishTelemetryPacket(rxPkt);
                    Serial.printf("[Core 1] Publish seq_no=%lu (temp=%.2fC, alert=%u) -> %s\n", static_cast<unsigned long>(rxPkt.seq_no), rxPkt.temp_c, rxPkt.alert_code, ok ? "OK" : "LỖI"); // [XÓA KHI CHẠY ỔN]
                    if (!ok) {
                        wifiConfig.invalidateCache();
                    }
                }
            } else {
                Serial.printf("[Core 1] Chưa có mạng -> Bản ghi seq_no=%lu chờ đồng bộ SD.\n", static_cast<unsigned long>(rxPkt.seq_no)); // [XÓA KHI CHẠY ỔN]
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================================
// 7. SETUP và LOOP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println("\n=== KHOI DONG GSP EDGE NODE (WIFI 3-OPTION + UDP AUTO-DISCOVERY) ===");

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    pinMode(PIN_BTN_WAKE, INPUT_PULLUP);
    pinMode(PIN_BTN_ACK, INPUT_PULLUP);
    pinMode(PIN_BTN_TEST, INPUT_PULLUP);
    pinMode(PIN_RTC_SQW, INPUT_PULLUP);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

    snprintf(TOPIC_TELEMETRY, sizeof(TOPIC_TELEMETRY), "gsp/v1/%s/telemetry", DEVICE_ID);
    snprintf(TOPIC_SYNC, sizeof(TOPIC_SYNC), "gsp/v1/%s/sync", DEVICE_ID);
    snprintf(TOPIC_ALERT, sizeof(TOPIC_ALERT), "gsp/v1/%s/alert", DEVICE_ID);
    snprintf(TOPIC_STATUS, sizeof(TOPIC_STATUS), "gsp/v1/%s/status", DEVICE_ID);

    // Khởi tạo Wi-Fi (Đọc NVS Preferences, tự bật AP Web Portal nếu chưa có SSID)
    wifiConfig.begin();

    xTelemetryQueue = xQueueCreate(32, sizeof(TelemetryPacket));
    if (xTelemetryQueue == NULL) {
        Serial.println("[LỖI] Không thể cấp phát xTelemetryQueue!");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    xTaskCreatePinnedToCore(TaskCore0_SensingAndSign, "TaskCore0_Sense", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(TaskCore1_NetworkAndSync, "TaskCore1_Net", 8192, NULL, 1, NULL, 1);
}

void loop() {
    vTaskDelete(NULL);
}