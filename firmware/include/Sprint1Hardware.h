#pragma once

#include <Arduino.h>
#include "HardwareConfig.h"
#include "Sprint1Logic.h"

struct Sprint1Reading {
    float temp = 0, hum = 0, battery = 0;
    uint32_t epoch = 0;
    uint8_t alert = 0;
    bool sensorOk = false, rtcOk = false, batteryOk = false;
};

// All I2C and HMI work runs in one task/main loop, never inside an ISR.
class Sprint1Hardware {
public:
    void begin();
    void update();
    Sprint1Reading reading();
    bool isRtcHardware() const { return rtcModulePresent_; }
    bool takeSampleRequest();
    void setNetwork(bool wifi, bool mqtt, const char* ip);
    void printStatus();
private:
    void buttons();
    void action(uint8_t button);
    void serialCommands();
    void command(char* line);
    void scan();
    bool readRtc();
    bool setRtc(uint32_t epoch);
    void configureSqw();
    void updateDisplay();
    void alarm();

    sprint1::DebouncedButton debounce_[3];
    uint32_t irqSeen_[3] = {};
    Sprint1Reading latest_;
    float realTemp_ = 0;
    uint32_t lastSensor_ = 0, measureStarted_ = 0, lastGoodSensor_ = 0;
    uint32_t lastRtc_ = 0, lastOled_ = 0, testStarted_ = 0;
    uint32_t lastSqw_ = 0, sqwSeen_ = 0, sqwLastEdge_ = 0;
    bool measuring_ = false, oledOk_ = false, test_ = false, muted_ = false;
    bool requestSample_ = false, sqwWarning_ = false;
    bool rtcModulePresent_ = false;
    uint8_t page_ = 0, previousAlarm_ = 0;
    char serialLine_[48] = {};
    uint8_t serialLength_ = 0;
    bool serialOverflow_ = false;
};
