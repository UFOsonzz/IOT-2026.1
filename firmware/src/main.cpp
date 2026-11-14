#include <Arduino.h>
#include "app_config.h"
#include "app_types.h"
#include "crypto_signer.h"
#include "mqtt_service.h"
#include "peripheral_manager.h"
#include "wifi_manager.h"

namespace {

PeripheralManager g_peripherals;
WifiManager       g_wifi;
MqttService       g_mqtt;
QueueHandle_t     g_telemetryQueue = nullptr;
uint32_t          g_sequenceNumber = 1000;

// Core 0: Deterministic sensing, I2C HMI, local alarm relay, and cryptographic signing
void sensingTask(void* pvParameters) {
    uint32_t lastSampleMs = 0;
    uint32_t lastEpoch = 0;
    uint32_t lastStatusMs = 0;
    bool sampleRequested = true;

    for (;;) {
        g_peripherals.update();
        const uint32_t now = millis();
        sampleRequested |= g_peripherals.takeSampleRequest();

        const SensorReading r = g_peripherals.reading();
        g_wifi.setAlertLedOverride(!r.sensorOk || r.alert > 0);

        if ((sampleRequested || now - lastSampleMs >= SAMPLE_INTERVAL_MS)
            && r.sensorOk && r.rtcOk && r.epoch != lastEpoch) {
            lastSampleMs = now;
            lastEpoch = r.epoch;
            sampleRequested = false;

            TelemetryPacket pkt = {};
            strncpy(pkt.device_id, DEVICE_ID, sizeof(pkt.device_id) - 1);
            pkt.seq_no     = ++g_sequenceNumber;
            pkt.ts_rtc     = r.epoch;
            pkt.temp_c     = roundf(r.temp * 100.0f) / 100.0f;
            pkt.hum_pct    = roundf(r.hum * 100.0f) / 100.0f;
            pkt.bat_v      = roundf(r.battery * 100.0f) / 100.0f;
            pkt.alert_code = r.alert;

            crypto::signTelemetryPacket(pkt);

            if (xQueueSend(g_telemetryQueue, &pkt, 0) != pdPASS) {
                Serial.println(F("[Core 0] Telemetry queue full; packet dropped"));
            }
        }

        if (now - lastStatusMs >= STATUS_LOG_INTERVAL_MS) {
            lastStatusMs = now;
            g_peripherals.printStatus();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Core 1: Non-deterministic network I/O, Web Portal, UDP discovery, and MQTT publishing
void networkTask(void* pvParameters) {
    Serial.printf("[Core %d] Network task started\n", xPortGetCoreID());
    g_mqtt.begin(DEVICE_ID);

    TelemetryPacket rxPkt;
    uint32_t lastReconnectMs = 0;

    for (;;) {
        g_wifi.run();

        const uint32_t now = millis();
        if (g_wifi.canSendData() && !g_mqtt.isConnected() && (now - lastReconnectMs >= MQTT_RECONNECT_MS)) {
            lastReconnectMs = now;
            const String targetIp = g_wifi.getTargetIP();
            if (!targetIp.isEmpty() && !g_mqtt.ensureConnected(targetIp)) {
                g_wifi.invalidateCache();
            }
        }

        g_mqtt.loop();

        const bool staConnected = (WiFi.status() == WL_CONNECTED);
        const IPAddress activeIp = staConnected ? WiFi.localIP() : WiFi.softAPIP();
        g_peripherals.setNetwork(staConnected, g_mqtt.isConnected(), activeIp.toString().c_str());

        if (xQueueReceive(g_telemetryQueue, &rxPkt, pdMS_TO_TICKS(50)) == pdPASS) {
            if (g_wifi.canSendData()) {
                const String targetIp = g_wifi.getTargetIP();
                if (g_mqtt.ensureConnected(targetIp)) {
                    if (!g_mqtt.publishTelemetry(rxPkt)) {
                        g_wifi.invalidateCache();
                    }
                } else {
                    g_wifi.invalidateCache();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println(F("\n=== GSP EDGE NODE STARTING ==="));

    g_peripherals.begin();
    g_wifi.begin();

    g_telemetryQueue = xQueueCreate(TELEMETRY_QUEUE_LEN, sizeof(TelemetryPacket));
    if (!g_telemetryQueue) {
        Serial.println(F("[FATAL] Failed to allocate telemetry queue"));
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    xTaskCreatePinnedToCore(sensingTask, "SenseTask", 8192, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(networkTask, "NetTask",   8192, nullptr, 1, nullptr, 1);
}

void loop() {
    vTaskDelete(nullptr);
}