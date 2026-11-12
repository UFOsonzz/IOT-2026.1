#pragma once
#include "Arduino.h"
#define U8G2_R0 0
#define U8X8_PIN_NONE 255
inline const uint8_t u8g2_font_5x7_tf[] = {0};
extern std::string mockDisplay;
class U8G2_SSD1306_128X64_NONAME_1_HW_I2C : public MockSerial {
public:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C(int, int, int, int) {}
    void setI2CAddress(int) {}
    void setBusClock(int) {}
    void begin() {}
    void firstPage() { output.clear(); }
    bool nextPage() { mockDisplay = output; return false; }
    void setFont(const uint8_t*) {}
    void setCursor(int, int) { output += '\n'; }
};
