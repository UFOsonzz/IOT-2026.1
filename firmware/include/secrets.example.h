#ifndef GSP_SECRETS_H
#define GSP_SECRETS_H

// ============================================================================
// CẤU HÌNH MẠNG WI-FI & MQTT BROKER (BẢN MẪU)
// Hướng dẫn: Sao chép file này thành "firmware/include/secrets.h" và điền
// thông tin thực tế trước khi biên dịch nạp xuống mạch ESP32-S3.
// ============================================================================

#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"

// Địa chỉ IP máy chủ chạy Docker Mosquitto Broker trong mạng LAN
#define MQTT_HOST           "192.168.1.100"
#define MQTT_PORT           1883

// Định danh thiết bị chuẩn GSP (Khớp với bảng devices trong TimescaleDB)
#define DEVICE_ID           "GSP-TRUCK-01"

#endif // GSP_SECRETS_H