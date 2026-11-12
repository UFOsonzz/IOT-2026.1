#include <Arduino.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include "WifiConfig.h"
#include "Sprint1Hardware.h"

#define MQTT_KEEPALIVE_SEC 30
static Sprint1Hardware sprint1Hardware;

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

// Core 0 owns I2C, OLED, RTC, Serial commands and all local HMI/alarm state.
void TaskCore0_SensingAndSign(void* pvParameters) {
    uint32_t lastSampleMs = 0, lastEpoch = 0, lastStatusMs = 0;
    bool sampleRequested = true;
    for (;;) {
        sprint1Hardware.update();
        const uint32_t now = millis();
        sampleRequested |= sprint1Hardware.takeSampleRequest();
        const Sprint1Reading r = sprint1Hardware.reading();
        wifiConfig.setAlertLedOverride(!r.sensorOk || r.alert > 0);
        if ((sampleRequested || now - lastSampleMs >= SAMPLE_INTERVAL_MS)
            && r.sensorOk && r.rtcOk && r.epoch != lastEpoch) {
            lastSampleMs = now; lastEpoch = r.epoch; sampleRequested = false;
            TelemetryPacket pkt = {};
            strncpy(pkt.device_id, DEVICE_ID, sizeof(pkt.device_id) - 1);
            pkt.seq_no = ++g_sequenceNumber;
            pkt.ts_rtc = r.epoch;
            pkt.temp_c = roundf(r.temp * 100.0f) / 100.0f;
            pkt.hum_pct = roundf(r.hum * 100.0f) / 100.0f;
            pkt.bat_v = roundf(r.battery * 100.0f) / 100.0f;
            pkt.alert_code = r.alert;
            char canonicalStr[160];
            snprintf(canonicalStr, sizeof(canonicalStr), "%s|%lu|%lu|%.2f|%.2f|%u",
                DEVICE_ID, static_cast<unsigned long>(pkt.seq_no),
                static_cast<unsigned long>(pkt.ts_rtc), pkt.temp_c, pkt.hum_pct, pkt.alert_code);
            computeHardwareSha256Hex(canonicalStr, pkt.sha256_hash);
            strncpy(pkt.ecdsa_sig, "DUMMY_SIG_OK_ESP32_SECP256R1", sizeof(pkt.ecdsa_sig) - 1);
            Serial.printf("[Core 0] Canonical: %s\n", canonicalStr);
            if (xQueueSend(xTelemetryQueue, &pkt, 0) != pdPASS)
                Serial.println("[Core 0] Queue full; packet dropped (SD buffering is Sprint 2).");
        }
        if (now - lastStatusMs >= 10000) { lastStatusMs = now; sprint1Hardware.printStatus(); }
        vTaskDelay(pdMS_TO_TICKS(10));
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

    String clientId = String("esp32-") + String(DEVICE_ID);
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

        sprint1Hardware.setNetwork(WiFi.status() == WL_CONNECTED, mqttClient.connected(),
            (WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()).toString().c_str());

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
                Serial.printf("[Core 1] Chưa có mạng -> Bỏ bản ghi seq_no=%lu (SD buffering thuộc Sprint 2).\n", static_cast<unsigned long>(rxPkt.seq_no)); // [XÓA KHI CHẠY ỔN]
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

    sprint1Hardware.begin();

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