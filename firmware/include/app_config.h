#pragma once

#include <stdint.h>

// GPIO Pinout & Hardware Target Configuration
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(BOARD_ESP32_S3)
// ============================================================================
// ESP32-S3 Pinout (Tránh vùng Octal Flash/PSRAM GPIO 26..32 và GPIO 34 không tồn tại)
// ============================================================================
#define BOARD_TARGET_NAME   "ESP32-S3"
#define PIN_I2C_SDA         8   // I2C SDA chuẩn trên ESP32-S3
#define PIN_I2C_SCL         9   // I2C SCL chuẩn trên ESP32-S3
#define PIN_RTC_SQW         6   // Xung SQW 1Hz ngắt từ DS3231
#define PIN_BAT_ADC         1   // ADC1_CH0 (ESP32-S3 không có GPIO 34)
#define PIN_RELAY           7   // Relay cảnh báo chuông/đèn
#define PIN_BTN_WAKE        4   // Nút WAKE / Chuyển trang OLED
#define PIN_BTN_ACK         5   // Nút ACK / Mute cảnh báo
#define PIN_BTN_TEST        15  // Nút TEST giả lập 9.5°C
#define PIN_STATUS_LED      2   // LED trạng thái hệ thống
#define PIN_WIFI_RESET_BTN  0   // Nút BOOT giữ 5s reset cấu hình Wi-Fi
#else
// ============================================================================
// ESP32 Classic Pinout (ESP32-WROOM-32 / DevKit v1)
// ============================================================================
#define BOARD_TARGET_NAME   "ESP32-Classic"
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
#endif

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

// ============================================================================
// CHỌN LOẠI CẢM BIẾN NHIỆT ẨM
// ============================================================================
#if !defined(SENSOR_TYPE_SHT30) && !defined(SENSOR_TYPE_SHT40)
#define SENSOR_TYPE_SHT40   // <-- Mặc định là SHT40. Đổi thành SENSOR_TYPE_SHT30 nếu dùng SHT30
#endif

#if defined(SENSOR_TYPE_SHT40)
#define SENSOR_NAME                 "SHT40"
#define SENSOR_I2C_ADDR             0x44
#define SENSOR_CONVERSION_DELAY_MS  10      // SHT40 đo High-precision chỉ mất 8.2ms
#define SENSOR_CMD_BYTE_1           0xFD    // 0xFD: Lệnh đo High-precision của SHT40
#define SENSOR_CMD_LEN              1
#define SENSOR_DECODE_FUNC          gsp::decodeSht40
#else
#define SENSOR_NAME                 "SHT30"
#define SENSOR_I2C_ADDR             0x44
#define SENSOR_CONVERSION_DELAY_MS  20      // SHT30 đo mất ~15ms
#define SENSOR_CMD_BYTE_1           0x24    // 0x2400: Lệnh đo High-precision của SHT30
#define SENSOR_CMD_BYTE_2           0x00
#define SENSOR_CMD_LEN              2
#define SENSOR_DECODE_FUNC          gsp::decodeSht30
#endif

// I2C & Peripheral Timing
constexpr uint8_t  SHT30_ADDRESS          = SENSOR_I2C_ADDR;
constexpr uint8_t  DS3231_ADDRESS         = 0x68;
constexpr uint8_t  OLED_ADDRESS           = 0x3c;
constexpr uint32_t I2C_CLOCK_HZ           = 400000;
constexpr uint32_t SENSOR_INTERVAL_MS     = 2000;
constexpr uint32_t SAMPLE_INTERVAL_MS     = 10000;
constexpr uint32_t SENSOR_MAX_AGE_MS      = 5000;
constexpr uint32_t BUTTON_DEBOUNCE_MS     = 35;
constexpr uint32_t GSP_TEST_DURATION_MS   = 15000;
constexpr uint32_t STATUS_LOG_INTERVAL_MS = 10000;