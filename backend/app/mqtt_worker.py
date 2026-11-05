import asyncio
import hashlib
import json
import logging
import os
from datetime import datetime, timezone
from typing import Any

import paho.mqtt.client as mqtt

from app.db import insert_alert, insert_telemetry, update_device_status

logger = logging.getLogger("gsp.mqtt_worker")

MQTT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "mosquitto")
MQTT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "1883"))
MQTT_CLIENT_ID = "gsp_backend_worker"

SUBSCRIBED_TOPICS = [
    ("gsp/v1/+/telemetry", 1),
    ("gsp/v1/+/sync", 1),
    ("gsp/v1/+/alert", 1),
    ("gsp/v1/+/status", 1),
]


def build_canonical_string(
    device_id: str,
    seq_no: int,
    ts_rtc: int,
    temp_c: float,
    hum_pct: float,
    alert_code: int,
) -> str:
    """
    Tái tạo chuỗi chuẩn hóa (Canonical String) đúng theo Data Contract:
    "<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>"
    Ví dụ: "GSP-TRUCK-01|1042|1761526800|4.25|62.50|0"
    """
    return f"{device_id}|{int(seq_no)}|{int(ts_rtc)}|{float(temp_c):.2f}|{float(hum_pct):.2f}|{int(alert_code)}"


def compute_sha256_hex(canonical_str: str) -> str:
    """Tính mã băm SHA-256 (64 ký tự hex chữ thường) của chuỗi chuẩn hóa."""
    return hashlib.sha256(canonical_str.encode("utf-8")).hexdigest()


def verify_dummy_signature(payload: dict[str, Any], computed_sha256: str) -> bool:
    """
    Kiểm tra tính toàn vẹn SHA-256 và chữ ký số ECDSA ở bản chạy thông (Walking Skeleton):
    sig_verified = (computed_sha256 == payload['sha256_hash']) and
                   (payload['ecdsa_sig'].startswith("DUMMY_SIG_OK") or len(payload['ecdsa_sig']) >= 64)
    """
    received_hash = str(payload.get("sha256_hash", "")).strip().lower()
    received_sig = str(payload.get("ecdsa_sig", "")).strip()

    hash_matches = computed_sha256.lower() == received_hash
    sig_matches = received_sig.startswith("DUMMY_SIG_OK") or len(received_sig) >= 64
    return bool(hash_matches and sig_matches)


