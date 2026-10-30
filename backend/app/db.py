import os
import logging
from datetime import datetime, timezone
from typing import Any, Optional

import psycopg2
from psycopg2.extras import RealDictCursor

logger = logging.getLogger("gsp.db")

DATABASE_URL = os.getenv(
    "DATABASE_URL",
    "postgresql://gsp_user:gsp_pass_2026@timescaledb:5432/gsp_coldchain",
)


def get_connection():
    """Khởi tạo kết nối tới PostgreSQL / TimescaleDB với RealDictCursor."""
    return psycopg2.connect(DATABASE_URL, cursor_factory=RealDictCursor)


def _parse_timestamp(ts_val: int | float | str | datetime) -> datetime:
    """Chuyển đổi Unix timestamp (giây) hoặc chuỗi ISO-8601 sang đối tượng datetime chuẩn UTC."""
    if isinstance(ts_val, datetime):
        return ts_val.astimezone(timezone.utc)
    if isinstance(ts_val, (int, float)):
        return datetime.fromtimestamp(float(ts_val), tz=timezone.utc)
    if isinstance(ts_val, str):
        ts_str = ts_val.strip()
        if ts_str.isdigit():
            return datetime.fromtimestamp(float(ts_str), tz=timezone.utc)
        try:
            return datetime.fromtimestamp(float(ts_str), tz=timezone.utc)
        except ValueError:
            if ts_str.endswith("Z"):
                ts_str = ts_str[:-1] + "+00:00"
            dt = datetime.fromisoformat(ts_str)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)
            return dt.astimezone(timezone.utc)
    raise ValueError(f"Định dạng thời gian không hợp lệ: {ts_val}")


def insert_telemetry(record: dict[str, Any]) -> bool:
    """
    Chèn bản ghi đo vi khí hậu vào Hypertable telemetry_logs (chỉ ghi thêm - Append-Only).
    Chống trùng lặp bản ghi đẩy bù (Auto-Sync) bằng ON CONFLICT (device_id, ts_rtc) DO NOTHING.
    Đồng thời cập nhật trạng thái trực tuyến và thời điểm xuất hiện gần nhất của thiết bị.
    Trả về True nếu bản ghi mới được chèn, False nếu bản ghi bị trùng lặp.
    """
    ts_utc = _parse_timestamp(record["ts_rtc"])
    device_id = str(record["device_id"])

    insert_sql = """
        INSERT INTO telemetry_logs (
            device_id,
            seq_no,
            ts_rtc,
            is_backfill,
            temp_c,
            hum_pct,
            bat_v,
            alert_code,
            sha256_hash,
            ecdsa_sig,
            sig_verified
        ) VALUES (
            %s, %s, %s, %s,
            %s, %s, %s, %s,
            %s, %s, %s
        )
        ON CONFLICT (device_id, ts_rtc) DO NOTHING
        RETURNING log_id;
    """

    update_device_sql = """
        UPDATE devices
        SET last_seen_at = NOW(),
            is_online = TRUE
        WHERE device_id = %s;
    """

    conn = None
    try:
        conn = get_connection()
        with conn:
            with conn.cursor() as cur:
                cur.execute(update_device_sql, (device_id,))
                cur.execute(
                    insert_sql,
                    (
                        device_id,
                        int(record["seq_no"]),
                        ts_utc,
                        bool(record.get("is_backfill", False)),
                        round(float(record["temp_c"]), 2),
                        round(float(record["hum_pct"]), 2),
                        round(float(record["bat_v"]), 2),
                        int(record.get("alert_code", 0)),
                        str(record["sha256_hash"]),
                        str(record["ecdsa_sig"]),
                        bool(record["sig_verified"]),
                    ),
                )
                inserted_row = cur.fetchone()
                return inserted_row is not None
    except Exception as exc:
        logger.error("Lỗi khi chèn bản ghi telemetry cho thiết bị %s: %s", device_id, exc)
        raise
    finally:
        if conn is not None:
            conn.close()


