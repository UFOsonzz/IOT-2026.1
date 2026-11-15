#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <Ticker.h>
#include <WiFi.h>
#include "app_config.h"

enum class NetMode : uint8_t {
    AP_PORTAL = 0,
    STA_CONNECTED = 1,
    STA_CONNECTING = 2
};

class WifiManager {
public:
    void begin();
    void run();
    void setAlertLedOverride(bool active);
    bool canSendData() const;
    String getTargetIP();
    void invalidateCache();
    String getCurrentOptionName() const;

    // Event & timer callbacks
    void onWiFiEvent(WiFiEvent_t event);
    void updateStatusLed();

private:
    void setupWifi();
    void setupWebServer();
    void checkResetButton();
    String discoverServerViaUDP();
    void toggleLed(uint32_t intervalMs);

    WebServer webServer_{80};
    Ticker ledTicker_;
    Preferences prefs_;

    String ssid_;
    String password_;
    String connMode_ = "opt1";
    String discoveredServerIP_;

    uint32_t lastBtnPressMs_ = 0;
    uint32_t lastBlinkMs_ = 0;
    volatile NetMode netMode_ = NetMode::AP_PORTAL;
    volatile bool alertLedOverride_ = false;
};