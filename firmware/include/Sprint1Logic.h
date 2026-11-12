#pragma once

#include <stdint.h>
#include <stddef.h>

namespace sprint1 {

inline uint8_t crc8(const uint8_t* bytes, size_t count) {
    uint8_t crc = 0xff;
    for (size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                               : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

inline bool decodeSht30(const uint8_t* data, size_t count, float& temp, float& hum) {
    if (count != 6 || crc8(data, 2) != data[2] || crc8(data + 3, 2) != data[5])
        return false;
    temp = -45.0f + 175.0f * ((data[0] << 8) | data[1]) / 65535.0f;
    hum = 100.0f * ((data[3] << 8) | data[4]) / 65535.0f;
    return true;
}

inline bool decodeBcd(uint8_t byte, unsigned& value) {
    if ((byte & 15) > 9 || (byte >> 4) > 9) return false;
    value = (byte >> 4) * 10 + (byte & 15);
    return true;
}

inline uint8_t bcd(unsigned value) { return ((value / 10) << 4) | (value % 10); }

inline unsigned daysInMonth(unsigned year, unsigned month) {
    constexpr unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    return days[month - 1] + (month == 2 && year % 4 == 0);
}

// DS3231 is stored in UTC; this node deliberately supports 2024..2099 only.
inline bool decodeRtc(const uint8_t* r, size_t size, uint32_t& epoch) {
    unsigned sec, min, hour, day, month, year;
    if (size != 7 || (r[0] & 0x80) || (r[1] & 0x80) || (r[2] & 0x80)
        || (r[4] & 0xc0) || (r[5] & 0xe0) || r[3] < 1 || r[3] > 7)
        return false;
    if (!decodeBcd(r[0], sec) || !decodeBcd(r[1], min)
        || !decodeBcd(r[2] & ((r[2] & 0x40) ? 0x1f : 0x3f), hour)
        || !decodeBcd(r[4], day) || !decodeBcd(r[5], month)
        || !decodeBcd(r[6], year)) return false;
    if (r[2] & 0x40) {
        if (hour < 1 || hour > 12) return false;
        hour = hour % 12 + ((r[2] & 0x20) ? 12 : 0);
    }
    year += 2000;
    if (year < 2024 || sec > 59 || min > 59 || hour > 23 || day < 1
        || day > daysInMonth(year, month)) return false;
    uint32_t days = 0;
    for (unsigned y = 2000; y < year; ++y) days += (y % 4 == 0) ? 366 : 365;
    for (unsigned m = 1; m < month; ++m) days += daysInMonth(year, m);
    days += day - 1;
    epoch = 946684800UL + days * 86400UL + hour * 3600UL + min * 60UL + sec;
    return true;
}

inline bool encodeRtc(uint32_t epoch, uint8_t r[7]) {
    if (epoch < 1704067200UL || epoch > 4102444799UL) return false;
    uint32_t days = (epoch - 946684800UL) / 86400UL;
    const unsigned weekday = (days + 6) % 7 + 1; // 2000-01-01 was Saturday.
    unsigned year = 2000, month = 1;
    while (days >= ((year % 4 == 0) ? 366U : 365U)) {
        days -= (year % 4 == 0) ? 366 : 365;
        ++year;
    }
    while (days >= daysInMonth(year, month)) days -= daysInMonth(year, month++);
    r[0] = bcd(epoch % 60);
    r[1] = bcd(epoch / 60 % 60);
    r[2] = bcd(epoch / 3600 % 24);
    r[3] = weekday;
    r[4] = bcd(days + 1);
    r[5] = bcd(month);
    r[6] = bcd(year - 2000);
    return true;
}

inline uint8_t alertFor(float temp) { return temp > 8.0f ? 1 : (temp < 2.0f ? 2 : 0); }

class DebouncedButton {
public:
    void edge(bool down, uint32_t at) { candidate_ = down; changedAt_ = at; pending_ = true; }
    bool update(uint32_t now, uint32_t debounceMs = 35) {
        if (!pending_ || static_cast<uint32_t>(now - changedAt_) < debounceMs) return false;
        pending_ = false;
        const bool pressed = candidate_ && !stable_;
        stable_ = candidate_;
        return pressed;
    }
private:
    bool candidate_ = false, stable_ = false, pending_ = false;
    uint32_t changedAt_ = 0;
};

} // namespace sprint1