def insert_alert(alert_record: dict[str, Any]) -> Optional[int]:
    """Lưu bản ghi cảnh báo khẩn cấp khi thiết bị phát hiện vượt ngưỡng GSP."""
    ts_utc = _parse_timestamp(alert_record["ts_rtc"])
    sql = """
        INSERT INTO alerts (
            device_id,
            seq_no,
            alert_code,
            severity,
            temp_c,
            hum_pct,
            message,
            ts_rtc
        ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
        RETURNING alert_id;
    """
    conn = None
    try:
        conn = get_connection()
        with conn:
            with conn.cursor() as cur:
                cur.execute(
                    sql,
                    (
                        str(alert_record["device_id"]),
                        int(alert_record.get("seq_no", 0)),
                        int(alert_record.get("alert_code", 1)),
                        str(alert_record.get("severity", "CRITICAL")),
                        round(float(alert_record.get("temp_c", 0.0)), 2),
                        round(float(alert_record.get("hum_pct", 0.0)), 2),
                        str(alert_record.get("message", "Vi phạm ngưỡng nhiệt độ bảo quản GSP")),
                        ts_utc,
                    ),
                )
                row = cur.fetchone()
                return int(row["alert_id"]) if row else None
    except Exception as exc:
        logger.error("Lỗi khi lưu bản ghi cảnh báo: %s", exc)
        return None
    finally:
        if conn is not None:
            conn.close()


def update_device_status(device_id: str, is_online: bool) -> None:
    """
    Cập nhật trạng thái kết nối của thiết bị khi nhận bản tin status {"state": "ONLINE"}
    hoặc bản tin Di chúc LWT {"state": "OFFLINE"}.
    """
    sql = """
        UPDATE devices
        SET is_online = %s,
            last_seen_at = CASE WHEN %s = TRUE THEN NOW() ELSE last_seen_at END
        WHERE device_id = %s;
    """
    conn = None
    try:
        conn = get_connection()
        with conn:
            with conn.cursor() as cur:
                cur.execute(sql, (is_online, is_online, device_id))
    except Exception as exc:
        logger.error("Lỗi khi cập nhật trạng thái thiết bị %s: %s", device_id, exc)
    finally:
        if conn is not None:
            conn.close()


def get_devices() -> list[dict[str, Any]]:
    """
    Lấy danh sách toàn bộ thiết bị từ bảng devices kèm theo thông số đo mới nhất
    (nhiệt độ, độ ẩm, điện áp pin, mã cảnh báo, trạng thái chữ ký số) bằng LEFT JOIN LATERAL.
    """
    sql = """
        SELECT
            d.device_id,
            d.device_name,
            d.storage_type,
            d.temp_min_c::FLOAT8 AS temp_min_c,
            d.temp_max_c::FLOAT8 AS temp_max_c,
            d.fw_version,
            d.is_online,
            d.last_seen_at,
            d.created_at,
            t.seq_no AS latest_seq_no,
            t.ts_rtc AS latest_ts_rtc,
            t.temp_c::FLOAT8 AS temp_c,
            t.hum_pct::FLOAT8 AS hum_pct,
            t.bat_v::FLOAT8 AS bat_v,
            t.alert_code,
            t.is_backfill,
            t.sig_verified
        FROM devices d
        LEFT JOIN LATERAL (
            SELECT
                seq_no,
                ts_rtc,
                temp_c,
                hum_pct,
                bat_v,
                alert_code,
                is_backfill,
                sig_verified
            FROM telemetry_logs tl
            WHERE tl.device_id = d.device_id
            ORDER BY tl.ts_rtc DESC
            LIMIT 1
        ) t ON TRUE
        ORDER BY d.device_id ASC;
    """
    conn = None
    try:
        conn = get_connection()
        with conn.cursor() as cur:
            cur.execute(sql)
            rows = cur.fetchall()
            return [dict(r) for r in rows]
    finally:
        if conn is not None:
            conn.close()


