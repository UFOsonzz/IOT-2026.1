#include "wifi_manager.h"
#include "portal_page.h"
#include <ArduinoJson.h>
#include <WiFiUdp.h>

namespace {
WifiManager* g_instance = nullptr;
}

void WifiManager::begin() {
    g_instance = this;
    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_WIFI_RESET_BTN, INPUT_PULLUP);

    ledTicker_.attach_ms(50, +[]() {
        if (g_instance) g_instance->updateStatusLed();
    });

    prefs_.begin("wifi_cfg", true);
    ssid_ = prefs_.getString("ssid", "");
    password_ = prefs_.getString("pass", "");
    connMode_ = prefs_.getString("mode", "opt1");
    prefs_.end();

    setupWifi();
    if (netMode_ == NetMode::AP_PORTAL) {
        setupWebServer();
    }
}

void WifiManager::run() {
    checkResetButton();
    if (netMode_ == NetMode::AP_PORTAL) {
        webServer_.handleClient();
    }
}

void WifiManager::setAlertLedOverride(bool active) {
    alertLedOverride_ = active;
}

bool WifiManager::canSendData() const {
    if (netMode_ == NetMode::STA_CONNECTED && WiFi.status() == WL_CONNECTED) return true;
    if (netMode_ == NetMode::AP_PORTAL && WiFi.softAPgetStationNum() > 0) return true;
    return false;
}

String WifiManager::getTargetIP() {
    if (netMode_ == NetMode::AP_PORTAL) {
        return DEFAULT_AP_CLIENT;
    }
    if (connMode_ == "opt2") {
        return WiFi.gatewayIP().toString();
    }
    if (discoveredServerIP_.isEmpty()) {
        discoveredServerIP_ = discoverServerViaUDP();
    }
    return discoveredServerIP_;
}

void WifiManager::invalidateCache() {
    if (netMode_ == NetMode::STA_CONNECTED && connMode_ == "opt1" && !discoveredServerIP_.isEmpty()) {
        discoveredServerIP_ = "";
    }
}

String WifiManager::getCurrentOptionName() const {
    if (netMode_ == NetMode::AP_PORTAL) return "Option 3 (AP Direct -> 192.168.4.2)";
    if (connMode_ == "opt2") return "Option 2 (Mac Gateway -> " + WiFi.gatewayIP().toString() + ")";
    return "Option 1 (UDP Auto-IP -> " + discoveredServerIP_ + ")";
}

void WifiManager::toggleLed(uint32_t intervalMs) {
    if (millis() - lastBlinkMs_ > intervalMs) {
        digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
        lastBlinkMs_ = millis();
    }
}

void WifiManager::updateStatusLed() {
    if (alertLedOverride_) {
        digitalWrite(PIN_STATUS_LED, HIGH);
        return;
    }
    if (digitalRead(PIN_WIFI_RESET_BTN) == LOW) {
        toggleLed((millis() - lastBtnPressMs_ < WIFI_RESET_HOLD_MS) ? 1000 : 50);
        return;
    }
    switch (netMode_) {
        case NetMode::AP_PORTAL:      toggleLed(100);  break;
        case NetMode::STA_CONNECTED:  toggleLed(3000); break;
        case NetMode::STA_CONNECTING: toggleLed(400);  break;
    }
}

void WifiManager::onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WIFI] STA Connected | IP: %s | GW: %s\n",
                          WiFi.localIP().toString().c_str(),
                          WiFi.gatewayIP().toString().c_str());
            discoveredServerIP_ = "";
            netMode_ = NetMode::STA_CONNECTED;
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[WIFI] Disconnected; reconnecting...");
            netMode_ = NetMode::STA_CONNECTING;
            discoveredServerIP_ = "";
            WiFi.reconnect();
            break;

        default:
            break;
    }
}

