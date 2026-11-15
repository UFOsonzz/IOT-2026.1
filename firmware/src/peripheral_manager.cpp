#include "peripheral_manager.h"
#include <Wire.h>
#include <U8g2lib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ISR_ATTR IRAM_ATTR

namespace {

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);
const uint8_t kButtonPins[] = {PIN_BTN_WAKE, PIN_BTN_ACK, PIN_BTN_TEST};

struct Edge {
    uint32_t generation;
    uint32_t at;
    bool down;
};

struct NetworkSnapshot {
    bool wifi;
    bool mqtt;
    char ip[16];
};

volatile Edge g_edges[3] = {};
volatile uint32_t g_sqwCount = 0;
NetworkSnapshot g_netState = {};
portMUX_TYPE g_stateMux = portMUX_INITIALIZER_UNLOCKED;

inline void lockState()   { portENTER_CRITICAL(&g_stateMux); }
inline void unlockState() { portEXIT_CRITICAL(&g_stateMux); }

void ISR_ATTR rtcInterrupt() {
    portENTER_CRITICAL_ISR(&g_stateMux);
    g_sqwCount = g_sqwCount + 1;
    portEXIT_CRITICAL_ISR(&g_stateMux);
}

void ISR_ATTR captureButton(uint8_t index, bool down) {
    portENTER_CRITICAL_ISR(&g_stateMux);
    g_edges[index].at = millis();
    g_edges[index].down = down;
    g_edges[index].generation = g_edges[index].generation + 1;
    portEXIT_CRITICAL_ISR(&g_stateMux);
}

void ISR_ATTR wakeInterrupt() { captureButton(0, digitalRead(PIN_BTN_WAKE) == LOW); }
void ISR_ATTR ackInterrupt()  { captureButton(1, digitalRead(PIN_BTN_ACK) == LOW); }
void ISR_ATTR testInterrupt() { captureButton(2, digitalRead(PIN_BTN_TEST) == LOW); }

bool probeI2c(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool readRtcRegs(uint8_t reg, uint8_t* bytes, uint8_t count) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(DS3231_ADDRESS, count) != count) return false;
    for (uint8_t i = 0; i < count; ++i) bytes[i] = Wire.read();
    return true;
}

bool writeRtcReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

} // namespace

void PeripheralManager::begin() {
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);
    for (uint8_t pin : kButtonPins) pinMode(pin, INPUT_PULLUP);
    pinMode(PIN_RTC_SQW, INPUT_PULLUP);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setTimeOut(25);
    Wire.setClock(I2C_CLOCK_HZ);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

    attachInterrupt(digitalPinToInterrupt(PIN_BTN_WAKE), wakeInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_ACK), ackInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_TEST), testInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RTC_SQW), rtcInterrupt, FALLING);

    scanI2c();
    configureSqw();
    if (!readRtc()) {
        Serial.println(F("[RTC ERROR] Missing/OSF/invalid date; set UTC TIME before telemetry"));
    }

    lastSensor_ = millis() - SENSOR_INTERVAL_MS;
    lastRtc_ = millis();
    sqwLastEdge_ = millis();
    Serial.println(F("[READY] GSP Hardware initialized. HELP for commands; RTC must store UTC."));
}

void PeripheralManager::configureSqw() {
    uint8_t ctrl;
    // EOSC=0, INTCN=0, RS2=RS1=0 -> 1Hz square wave output
    if (readRtcRegs(0x0e, &ctrl, 1) && writeRtcReg(0x0e, ctrl & ~0x9c)) {
        Serial.println(F("[RTC] SQW configured: 1Hz"));
    }
}

void PeripheralManager::scanI2c() {
    Serial.print(F("[I2C] Found:"));
    for (uint8_t addr = 1; addr < 127; ++addr) {
        if (probeI2c(addr)) {
            Serial.print(F(" 0x"));
            Serial.print(addr, HEX);
        }
    }
    Serial.println();
}