class GSPMqttWorker:
    """
    Worker lắng nghe MQTT Broker với Persistent Session (clean_session=False, QoS=1).
    Xác thực chữ ký số, lưu vào TimescaleDB và đẩy sự kiện qua asyncio.Queue xuống WebSocket.
    """

    def __init__(self, event_queue: asyncio.Queue, loop: asyncio.AbstractEventLoop):
        self.event_queue = event_queue
        self.loop = loop

        # Tương thích cả paho-mqtt 1.x và 2.x
        if hasattr(mqtt, "CallbackAPIVersion"):
            self.client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION1,
                client_id=MQTT_CLIENT_ID,
                clean_session=False,
            )
        else:
            self.client = mqtt.Client(
                client_id=MQTT_CLIENT_ID,
                clean_session=False,
            )

        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect

    def start(self) -> None:
        """Kết nối tới Mosquitto Broker và khởi chạy vòng lặp mạng trên luồng nền."""
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)
        logger.info(
            "Đang kết nối MQTT Broker tại %s:%d (client_id=%s, clean_session=False)...",
            MQTT_BROKER_HOST,
            MQTT_BROKER_PORT,
            MQTT_CLIENT_ID,
        )
        self.client.connect_async(MQTT_BROKER_HOST, MQTT_BROKER_PORT, keepalive=60)
        self.client.loop_start()

    def stop(self) -> None:
        """Dừng vòng lặp mạng và ngắt kết nối MQTT an toàn."""
        logger.info("Đang dừng GSPMqttWorker...")
        self.client.loop_stop()
        self.client.disconnect()

    def _enqueue_ws_event(self, event_data: dict[str, Any]) -> None:
        """Đẩy sự kiện JSON sang hàng đợi asyncio một cách an toàn giữa các luồng (Thread-safe)."""
        if not self.loop.is_closed():
            self.loop.call_soon_threadsafe(self.event_queue.put_nowait, event_data)

    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: dict, rc: int) -> None:
        if rc == 0:
            logger.info("Kết nối MQTT Broker thành công! Đang đăng ký các kênh GSP QoS 1...")
            client.subscribe(SUBSCRIBED_TOPICS)
        else:
            logger.error("Kết nối MQTT thất bại với mã lỗi rc=%d", rc)

    def _on_disconnect(self, client: mqtt.Client, userdata: Any, rc: int) -> None:
        if rc != 0:
            logger.warning("Mất kết nối đột ngột tới MQTT Broker (rc=%d). Đang tự động kết nối lại...", rc)

    def _on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        topic = msg.topic
        parts = topic.split("/")
        if len(parts) != 4 or parts[0] != "gsp" or parts[1] != "v1":
            logger.warning("Bỏ qua topic không hợp lệ: %s", topic)
            return

        topic_device_id = parts[2]
        channel = parts[3]

        try:
            payload_str = msg.payload.decode("utf-8")
            payload = json.loads(payload_str)
        except Exception as exc:
            logger.error("Payload JSON không hợp lệ trên topic %s: %s", topic, exc)
            return

        try:
            if channel in ("telemetry", "sync"):
                self._handle_telemetry_or_sync(topic_device_id, channel, payload)
            elif channel == "status":
                self._handle_status(topic_device_id, payload)
            elif channel == "alert":
                self._handle_alert(topic_device_id, payload)
        except Exception as exc:
            logger.error("Lỗi xử lý bản tin trên kênh %s (thiết bị %s): %s", channel, topic_device_id, exc)

    def _handle_telemetry_or_sync(
        self,
        topic_device_id: str,
        channel: str,
        payload: dict[str, Any],
    ) -> None:
        device_id = str(payload.get("device_id", topic_device_id))
        seq_no = int(payload["seq_no"])
        ts_rtc = int(payload["ts_rtc"])
        temp_c = round(float(payload["temp_c"]), 2)
        hum_pct = round(float(payload["hum_pct"]), 2)
        bat_v = round(float(payload["bat_v"]), 2)
        alert_code = int(payload.get("alert_code", 0))
        sha256_hash = str(payload.get("sha256_hash", ""))
        ecdsa_sig = str(payload.get("ecdsa_sig", ""))

        # 1. Tái tạo chuỗi chuẩn hóa (Canonical String) và băm SHA-256
        canonical_str = build_canonical_string(
            device_id=device_id,
            seq_no=seq_no,
            ts_rtc=ts_rtc,
            temp_c=temp_c,
            hum_pct=hum_pct,
            alert_code=alert_code,
        )
        computed_sha256 = compute_sha256_hex(canonical_str)

        # 2. Xác thực chữ ký Dummy theo đúng hợp đồng giao tiếp
        sig_verified = verify_dummy_signature(payload, computed_sha256)
        is_backfill = channel == "sync"

        record = {
            "device_id": device_id,
            "seq_no": seq_no,
            "ts_rtc": ts_rtc,
            "is_backfill": is_backfill,
            "temp_c": temp_c,
            "hum_pct": hum_pct,
            "bat_v": bat_v,
            "alert_code": alert_code,
            "sha256_hash": sha256_hash,
            "ecdsa_sig": ecdsa_sig,
            "sig_verified": sig_verified,
        }

        # 3. Lưu vào Hypertable TimescaleDB (chống trùng lặp bằng ON CONFLICT DO NOTHING)
        inserted = insert_telemetry(record)

        if not sig_verified:
            logger.warning(
                "CẢNH BÁO GIẢ MẠO DỮ LIỆU! Thiết bị=%s, seq_no=%d, hash_nhận=%s, hash_tính=%s",
                device_id,
                seq_no,
                sha256_hash,
                computed_sha256,
            )

        # 4. Đẩy sự kiện xuống hàng đợi asyncio.Queue để phát qua WebSocket
        ws_event = {
            "event": "sync" if is_backfill else "telemetry",
            "inserted": inserted,
            "data": {
                **record,
                "ts_iso": datetime.fromtimestamp(ts_rtc, tz=timezone.utc).isoformat(),
                "computed_sha256": computed_sha256,
            },
        }
        self._enqueue_ws_event(ws_event)

    def _handle_status(self, topic_device_id: str, payload: dict[str, Any]) -> None:
        state_str = str(payload.get("state", "OFFLINE")).upper()
        is_online = state_str == "ONLINE"
        update_device_status(topic_device_id, is_online)

        ws_event = {
            "event": "status",
            "data": {
                "device_id": topic_device_id,
                "state": state_str,
                "is_online": is_online,
                "updated_at": datetime.now(timezone.utc).isoformat(),
            },
        }
        self._enqueue_ws_event(ws_event)

    def _handle_alert(self, topic_device_id: str, payload: dict[str, Any]) -> None:
        device_id = str(payload.get("device_id", topic_device_id))
        ts_rtc = int(payload.get("ts_rtc", int(datetime.now(timezone.utc).timestamp())))
        alert_record = {
            "device_id": device_id,
            "seq_no": int(payload.get("seq_no", 0)),
            "alert_code": int(payload.get("alert_code", 1)),
            "severity": str(payload.get("severity", "CRITICAL")),
            "temp_c": float(payload.get("temp_c", 0.0)),
            "hum_pct": float(payload.get("hum_pct", 0.0)),
            "message": str(payload.get("message", "Cảnh báo vượt ngưỡng nhiệt độ GSP!")),
            "ts_rtc": ts_rtc,
        }
        alert_id = insert_alert(alert_record)

        ws_event = {
            "event": "alert",
            "data": {
                **alert_record,
                "alert_id": alert_id,
                "ts_iso": datetime.fromtimestamp(ts_rtc, tz=timezone.utc).isoformat(),
            },
        }
        self._enqueue_ws_event(ws_event)