#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "app_config.h"
#include "app_types.h"

class MqttService {
public:
    void begin(const char* deviceId);
    void loop();
    bool isConnected();
    bool ensureConnected(const String& brokerIp);
    bool publishTelemetry(const TelemetryPacket& pkt);

private:
    WiFiClient wifiClient_;
    PubSubClient mqttClient_{wifiClient_};
    String currentBrokerIp_;
    String clientId_;

    char topicTelemetry_[64] = {};
    char topicSync_[64]      = {};
    char topicAlert_[64]     = {};
    char topicStatus_[64]    = {};
};