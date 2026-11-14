#pragma once

#include <stdint.h>

// GPIO Pinout
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22
#define PIN_RTC_SQW         27
#define PIN_BAT_ADC         34
#define PIN_RELAY           26
#define PIN_BTN_WAKE        32
#define PIN_BTN_ACK         33
#define PIN_BTN_TEST        25
#define PIN_STATUS_LED      2
#define PIN_WIFI_RESET_BTN  0

#ifndef GSP_BATTERY_CONNECTED
#define GSP_BATTERY_CONNECTED 0
#endif

// Device & Network Identity
#define DEVICE_ID           "GSP-TRUCK-01"
#define DEFAULT_AP_CLIENT   "192.168.4.2"

constexpr uint16_t MQTT_PORT              = 1883;
constexpr uint16_t MQTT_KEEPALIVE_SEC     = 30;
constexpr uint16_t MQTT_BUFFER_SIZE       = 1024;
constexpr uint16_t UDP_DISCOVERY_PORT     = 8888;
constexpr uint32_t WIFI_RESET_HOLD_MS     = 5000;
constexpr uint32_t MQTT_RECONNECT_MS      = 3000;
constexpr uint8_t  TELEMETRY_QUEUE_LEN    = 32;

// I2C & Peripheral Timing
constexpr uint8_t  SHT30_ADDRESS          = 0x44;
constexpr uint8_t  DS3231_ADDRESS         = 0x68;
constexpr uint8_t  OLED_ADDRESS           = 0x3c;
constexpr uint32_t I2C_CLOCK_HZ           = 400000;
constexpr uint32_t SENSOR_INTERVAL_MS     = 2000;
constexpr uint32_t SAMPLE_INTERVAL_MS     = 10000;
constexpr uint32_t SENSOR_MAX_AGE_MS      = 5000;
constexpr uint32_t BUTTON_DEBOUNCE_MS     = 35;
constexpr uint32_t GSP_TEST_DURATION_MS   = 15000;
constexpr uint32_t STATUS_LOG_INTERVAL_MS = 10000;