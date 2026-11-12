#include "Sprint1Hardware.h"
#include "Wire.h"
#include <cassert>
#include <cmath>
#include <cstdio>

uint32_t mockMillis = 1000;
time_t mockUtc = 0;
int mockPins[40], mockModes[40];
void (*mockInterrupts[40])() = {};
MockSerial Serial;
MockWire Wire;
std::string mockDisplay;

void step(Sprint1Hardware& hw, uint32_t elapsed) { mockMillis += elapsed; hw.update(); }
void edge(int pin, bool down) {
    mockPins[pin] = down ? LOW : HIGH;
    assert(mockInterrupts[pin]); mockInterrupts[pin]();
}
void click(Sprint1Hardware& hw, int pin) {
    edge(pin, true); hw.update(); step(hw, 35);
    edge(pin, false); hw.update(); step(hw, 35);
}
void setSensor(float temp, float hum) {
    uint16_t t = std::lround((temp + 45) / 175 * 65535), h = std::lround(hum / 100 * 65535);
    Wire.frame[0] = t >> 8; Wire.frame[1] = t;
    Wire.frame[3] = h >> 8; Wire.frame[4] = h;
    Wire.frame[2] = sprint1::crc8(Wire.frame, 2);
    Wire.frame[5] = sprint1::crc8(Wire.frame + 3, 2);
}

int main() {
    for (auto& pin : mockPins) pin = HIGH;
    setSensor(4.5, 62.5);
    Wire.regs[0x0f] = 0x80; // New RTC with OSF set.
    Sprint1Hardware hw;
    hw.begin();
    assert(Wire.sda == 21 && Wire.scl == 22 && Wire.clock == 400000);
    for (int pin : {27, 32, 33, 25}) assert(mockInterrupts[pin] && mockModes[pin] == INPUT_PULLUP);
    assert(!hw.reading().rtcOk);
    Serial.inject("TIME 1735689600\n"); hw.update(); step(hw, 20);
    assert(hw.reading().rtcOk && hw.reading().epoch == 1735689600);
    assert(hw.reading().sensorOk && std::fabs(hw.reading().temp - 4.5) < 0.01);
    assert(hw.reading().alert == 0 && mockPins[PIN_RELAY] == LOW);
    assert(!hw.reading().batteryOk && hw.reading().battery == 0);
    std::puts("PASS actual driver with mocks: ESP32 pins, UTC/OSF initialization, validated I2C sample, no fake battery");

    Serial.output.clear();
    edge(PIN_BTN_WAKE, true); hw.update(); step(hw, 5);
    edge(PIN_BTN_WAKE, false); hw.update(); step(hw, 5);
    edge(PIN_BTN_WAKE, true); hw.update(); step(hw, 34);
    assert(Serial.output.find("BTN1") == std::string::npos);
    step(hw, 1); step(hw, 500);
    assert(Serial.output.find("BTN1 page=2") != std::string::npos);
    assert(Serial.output.find("BTN1", Serial.output.find("BTN1") + 1) == std::string::npos);
    assert(mockDisplay.find("PAGE 2") != std::string::npos);
    edge(PIN_BTN_WAKE, false); hw.update(); step(hw, 35);
    std::puts("PASS actual ISR -> stable debounce -> one OLED page change, including bounce and held button");

    hw.takeSampleRequest();
    click(hw, PIN_BTN_TEST);
    assert(hw.reading().temp == 9.5f && hw.reading().alert == 1 && mockPins[PIN_RELAY] == HIGH);
    assert(hw.takeSampleRequest());
    click(hw, PIN_BTN_ACK);
    assert(hw.reading().alert == 1 && mockPins[PIN_RELAY] == LOW);
    step(hw, 15000); step(hw, 20);
    assert(std::fabs(hw.reading().temp - 4.5f) < 0.01 && hw.reading().alert == 0);
    assert(hw.takeSampleRequest());
    std::puts("PASS actual HMI: 15-second test, immediate alarm/sample request, ACK preserves alert, restore real sensor");

    Wire.frame[5] ^= 1; step(hw, 2000); step(hw, 20);
    assert(!hw.reading().sensorOk && mockPins[PIN_RELAY] == HIGH);
    click(hw, PIN_BTN_ACK); assert(mockPins[PIN_RELAY] == LOW);
    setSensor(4.5, 62.5); Wire.shortCount = 3;
    step(hw, 2000); step(hw, 20); assert(!hw.reading().sensorOk);
    Wire.shortCount = 6; step(hw, 2000); step(hw, 20); assert(hw.reading().sensorOk);
    Wire.sensorPresent = false; step(hw, 2000); assert(!hw.reading().sensorOk);
    Wire.sensorPresent = true; step(hw, 2000); step(hw, 20); assert(hw.reading().sensorOk);
    std::puts("PASS actual driver: CRC error, short read, NACK, local fault/mute, automatic SHT30 recovery");

    Wire.rtcPresent = false; step(hw, 1100);
    assert(!hw.reading().rtcOk && hw.reading().epoch == 0);
    Wire.rtcPresent = true; Wire.regs[0x0f] |= 0x80; step(hw, 1100);
    assert(!hw.reading().rtcOk);
    Serial.inject("TIME 1735689700\n"); hw.update();
    assert(hw.reading().rtcOk && hw.reading().epoch == 1735689700);
    Serial.inject("TIME -1\nTIME 99999999999999\n"); hw.update();
    assert(hw.reading().epoch == 1735689700);
    mockInterrupts[PIN_RTC_SQW](); hw.update();
    Serial.output.clear(); step(hw, 3000);
    assert(Serial.output.find("SQW WARN") != std::string::npos);
    mockInterrupts[PIN_RTC_SQW](); hw.update();
    assert(Serial.output.find("edges recovered") != std::string::npos);
    std::puts("PASS actual RTC driver: unplug, OSF invalidation, manual UTC, rejected bad time, SQW warning/recovery");

    Wire.oledPresent = false; step(hw, 500); hw.printStatus();
    assert(Serial.output.find("OLED=MISSING") != std::string::npos);
    Wire.oledPresent = true; step(hw, 500); hw.printStatus();
    assert(Serial.output.find("OLED=OK") != std::string::npos);
    Serial.output.clear(); Serial.inject(std::string(80, 'A') + "\nSTATUS\n");
    hw.update(); hw.update(); hw.update();
    assert(Serial.output.find("Command too long") != std::string::npos);
    assert(Serial.output.find("[STATUS]") != std::string::npos);
    mockUtc = 1800000000; hw.update();
    assert(hw.reading().epoch == 1800000000);
    std::puts("PASS actual driver: OLED recovery, bounded Serial parser, one-shot NTP -> DS3231 synchronization");
}