bool PeripheralManager::readRtc() {
    uint8_t regs[7], status;
    const bool wasOk = latest_.rtcOk;
    const bool hadModule = rtcModulePresent_;
    uint32_t rtcEpoch = 0;

    const bool hwPresent = readRtcRegs(0, regs, 7) && readRtcRegs(0x0f, &status, 1);
    const bool hwValid = hwPresent && !(status & 0x80) && gsp::decodeRtc(regs, 7, rtcEpoch);

    if (hwValid) {
        rtcModulePresent_ = true;
        latest_.epoch = rtcEpoch;
        latest_.rtcOk = true;
        if (!wasOk || !hadModule) Serial.println(F("[RTC] UTC valid from RTC module"));
    } else {
        rtcModulePresent_ = false;
        const time_t utc = time(nullptr);
        if (utc >= 1704067200LL && utc <= 4102444799LL) {
            latest_.epoch = static_cast<uint32_t>(utc);
            latest_.rtcOk = true;
            if (!wasOk || hadModule) Serial.println(F("[TIME] Fallback to NTP network time (no RTC module)"));
        } else {
            latest_.epoch = 0;
            latest_.rtcOk = false;
            if (wasOk) Serial.println(F("[RTC ERROR] Missing/OSF/invalid date and no NTP; telemetry paused"));
        }
    }
    return latest_.rtcOk;
}

bool PeripheralManager::setRtc(uint32_t epoch) {
    uint8_t regs[7], status;
    if (!gsp::encodeRtc(epoch, regs) || !readRtcRegs(0x0f, &status, 1)) return false;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(static_cast<uint8_t>(0));
    Wire.write(regs, 7);
    // Clear OSF (Oscillator Stop Flag) bit on valid time update
    if (Wire.endTransmission() != 0 || !writeRtcReg(0x0f, status & ~0x80)) return false;

    configureSqw();
    if (!readRtc()) return false;

    requestSample_ = true;
    Serial.print(F("[RTC SET OK] UTC epoch="));
    Serial.println(latest_.epoch);
    return true;
}

void PeripheralManager::update() {
    pollButtons();
    pollSerial();
    const uint32_t now = millis();

    // Non-blocking sensor measurement state machine
    if (!measuring_ && now - lastSensor_ >= SENSOR_INTERVAL_MS) {
        lastSensor_ = now;
        Wire.beginTransmission(SENSOR_I2C_ADDR);
#if SENSOR_CMD_LEN == 1
        Wire.write(SENSOR_CMD_BYTE_1);
#else
        Wire.write(SENSOR_CMD_BYTE_1);
        Wire.write(SENSOR_CMD_BYTE_2);
#endif
        measuring_ = (Wire.endTransmission() == 0);
        measureStarted_ = now;
        if (!measuring_) {
            latest_.sensorOk = false;
            Serial.printf("[%s ERROR] NACK at 0x%02X\n", SENSOR_NAME, SENSOR_I2C_ADDR);
        }
    }

    if (measuring_ && now - measureStarted_ >= SENSOR_CONVERSION_DELAY_MS) {
        measuring_ = false;
        uint8_t bytes[6];
        const uint8_t count = Wire.requestFrom(SENSOR_I2C_ADDR, static_cast<uint8_t>(6));
        for (uint8_t i = 0; i < count && i < 6; ++i) bytes[i] = Wire.read();

        latest_.sensorOk = SENSOR_DECODE_FUNC(bytes, count, realTemp_, latest_.hum);
        if (latest_.sensorOk) {
            lastGoodSensor_ = now;
        } else {
            Serial.printf("[%s ERROR] Short read or CRC mismatch; telemetry paused\n", SENSOR_NAME);
        }
    }

    if (latest_.sensorOk && now - lastGoodSensor_ > SENSOR_MAX_AGE_MS) {
        latest_.sensorOk = false;
    }

    uint32_t ticks;
    lockState();
    ticks = g_sqwCount;
    unlockState();

    if (ticks != sqwSeen_) {
        sqwSeen_ = ticks;
        sqwLastEdge_ = now;
        if (sqwWarning_) {
            Serial.println(F("[SQW] 1Hz edges recovered"));
            sqwWarning_ = false;
        }
    }

    if (now - lastRtc_ >= 1000 || ticks != lastSqw_ || (!latest_.rtcOk && time(nullptr) >= 1704067200LL)) {
        lastRtc_ = now;
        lastSqw_ = ticks;
        readRtc();
    }

    if (now - sqwLastEdge_ > 2500 && !sqwWarning_) {
        sqwWarning_ = true;
        Serial.println(F("[SQW WARN] No 1Hz edge on SQW pin; reading RTC over I2C each second"));
    }

    if (test_ && now - testStarted_ >= GSP_TEST_DURATION_MS) {
        test_ = false;
        requestSample_ = true;
        Serial.printf("[TEST] End; using real %s temperature\n", SENSOR_NAME);
    }

    latest_.temp = test_ ? 9.50f : realTemp_;
    latest_.alert = latest_.sensorOk ? gsp::alertFor(latest_.temp) : 0;

#if GSP_BATTERY_CONNECTED
    latest_.battery = analogReadMilliVolts(PIN_BAT_ADC) / 1000.0f * 2.0f;
    latest_.batteryOk = latest_.battery >= 2.50f && latest_.battery <= 4.35f;
#endif
    if (!latest_.batteryOk) latest_.battery = 0.0f;

    evaluateAlarm();
    if (now - lastOled_ >= 500) {
        lastOled_ = now;
        updateDisplay();
    }
}

