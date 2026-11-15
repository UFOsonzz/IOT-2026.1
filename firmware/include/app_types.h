#pragma once

#include <stdint.h>

struct SensorReading {
    float temp = 0.0f;
    float hum = 0.0f;
    float battery = 0.0f;
    uint32_t epoch = 0;
    uint8_t alert = 0;
    bool sensorOk = false;
    bool rtcOk = false;
    bool batteryOk = false;
};

struct TelemetryPacket {
    char device_id[32];
    uint32_t seq_no;
    uint32_t ts_rtc;
    float temp_c;
    float hum_pct;
    float bat_v;
    uint8_t alert_code;
    char sha256_hash[65];
    char ecdsa_sig[64];
    bool is_backfill;
};