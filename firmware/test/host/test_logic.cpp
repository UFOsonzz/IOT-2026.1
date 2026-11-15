#include "gsp_codec.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

int main() {
    using namespace gsp;
    const uint8_t vector[] = {0xbe, 0xef};
    assert(crc8(vector, 2) == 0x92); // Sensirion datasheet test vector.
    float temp = 0, hum = 0;
    uint8_t frame[] = {0x66, 0x66, 0, 0x80, 0x00, 0};
    frame[2] = crc8(frame, 2); frame[5] = crc8(frame + 3, 2);
    assert(decodeSht30(frame, 6, temp, hum));
    assert(std::fabs(temp - 25) < 0.001 && std::fabs(hum - 50.00076) < 0.001);
    assert(decodeSht40(frame, 6, temp, hum));
    assert(std::fabs(temp - 25) < 0.001 && std::fabs(hum - 56.50095) < 0.001);
    assert(!decodeSht30(frame, 5, temp, hum));
    frame[2] ^= 1; assert(!decodeSht30(frame, 6, temp, hum)); frame[2] ^= 1;
    frame[5] ^= 1; assert(!decodeSht30(frame, 6, temp, hum));
    std::puts("PASS SHT30/SHT40 datasheet CRC, conversion, short read, temperature/humidity corruption");

    uint8_t rtc[] = {0x59, 0x59, 0x23, 5, 0x29, 0x02, 0x24};
    uint32_t epoch;
    assert(decodeRtc(rtc, 7, epoch) && epoch == 1709251199UL);
    uint8_t encoded[7]; assert(encodeRtc(epoch, encoded));
    assert(std::memcmp(encoded, rtc, 7) == 0);
    rtc[0] = 0; rtc[1] = 0;
    rtc[2] = 0x72; assert(decodeRtc(rtc, 7, epoch) && epoch == 1709208000UL); // 12 PM.
    rtc[2] = 0x52; assert(decodeRtc(rtc, 7, epoch) && epoch == 1709164800UL); // 12 AM.
    rtc[2] = 0x23; rtc[6] = 0x25; assert(!decodeRtc(rtc, 7, epoch)); // Non-leap Feb 29.
    rtc[6] = 0x24; rtc[0] = 0x6a; assert(!decodeRtc(rtc, 7, epoch));
    rtc[0] = 0x59; rtc[5] = 0x82; assert(!decodeRtc(rtc, 7, epoch)); // Century not supported.
    assert(!decodeRtc(rtc, 6, epoch));
    assert(!encodeRtc(1704067199UL, encoded));
    assert(!encodeRtc(4102444800UL, encoded));
    assert(encodeRtc(4102444799UL, encoded));
    assert(decodeRtc(encoded, 7, epoch) && epoch == 4102444799UL);
    // Check every day over the complete supported range, including all leap days.
    for (uint32_t e = 1704067200UL; e < 4102400000UL; e += 86400UL) {
        assert(encodeRtc(e, encoded));
        assert(decodeRtc(encoded, 7, epoch) && epoch == e);
    }
    std::puts("PASS RTC UTC known epoch, leap day, 12h AM/PM, invalid BCD, century, 2024..2099 daily roundtrip");

    DebouncedButton button;
    button.edge(true, 100); assert(!button.update(134)); assert(button.update(135));
    assert(!button.update(1000)); // Held button never repeats.
    button.edge(false, 1001); button.edge(true, 1008); // Release bounce.
    assert(!button.update(1043));
    button.edge(false, 1100); assert(!button.update(1135));
    button.edge(true, 1200); button.edge(false, 1205); button.edge(true, 1210);
    assert(!button.update(1244)); assert(button.update(1245));
    DebouncedButton wrap;
    wrap.edge(true, 0xfffffff0UL); assert(!wrap.update(5)); assert(wrap.update(19));
    std::puts("PASS button bounce, stable release, single press during hold, millis rollover");
    assert(alertFor(2) == 0 && alertFor(8) == 0);
    assert(alertFor(1.99) == 2 && alertFor(8.01) == 1 && alertFor(9.5) == 1);
    std::puts("PASS GSP boundaries and 9.50C test");
}