void PeripheralManager::pollButtons() {
    for (uint8_t i = 0; i < 3; ++i) {
        Edge edge;
        lockState();
        edge.generation = g_edges[i].generation;
        edge.at = g_edges[i].at;
        edge.down = g_edges[i].down;
        unlockState();

        if (edge.generation != irqSeen_[i]) {
            irqSeen_[i] = edge.generation;
            debounce_[i].edge(edge.down, edge.at);
        }
        if (debounce_[i].update(millis(), BUTTON_DEBOUNCE_MS)) {
            handleAction(i);
        }
    }
}

void PeripheralManager::handleAction(uint8_t button) {
    if (button == 0) {
        page_ ^= 1;
        Serial.print(F("[HMI] BTN1 page="));
        Serial.println(page_ + 1);
    } else if (button == 1) {
        muted_ = true;
        digitalWrite(PIN_RELAY, LOW);
        Serial.println(F("[HMI] BTN2 ACK/MUTE (data alert unchanged)"));
    } else {
        test_ = true;
        testStarted_ = millis();
        muted_ = false;
        requestSample_ = true;
        Serial.println(F("[HMI] BTN3 TEST=9.50C for 15s"));
    }
}

void PeripheralManager::evaluateAlarm() {
    // Code 3 is reserved for local hardware fault so telemetry alert_code stays uncorrupted
    const uint8_t code = !latest_.sensorOk ? 3 : latest_.alert;
    if (code != previousAlarm_) {
        muted_ = false;
        requestSample_ = true;
    }
    previousAlarm_ = code;
    if (!code) muted_ = false;
    digitalWrite(PIN_RELAY, (code && !muted_) ? HIGH : LOW);
}

bool PeripheralManager::takeSampleRequest() {
    const bool result = requestSample_;
    requestSample_ = false;
    return result;
}

void PeripheralManager::setNetwork(bool wifi, bool mqtt, const char* ip) {
    lockState();
    g_netState.wifi = wifi;
    g_netState.mqtt = mqtt;
    strncpy(g_netState.ip, ip, sizeof(g_netState.ip) - 1);
    g_netState.ip[sizeof(g_netState.ip) - 1] = '\0';
    unlockState();
}

void PeripheralManager::printStatus() {
    Serial.printf("[STATUS] %s=", SENSOR_NAME); Serial.print(latest_.sensorOk ? F("OK") : F("ERROR"));
    Serial.print(F(" temp=")); Serial.print(latest_.temp, 2);
    Serial.print(F(" hum=")); Serial.print(latest_.hum, 2);
    Serial.print(F(" RTC=")); Serial.print(latest_.rtcOk ? F("OK") : F("INVALID"));
    Serial.print(F(" epoch=")); Serial.print(latest_.epoch);
    Serial.print(F(" src=")); Serial.print(rtcModulePresent_ ? F("RTC") : F("NTP"));
    Serial.print(F(" SQW=")); Serial.print(sqwSeen_);
    Serial.print(F(" OLED=")); Serial.print(oledOk_ ? F("OK") : F("MISSING"));
    Serial.print(F(" BAT="));
    if (latest_.batteryOk) Serial.print(latest_.battery, 2);
    else Serial.print(F("N/A"));
    Serial.print(F(" alert=")); Serial.print(latest_.alert);
    Serial.print(F(" muted=")); Serial.print(muted_);
    Serial.print(F(" test=")); Serial.println(test_);
}

