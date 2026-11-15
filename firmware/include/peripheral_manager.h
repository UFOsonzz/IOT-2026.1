#pragma once

#include <Arduino.h>
#include "app_config.h"
#include "app_types.h"
#include "gsp_codec.h"

// Owns all I2C peripherals, GPIO interrupts, local alarm state, and Serial CLI.
// Must be polled exclusively from a single task context (Core 0).
class PeripheralManager {
public:
    void begin();
    void update();
    SensorReading reading() const { return latest_; }
    bool isRtcHardware() const { return rtcModulePresent_; }
    bool takeSampleRequest();
    void setNetwork(bool wifi, bool mqtt, const char* ip);
    void printStatus();

private:
    void pollButtons();
    void handleAction(uint8_t button);
    void pollSerial();
    void executeCommand(char* line);
    void scanI2c();
    bool readRtc();
    bool setRtc(uint32_t epoch);
    void configureSqw();
    void updateDisplay();
    void evaluateAlarm();

    gsp::DebouncedButton debounce_[3];
    uint32_t irqSeen_[3] = {};
    SensorReading latest_;
    float realTemp_ = 0.0f;

    uint32_t lastSensor_ = 0;
    uint32_t measureStarted_ = 0;
    uint32_t lastGoodSensor_ = 0;
    uint32_t lastRtc_ = 0;
    uint32_t lastOled_ = 0;
    uint32_t testStarted_ = 0;
    uint32_t lastSqw_ = 0;
    uint32_t sqwSeen_ = 0;
    uint32_t sqwLastEdge_ = 0;

    bool measuring_ = false;
    bool oledOk_ = false;
    bool test_ = false;
    bool muted_ = false;
    bool requestSample_ = false;
    bool sqwWarning_ = false;
    bool rtcModulePresent_ = false;

    uint8_t page_ = 0;
    uint8_t previousAlarm_ = 0;
    char serialLine_[48] = {};
    uint8_t serialLength_ = 0;
    bool serialOverflow_ = false;
};