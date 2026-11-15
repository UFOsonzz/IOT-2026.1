#include "mqtt_service.h"
#include <ArduinoJson.h>

void MqttService::begin(const char* deviceId) {
    clientId_ = String("esp32-") + deviceId;
    snprintf(topicTelemetry_, sizeof(topicTelemetry_), "gsp/v1/%s/telemetry", deviceId);
    snprintf(topicSync_, sizeof(topicSync_), "gsp/v1/%s/sync", deviceId);
    snprintf(topicAlert_, sizeof(topicAlert_), "gsp/v1/%s/alert", deviceId);
    snprintf(topicStatus_, sizeof(topicStatus_), "gsp/v1/%s/status", deviceId);

    mqttClient_.setKeepAlive(MQTT_KEEPALIVE_SEC);
    mqttClient_.setBufferSize(MQTT_BUFFER_SIZE);
}

void MqttService::loop() {
    if (mqttClient_.connected()) {
        mqttClient_.loop();
    }
}

bool MqttService::isConnected() {
    return mqttClient_.connected();
}

bool MqttService::ensureConnected(const String& brokerIp) {
    if (brokerIp.isEmpty()) return false;
    if (mqttClient_.connected() && currentBrokerIp_ == brokerIp) return true;

    if (mqttClient_.connected()) {
        mqttClient_.disconnect();
    }

    currentBrokerIp_ = brokerIp;
    mqttClient_.setServer(currentBrokerIp_.c_str(), MQTT_PORT);

    const char* lwtPayload = "{\"state\":\"OFFLINE\"}";
    const bool connected = mqttClient_.connect(
        clientId_.c_str(),
        nullptr,
        nullptr,
        topicStatus_,
        1,
        true,
        lwtPayload
    );

    if (connected) {
        Serial.printf("[MQTT] Connected to %s:%u -> LWT ONLINE published\n", currentBrokerIp_.c_str(), MQTT_PORT);
        mqttClient_.publish(topicStatus_, "{\"state\":\"ONLINE\"}", true);
        return true;
    }

    Serial.printf("[MQTT] Connect failed to %s:%u (rc=%d)\n", currentBrokerIp_.c_str(), MQTT_PORT, mqttClient_.state());
    return false;
}

bool MqttService::publishTelemetry(const TelemetryPacket& pkt) {
    JsonDocument doc;
    doc["device_id"]   = pkt.device_id;
    doc["seq_no"]      = pkt.seq_no;
    doc["ts_rtc"]      = pkt.ts_rtc;
    doc["temp_c"]      = serialized(String(pkt.temp_c, 2));
    doc["hum_pct"]     = serialized(String(pkt.hum_pct, 2));
    doc["bat_v"]       = serialized(String(pkt.bat_v, 2));
    doc["alert_code"]  = pkt.alert_code;
    doc["sha256_hash"] = pkt.sha256_hash;
    doc["ecdsa_sig"]   = pkt.ecdsa_sig;

    char jsonBuf[512];
    const size_t len = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
    const char* targetTopic = pkt.is_backfill ? topicSync_ : topicTelemetry_;
    const bool pubOk = mqttClient_.publish(targetTopic, reinterpret_cast<const uint8_t*>(jsonBuf), len, false);

    if (pubOk && pkt.alert_code > 0) {
        JsonDocument alertDoc;
        alertDoc["device_id"]  = pkt.device_id;
        alertDoc["seq_no"]     = pkt.seq_no;
        alertDoc["ts_rtc"]     = pkt.ts_rtc;
        alertDoc["alert_code"] = pkt.alert_code;
        alertDoc["severity"]   = "CRITICAL";
        alertDoc["temp_c"]     = serialized(String(pkt.temp_c, 2));
        alertDoc["hum_pct"]    = serialized(String(pkt.hum_pct, 2));
        alertDoc["message"]    = "Canh bao vuot nguong nhiet do GSP tai thiet bi bien!";

        char alertBuf[384];
        const size_t alertLen = serializeJson(alertDoc, alertBuf, sizeof(alertBuf));
        mqttClient_.publish(topicAlert_, reinterpret_cast<const uint8_t*>(alertBuf), alertLen, false);
    }

    return pubOk;
}