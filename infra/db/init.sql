-- =========================================================================
-- KHỞI TẠO CƠ SỞ DỮ LIỆU TIMESCALEDB CHO HỆ THỐNG GSP COLD CHAIN IOT
-- Đảm bảo tính bất biến (Append-Only / Immutable) và chống trùng lặp bản ghi
-- =========================================================================

-- 1. Kích hoạt phần mở rộng TimescaleDB
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- 2. Bảng danh mục thiết bị giám sát biên (ESP32-S3 Nodes)
CREATE TABLE IF NOT EXISTS devices (
    device_id VARCHAR(32) PRIMARY KEY,
    device_name VARCHAR(100) NOT NULL,
    storage_type VARCHAR(20) NOT NULL,
    temp_min_c NUMERIC(5,2) NOT NULL DEFAULT 2.00,
    temp_max_c NUMERIC(5,2) NOT NULL DEFAULT 8.00,
    ecdsa_pub_key TEXT NOT NULL,
    mqtt_token_hash VARCHAR(64) NOT NULL,
    fw_version VARCHAR(16) DEFAULT '1.0.0',
    is_online BOOLEAN DEFAULT FALSE,
    last_seen_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 3. Bảng lưu trữ dữ liệu đo vi khí hậu (Chuỗi thời gian bất biến)
CREATE TABLE IF NOT EXISTS telemetry_logs (
    log_id BIGSERIAL,
    device_id VARCHAR(32) NOT NULL REFERENCES devices(device_id),
    seq_no BIGINT NOT NULL,
    ts_rtc TIMESTAMPTZ NOT NULL,
    received_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    is_backfill BOOLEAN NOT NULL DEFAULT FALSE,
    temp_c NUMERIC(5,2) NOT NULL,
    hum_pct NUMERIC(5,2) NOT NULL,
    bat_v NUMERIC(4,2) NOT NULL,
    alert_code SMALLINT NOT NULL DEFAULT 0,
    sha256_hash CHAR(64) NOT NULL,
    ecdsa_sig VARCHAR(160) NOT NULL,
    sig_verified BOOLEAN NOT NULL,
    CONSTRAINT pk_telemetry PRIMARY KEY (device_id, ts_rtc)
);

-- Chuyển bảng telemetry_logs thành Hypertable phân mảnh theo nhãn thời gian gốc của RTC
SELECT create_hypertable('telemetry_logs', 'ts_rtc', if_not_exists => TRUE);

-- Tạo Index phụ trợ giúp truy vấn nhanh theo thiết bị và số thứ tự gói tin
CREATE INDEX IF NOT EXISTS idx_telemetry_device_seq ON telemetry_logs (device_id, seq_no DESC);

-- 4. Cơ chế khóa cứng tính bất biến (READONLY / Append-Only) theo yêu cầu FR-05
CREATE OR REPLACE FUNCTION prevent_telemetry_tampering()
RETURNS TRIGGER AS $$
BEGIN
    RAISE EXCEPTION 'VI PHAM PHAP LY GSP: Bang telemetry_logs la bat bien (Append-Only). Nghiem cam moi thao tac % tren du lieu goc!', TG_OP
        USING ERRCODE = '42501';
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_immutable_telemetry ON telemetry_logs;

CREATE TRIGGER trg_immutable_telemetry
BEFORE UPDATE OR DELETE ON telemetry_logs
FOR EACH ROW
EXECUTE FUNCTION prevent_telemetry_tampering();

-- 5. Bảng quản lý sự cố & cảnh báo vi phạm ngưỡng nhiệt độ GSP
CREATE TABLE IF NOT EXISTS alerts (
    alert_id BIGSERIAL PRIMARY KEY,
    device_id VARCHAR(32) NOT NULL REFERENCES devices(device_id),
    seq_no BIGINT,
    alert_code SMALLINT NOT NULL,
    severity VARCHAR(20) NOT NULL DEFAULT 'CRITICAL',
    temp_c NUMERIC(5,2) NOT NULL,
    hum_pct NUMERIC(5,2) NOT NULL,
    message TEXT NOT NULL,
    ts_rtc TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    is_resolved BOOLEAN NOT NULL DEFAULT FALSE,
    resolved_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_alerts_device_time ON alerts (device_id, ts_rtc DESC);

-- 6. Bảng nhật ký kiểm toán hệ thống (Audit Trail phục vụ thanh tra GSP)
CREATE TABLE IF NOT EXISTS audit_logs (
    audit_id BIGSERIAL PRIMARY KEY,
    event_type VARCHAR(50) NOT NULL,
    actor VARCHAR(64) NOT NULL DEFAULT 'SYSTEM',
    device_id VARCHAR(32),
    description TEXT NOT NULL,
    metadata JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at ON audit_logs (created_at DESC);

-- 7. Nạp sẵn thiết bị mẫu phục vụ chạy thông luồng End-to-End
INSERT INTO devices (
    device_id,
    device_name,
    storage_type,
    temp_min_c,
    temp_max_c,
    ecdsa_pub_key,
    mqtt_token_hash,
    fw_version,
    is_online
) VALUES (
    'GSP-TRUCK-01',
    'Xe tải Vaccine 29H-123.45',
    'MEDICAL_2_8C',
    2.00,
    8.00,
    'DUMMY_PUBLIC_KEY_SECP256R1',
    'dummy_token_hash',
    '1.0.0',
    FALSE
) ON CONFLICT (device_id) DO NOTHING;

-- Ghi nhận sự kiện khởi tạo hệ thống vào bảng kiểm toán
INSERT INTO audit_logs (event_type, actor, device_id, description)
VALUES (
    'SYSTEM_INIT',
    'DB_INIT_SCRIPT',
    'GSP-TRUCK-01',
    'Khoi tao thanh cong Hypertable telemetry_logs, Trigger bat bien GSP va thiet bi mau GSP-TRUCK-01.'
);