def get_device_telemetry(
    device_id: str,
    limit: int = 50,
    cursor_ts: Optional[str] = None,
) -> list[dict[str, Any]]:
    """
    Lấy lịch sử bản ghi đo của thiết bị có phân trang theo con trỏ thời gian (ts_rtc < cursor_ts),
    sắp xếp từ mới nhất về cũ nhất (ts_rtc DESC).
    """
    safe_limit = max(1, min(int(limit), 500))
    conn = None
    try:
        conn = get_connection()
        with conn.cursor() as cur:
            if cursor_ts:
                cursor_dt = _parse_timestamp(cursor_ts)
                sql = """
                    SELECT
                        log_id,
                        device_id,
                        seq_no,
                        ts_rtc,
                        EXTRACT(EPOCH FROM ts_rtc)::BIGINT AS ts_epoch,
                        received_at,
                        is_backfill,
                        temp_c::FLOAT8 AS temp_c,
                        hum_pct::FLOAT8 AS hum_pct,
                        bat_v::FLOAT8 AS bat_v,
                        alert_code,
                        sha256_hash,
                        ecdsa_sig,
                        sig_verified
                    FROM telemetry_logs
                    WHERE device_id = %s
                      AND ts_rtc < %s
                    ORDER BY ts_rtc DESC
                    LIMIT %s;
                """
                cur.execute(sql, (device_id, cursor_dt, safe_limit))
            else:
                sql = """
                    SELECT
                        log_id,
                        device_id,
                        seq_no,
                        ts_rtc,
                        EXTRACT(EPOCH FROM ts_rtc)::BIGINT AS ts_epoch,
                        received_at,
                        is_backfill,
                        temp_c::FLOAT8 AS temp_c,
                        hum_pct::FLOAT8 AS hum_pct,
                        bat_v::FLOAT8 AS bat_v,
                        alert_code,
                        sha256_hash,
                        ecdsa_sig,
                        sig_verified
                    FROM telemetry_logs
                    WHERE device_id = %s
                    ORDER BY ts_rtc DESC
                    LIMIT %s;
                """
                cur.execute(sql, (device_id, safe_limit))

            rows = cur.fetchall()
            return [dict(r) for r in rows]
    finally:
        if conn is not None:
            conn.close()


def verify_device_chain(device_id: str) -> dict[str, Any]:
    """
    Thống kê và kiểm tra tính toàn vẹn chuỗi bản ghi của thiết bị:
    Tổng số bản ghi hợp lệ (sig_verified = TRUE) và bị giả mạo/sai lệch (sig_verified = FALSE).
    """
    sql = """
        SELECT
            COUNT(*)::BIGINT AS total_records,
            COUNT(*) FILTER (WHERE sig_verified = TRUE)::BIGINT AS valid_records,
            COUNT(*) FILTER (WHERE sig_verified = FALSE)::BIGINT AS tampered_records,
            COUNT(*) FILTER (WHERE is_backfill = TRUE)::BIGINT AS backfill_records,
            MIN(ts_rtc) AS first_record_at,
            MAX(ts_rtc) AS last_record_at
        FROM telemetry_logs
        WHERE device_id = %s;
    """
    conn = None
    try:
        conn = get_connection()
        with conn.cursor() as cur:
            cur.execute(sql, (device_id,))
            stats = dict(cur.fetchone() or {})
            tampered_count = int(stats.get("tampered_records", 0))
            total_count = int(stats.get("total_records", 0))
            return {
                "device_id": device_id,
                "total_records": total_count,
                "valid_records": int(stats.get("valid_records", 0)),
                "tampered_records": tampered_count,
                "backfill_records": int(stats.get("backfill_records", 0)),
                "chain_intact": (tampered_count == 0),
                "gsp_compliance_status": "VERIFIED" if tampered_count == 0 else "COMPROMISED",
                "first_record_at": stats.get("first_record_at"),
                "last_record_at": stats.get("last_record_at"),
            }
    finally:
        if conn is not None:
            conn.close()