void WifiManager::setupWifi() {
    if (!ssid_.isEmpty()) {
        Serial.printf("[WIFI] Starting STA -> SSID: [%s] | Mode: [%s]\n", ssid_.c_str(), connMode_.c_str());
        netMode_ = NetMode::STA_CONNECTING;
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false); // Disable modem sleep for deterministic UDP broadcast response
        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t) {
            if (g_instance) g_instance->onWiFiEvent(event);
        });
        WiFi.begin(ssid_.c_str(), password_.c_str());
    } else {
        WiFi.mode(WIFI_AP_STA);
        WiFi.setSleep(false);
        uint8_t mac[6];
        WiFi.softAPmacAddress(mac);
        String apSsid = "GSP-ESP32-" + String(mac[4], HEX) + String(mac[5], HEX);
        apSsid.toUpperCase();
        WiFi.softAP(apSsid.c_str());

        Serial.printf("[WIFI AP] SSID: %s | Portal: http://%s\n",
                      apSsid.c_str(), WiFi.softAPIP().toString().c_str());
        netMode_ = NetMode::AP_PORTAL;
    }
}

void WifiManager::setupWebServer() {
    webServer_.on("/", [this]() {
        webServer_.send(200, "text/html", PORTAL_HTML);
    });

    webServer_.on("/scanWifi", [this]() {
        int count = WiFi.scanNetworks(false, false);
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < count; ++i) {
            arr.add(WiFi.SSID(i));
        }
        String res;
        serializeJson(doc, res);
        WiFi.scanDelete();
        webServer_.send(200, "application/json", res);
    });

    webServer_.on("/saveWifi", [this]() {
        String ssidArg = webServer_.arg("ssid");
        String passArg = webServer_.arg("pass");
        String modeArg = webServer_.arg("mode");
        if (modeArg != "opt1" && modeArg != "opt2") modeArg = "opt1";

        prefs_.begin("wifi_cfg", false);
        prefs_.putString("ssid", ssidArg);
        prefs_.putString("pass", passArg);
        prefs_.putString("mode", modeArg);
        prefs_.end();

        Serial.printf("[WEB CONFIG] Saved SSID: %s | Mode: %s\n", ssidArg.c_str(), modeArg.c_str());
        String modeName = (modeArg == "opt1") ? "Option 1 (UDP Auto-Discovery)" : "Option 2 (Mac Gateway)";
        webServer_.send(200, "text/plain", "Đã lưu cấu hình " + modeName + "! Hãy bấm nút KHỞI ĐỘNG LẠI.");
    });

    webServer_.on("/reStart", [this]() {
        webServer_.send(200, "text/plain", "ESP32 đang khởi động lại...");
        delay(1000);
        ESP.restart();
    });

    webServer_.begin();
}

void WifiManager::checkResetButton() {
    if (digitalRead(PIN_WIFI_RESET_BTN) == LOW) {
        if (millis() - lastBtnPressMs_ > WIFI_RESET_HOLD_MS) {
            Serial.println("\n[WIFI RESET] BOOT held 5s -> Clearing NVS WiFi config, reverting to AP Mode...");
            prefs_.begin("wifi_cfg", false);
            prefs_.clear();
            prefs_.end();
            delay(500);
            ESP.restart();
        }
    } else {
        lastBtnPressMs_ = millis();
    }
}

String WifiManager::discoverServerViaUDP() {
    WiFiUDP udp;
    udp.begin(0);

    IPAddress subnetBroadcast = static_cast<uint32_t>(WiFi.localIP()) | ~(static_cast<uint32_t>(WiFi.subnetMask()));
    IPAddress globalBroadcast(255, 255, 255, 255);

    for (int attempt = 1; attempt <= 2; ++attempt) {
        udp.beginPacket(subnetBroadcast, UDP_DISCOVERY_PORT);
        udp.print("WHO_IS_IOT_SERVER");
        udp.endPacket();

        udp.beginPacket(globalBroadcast, UDP_DISCOVERY_PORT);
        udp.print("WHO_IS_IOT_SERVER");
        udp.endPacket();

        const uint32_t startWait = millis();
        while (millis() - startWait < 700) {
            int packetSize = udp.parsePacket();
            if (packetSize > 0) {
                char buf[64];
                int len = udp.read(buf, sizeof(buf) - 1);
                buf[len] = '\0';
                if (strcmp(buf, "IOT_SERVER_HERE") == 0) {
                    String foundIP = udp.remoteIP().toString();
                    Serial.printf("[UDP DISCOVERY] Found Broker at IP: %s\n", foundIP.c_str());
                    udp.stop();
                    return foundIP;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    udp.stop();
    return "";
}