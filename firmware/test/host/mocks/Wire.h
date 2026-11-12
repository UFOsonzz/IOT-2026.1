#pragma once
#include <stdint.h>
#include <vector>
#include <deque>
#include <algorithm>

class MockWire {
public:
    bool sensorPresent = true, rtcPresent = true, oledPresent = true;
    uint8_t regs[32] = {}, frame[6] = {}, shortCount = 6;
    int sda = -1, scl = -1;
    uint32_t clock = 0;
    uint8_t address = 0, reg = 0;
    std::vector<uint8_t> tx;
    std::deque<uint8_t> rx;
    void begin(int data, int clk) { sda = data; scl = clk; }
    void setTimeOut(int) {}
    void setClock(uint32_t hz) { clock = hz; }
    void beginTransmission(uint8_t addr) { address = addr; tx.clear(); }
    void write(uint8_t value) { tx.push_back(value); }
    void write(const uint8_t* bytes, size_t count) { tx.insert(tx.end(), bytes, bytes + count); }
    uint8_t endTransmission(bool = true) {
        if (!((address == 0x44 && sensorPresent) || (address == 0x68 && rtcPresent)
            || (address == 0x3c && oledPresent))) return 2;
        if (address == 0x68 && !tx.empty()) {
            reg = tx[0];
            for (size_t i = 1; i < tx.size(); ++i) regs[(reg + i - 1) % 32] = tx[i];
        }
        return 0;
    }
    uint8_t requestFrom(uint8_t addr, uint8_t count) {
        rx.clear();
        if (addr == 0x68 && rtcPresent) {
            for (uint8_t i = 0; i < count; ++i) rx.push_back(regs[(reg + i) % 32]);
        } else if (addr == 0x44 && sensorPresent) {
            for (uint8_t i = 0; i < std::min(shortCount, count); ++i) rx.push_back(frame[i]);
        }
        return rx.size();
    }
    uint8_t read() { auto value = rx.front(); rx.pop_front(); return value; }
};
extern MockWire Wire;
