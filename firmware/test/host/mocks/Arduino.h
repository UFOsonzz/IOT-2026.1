#pragma once
#include <stdint.h>
#include <stddef.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

#define IRAM_ATTR
#define F(x) x
#define LOW 0
#define HIGH 1
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 3
#define FALLING 4
#define ADC_11db 3
#define HEX 16
#define portMUX_INITIALIZER_UNLOCKED 0
using portMUX_TYPE = int;
inline void portENTER_CRITICAL(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL(portMUX_TYPE*) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE*) {}

extern uint32_t mockMillis;
extern time_t mockUtc;
extern int mockPins[40], mockModes[40];
extern void (*mockInterrupts[40])();
inline uint32_t millis() { return mockMillis; }
inline void pinMode(int pin, int mode) { mockModes[pin] = mode; }
inline int digitalRead(int pin) { return mockPins[pin]; }
inline void digitalWrite(int pin, int value) { mockPins[pin] = value; }
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int pin, void (*callback)(), int) { mockInterrupts[pin] = callback; }
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline uint32_t analogReadMilliVolts(int) { return 2000; }
inline time_t fakeTime(time_t*) { return mockUtc; }
#define time fakeTime

class MockSerial {
public:
    std::string output, input;
    size_t position = 0;
    void print(const char* text) { output += text; }
    void print(char* text) { output += text; }
    void print(char value) { output += value; }
    template<class T> void print(T value) {
        if constexpr (std::is_integral_v<T>) output += std::to_string(+value);
        else output += std::to_string(value);
    }
    template<class T> void print(T value, int format) {
        std::ostringstream s;
        if constexpr (std::is_floating_point_v<T>) s << std::fixed << std::setprecision(format) << value;
        else if (format == HEX) s << std::hex << +value;
        else s << +value;
        output += s.str();
    }
    void println() { output += '\n'; }
    template<class T> void println(T value) { print(value); println(); }
    int available() { return input.size() - position; }
    char read() { return input[position++]; }
    void inject(const std::string& text) { input = text; position = 0; }
};
extern MockSerial Serial;