void PeripheralManager::pollSerial() {
    // Cap bytes processed per cycle to avoid starving I2C/HMI on UART flood
    uint8_t budget = 48;
    while (budget-- && Serial.available()) {
        const char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            if (serialOverflow_) {
                Serial.println(F("[ERROR] Command too long"));
            } else {
                serialLine_[serialLength_] = '\0';
                executeCommand(serialLine_);
            }
            serialLength_ = 0;
            serialOverflow_ = false;
        } else if (serialLength_ < sizeof(serialLine_) - 1) {
            serialLine_[serialLength_++] = c;
        } else {
            serialOverflow_ = true;
        }
    }
}

void PeripheralManager::executeCommand(char* line) {
    if (!strcmp(line, "STATUS")) printStatus();
    else if (!strcmp(line, "SCAN")) { scanI2c(); configureSqw(); }
    else if (!strcmp(line, "WAKE")) handleAction(0);
    else if (!strcmp(line, "ACK"))  handleAction(1);
    else if (!strcmp(line, "TEST")) handleAction(2);
    else if (!strncmp(line, "TIME ", 5)) {
        char* end;
        const unsigned long epoch = strtoul(line + 5, &end, 10);
        bool digits = line[5] != '\0';
        for (char* p = line + 5; *p; ++p) {
            if (*p < '0' || *p > '9') digits = false;
        }
        if (!digits || *end || epoch < 1704067200UL || epoch > 4102444799UL || !setRtc(epoch)) {
            Serial.println(F("[RTC SET ERROR] Use TIME <UTC epoch 2024..2099>; check 0x68"));
        }
    } else if (line[0]) {
        Serial.println(F("[HELP] STATUS | SCAN | TIME <UTC epoch> | WAKE | ACK | TEST"));
    }
}

void PeripheralManager::updateDisplay() {
    if (!probeI2c(OLED_ADDRESS)) {
        oledOk_ = false;
        return;
    }
    if (!oledOk_) {
        oled.setI2CAddress(OLED_ADDRESS << 1);
        oled.setBusClock(I2C_CLOCK_HZ);
        oled.begin();
        Wire.setClock(I2C_CLOCK_HZ);
        oledOk_ = true;
    }

    NetworkSnapshot net;
    lockState();
    net = g_netState;
    unlockState();

    oled.firstPage();
    do {
        oled.setFont(u8g2_font_5x7_tf);
        oled.setCursor(0, 8);
        oled.print(F("GSP MONITOR  PAGE "));
        oled.print(page_ + 1);

        if (!page_) {
            oled.setCursor(0, 20);
            oled.print(F("Temp: "));
            if (latest_.sensorOk) { oled.print(latest_.temp, 2); oled.print(F(" C")); }
            else oled.print(F("SENSOR ERROR"));

            oled.setCursor(0, 30);
            oled.print(F("Hum : "));
            if (latest_.sensorOk) { oled.print(latest_.hum, 2); oled.print(F(" %")); }
            else oled.print(F("N/A"));

            oled.setCursor(0, 40);
            oled.print(F("Bat : "));
            if (latest_.batteryOk) { oled.print(latest_.battery, 2); oled.print(F(" V")); }
            else oled.print(F("N/A (USB only)"));

            oled.setCursor(0, 50);
            oled.print(F("Alarm: "));
            oled.print(latest_.alert);
            if (muted_) oled.print(F(" MUTE"));
            if (test_)  oled.print(F(" TEST"));
        } else {
            oled.setCursor(0, 20);
            oled.print(F("WiFi: "));
            oled.print(net.wifi ? F("ON") : F("OFF/AP"));
            oled.setCursor(0, 30);
            oled.print(F("MQTT: "));
            oled.print(net.mqtt ? F("ON") : F("OFF"));
            oled.setCursor(0, 40);
            oled.print(net.ip);
            oled.setCursor(0, 50);
            oled.print(F("SQW ticks: "));
            oled.print(sqwSeen_);
        }

        oled.setCursor(0, 62);
        if (latest_.rtcOk) {
            oled.print(rtcModulePresent_ ? F("RTC ") : F("NTP "));
            oled.print(latest_.epoch);
        } else {
            oled.print(F("RTC INVALID: set TIME"));
        }
    } while (oled.nextPage());
}