#!/usr/bin/env python3
"""
GSP Cold-Chain IoT - Edge Device & Attack Scenario Simulator
Giả lập thiết bị biên ESP32-S3-WROOM-1-N16R8 phục vụ kiểm thử End-to-End (Walking Skeleton).
Hỗ trợ đầy đủ 5 kịch bản:
  1. Telemetry chuẩn GSP (+2.0°C .. +8.0°C) kèm băm SHA-256 thật & chữ ký ECDSA Dummy.
  2. Vi phạm ngưỡng nhiệt độ GSP (> 8.0°C) bắn đồng thời kênh telemetry và alert.
  3. Đẩy bù dữ liệu Offline từ thẻ MicroSD (kênh sync) & kiểm tra chống trùng lặp (Idempotency).
  4. Tấn công sửa đổi gói tin trên đường truyền (Man-in-the-Middle) để test phát hiện TAMPERED.
  5. Giả lập mất kết nối/ngắt nguồn để kiểm tra bản tin trạng thái ONLINE/OFFLINE (LWT).
"""

import argparse
import hashlib
import json
import os
import random
import sys
import time
from datetime import datetime, timezone
from typing import Any

import paho.mqtt.client as mqtt

DEFAULT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "localhost")
DEFAULT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "1883"))
DEFAULT_DEVICE_ID = os.getenv("DEVICE_ID", "GSP-TRUCK-01")


def build_canonical_string(
    device_id: str,
    seq_no: int,
    ts_rtc: int,
    temp_c: float,
    hum_pct: float,
    alert_code: int,
) -> str:
    """
    Ghép chuỗi chuẩn hóa ký số đúng theo Data Contract:
    "<device_id>|<seq_no>|<ts_rtc>|<temp_2dec>|<hum_2dec>|<alert_code>"
    """
    return f"{device_id}|{int(seq_no)}|{int(ts_rtc)}|{float(temp_c):.2f}|{float(hum_pct):.2f}|{int(alert_code)}"


def compute_sha256_hex(canonical_str: str) -> str:
    """Băm SHA-256 chuỗi chuẩn hóa (64 ký tự hex chữ thường)."""
    return hashlib.sha256(canonical_str.encode("utf-8")).hexdigest()


class GSPEdgeSimulator:
    def __init__(self, host: str, port: int, device_id: str) -> None:
        self.host = host
        self.port = port
        self.device_id = device_id
        self.seq_no = int(time.time()) % 10000 + 1000
        self.bat_v = 4.12

        self.topic_telemetry = f"gsp/v1/{self.device_id}/telemetry"
        self.topic_sync = f"gsp/v1/{self.device_id}/sync"
        self.topic_alert = f"gsp/v1/{self.device_id}/alert"
        self.topic_status = f"gsp/v1/{self.device_id}/status"

        client_id = f"sim-{self.device_id}-{random.randint(100, 999)}"
        if hasattr(mqtt, "CallbackAPIVersion"):
            self.client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION1,
                client_id=client_id,
                clean_session=True,
            )
        else:
            self.client = mqtt.Client(
                client_id=client_id,
                clean_session=True,
            )

        # Cài đặt Di chúc LWT: QoS 1, Retain = True
        lwt_payload = json.dumps({"state": "OFFLINE"})
        self.client.will_set(self.topic_status, payload=lwt_payload, qos=1, retain=True)

        self.client.on_connect = self._on_connect

    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: dict, rc: int) -> None:
        if rc == 0:
            print(f"[MQTT] Đã kết nối tới Broker {self.host}:{self.port} (Thiết bị: {self.device_id})")
            online_payload = json.dumps({"state": "ONLINE"})
            client.publish(self.topic_status, payload=online_payload, qos=1, retain=True)
            print(f"[STATUS] Đã phát bản tin trạng thái ONLINE (Retain=True) lên {self.topic_status}")
        else:
            print(f"[LỖI] Kết nối MQTT thất bại với mã rc={rc}")

    def connect(self) -> None:
        self.client.connect(self.host, self.port, keepalive=30)
        self.client.loop_start()
        time.sleep(0.8)

    def disconnect_gracefully(self) -> None:
        offline_payload = json.dumps({"state": "OFFLINE"})
        self.client.publish(self.topic_status, payload=offline_payload, qos=1, retain=True)
        time.sleep(0.3)
        self.client.loop_stop()
        self.client.disconnect()
        print("[MQTT] Đã gửi trạng thái OFFLINE và ngắt kết nối an toàn.")

    def create_signed_payload(
        self,
        temp_c: float,
        hum_pct: float,
        alert_code: int = 0,
        ts_override: int | None = None,
        seq_override: int | None = None,
    ) -> tuple[dict[str, Any], str]:
        if seq_override is not None:
            seq = seq_override
        else:
            self.seq_no += 1
            seq = self.seq_no

        ts_rtc = ts_override if ts_override is not None else int(datetime.now(timezone.utc).timestamp())
        temp_rounded = round(float(temp_c), 2)
        hum_rounded = round(float(hum_pct), 2)
        self.bat_v = max(3.45, round(self.bat_v - 0.01, 2))

        canonical = build_canonical_string(
            device_id=self.device_id,
            seq_no=seq,
            ts_rtc=ts_rtc,
            temp_c=temp_rounded,
            hum_pct=hum_rounded,
            alert_code=alert_code,
        )
        sha256_hex = compute_sha256_hex(canonical)

        payload = {
            "device_id": self.device_id,
            "seq_no": seq,
            "ts_rtc": ts_rtc,
            "temp_c": temp_rounded,
            "hum_pct": hum_rounded,
            "bat_v": self.bat_v,
            "alert_code": alert_code,
            "sha256_hash": sha256_hex,
            "ecdsa_sig": "DUMMY_SIG_OK_ESP32S3_SECP256R1",
        }
        return payload, canonical

    def send_normal_telemetry(self) -> None:
        """Kịch bản 1: Gửi gói tin đo đạt chuẩn GSP (+2.0°C đến +8.0°C)."""
        temp_c = round(random.uniform(3.80, 5.40), 2)
        hum_pct = round(random.uniform(60.00, 65.00), 2)
        payload, canonical = self.create_signed_payload(temp_c=temp_c, hum_pct=hum_pct, alert_code=0)

        info = self.client.publish(self.topic_telemetry, json.dumps(payload), qos=1, retain=False)
        info.wait_for_publish()
        print(
            f"[REALTIME OK] Seq=#{payload['seq_no']} | Temp={temp_c:.2f}°C | "
            f"Hum={hum_pct:.2f}% | Canonical='{canonical}' | SHA256={payload['sha256_hash'][:12]}..."
        )

    def send_excursion_alert(self) -> None:
        """Kịch bản 2: Giả lập nhiệt độ kho lạnh vọt lên > 8.0°C (Vi phạm GSP)."""
        temp_c = round(random.uniform(9.20, 11.50), 2)
        hum_pct = round(random.uniform(68.00, 74.00), 2)
        payload, canonical = self.create_signed_payload(temp_c=temp_c, hum_pct=hum_pct, alert_code=1)

        # 1. Gửi bản ghi đo lên kênh telemetry
        self.client.publish(self.topic_telemetry, json.dumps(payload), qos=1, retain=False)

        # 2. Bắn cảnh báo khẩn cấp lên kênh alert
        alert_msg = {
            "device_id": self.device_id,
            "seq_no": payload["seq_no"],
            "ts_rtc": payload["ts_rtc"],
            "alert_code": 1,
            "severity": "CRITICAL",
            "temp_c": temp_c,
            "hum_pct": hum_pct,
            "message": f"CẢNH BÁO GSP: Nhiệt độ kho vọt lên {temp_c:.2f}°C (Vượt ngưỡng +8.00°C)!",
        }
        info = self.client.publish(self.topic_alert, json.dumps(alert_msg), qos=1, retain=False)
        info.wait_for_publish()
        print(
            f"[ALERT GSP!] Seq=#{payload['seq_no']} | Temp={temp_c:.2f}°C (VƯỢT NGƯỠNG) | "
            f"Canonical='{canonical}'"
        )

    def send_sd_backfill_sync(self) -> None:
        """
        Kịch bản 3: Giả lập đẩy bù 3 bản ghi từ thẻ nhớ MicroSD lên kênh sync
        và cố tình gửi lặp lại bản ghi thứ 3 để kiểm chứng ON CONFLICT DO NOTHING.
        """
        print("\n--- [AUTO-SYNC] Bắt đầu đẩy bù dữ liệu Offline từ thẻ MicroSD ---")
        base_ts = int(datetime.now(timezone.utc).timestamp()) - 180
        last_payload = None

        for idx in range(3):
            ts_past = base_ts + (idx * 60)
            temp_c = round(4.10 + (idx * 0.25), 2)
            hum_pct = round(61.50 + (idx * 0.40), 2)
            payload, _ = self.create_signed_payload(
                temp_c=temp_c,
                hum_pct=hum_pct,
                alert_code=0,
                ts_override=ts_past,
            )
            last_payload = payload
            info = self.client.publish(self.topic_sync, json.dumps(payload), qos=1, retain=False)
            info.wait_for_publish()
            print(
                f"  -> [SYNC #{idx + 1}/3] Đã đẩy bản ghi SD: Seq=#{payload['seq_no']}, "
                f"ts_rtc={ts_past}, Temp={temp_c:.2f}°C"
            )
            time.sleep(0.5)

        if last_payload is not None:
            print("  -> [TEST CHỐNG TRÙNG LẶP] Cố tình gửi lại bản ghi SD vừa rồi (cùng device_id & ts_rtc)...")
            info = self.client.publish(self.topic_sync, json.dumps(last_payload), qos=1, retain=False)
            info.wait_for_publish()
            print("  -> [HOÀN TẤT] Backend sẽ tự động bỏ qua bản ghi trùng nhờ ON CONFLICT DO NOTHING!\n")

    def send_tampered_packet(self) -> None:
        """
        Kịch bản 4: Giả lập tấn công sửa đổi dữ liệu (Tampering).
        Thiết bị đo được 9.80°C (vượt ngưỡng), nhưng kẻ gian sửa JSON thành 4.50°C trên đường truyền
        mà không thể làm giả chữ ký/mã băm gốc -> Hệ thống sẽ bắt lỗi TAMPERED!
        """
        real_temp = 9.80
        hum_pct = 64.00
        payload, canonical = self.create_signed_payload(temp_c=real_temp, hum_pct=hum_pct, alert_code=1)

        # Kẻ tấn công sửa nhiệt độ trong gói JSON xuống 4.50°C và xóa alert_code về 0
        payload["temp_c"] = 4.50
        payload["alert_code"] = 0

        info = self.client.publish(self.topic_telemetry, json.dumps(payload), qos=1, retain=False)
        info.wait_for_publish()
        print(
            f"[ATTACK SIMULATED] Seq=#{payload['seq_no']} | Nhiệt độ thực ký số={real_temp:.2f}°C "
            f"nhưng bị sửa lậu thành {payload['temp_c']:.2f}°C -> Backend sẽ đánh dấu TAMPERED!"
        )

    def toggle_status(self, state: str) -> None:
        """Kịch bản 5: Giả lập chuyển đổi trạng thái ONLINE / OFFLINE (LWT)."""
        payload = json.dumps({"state": state.upper()})
        info = self.client.publish(self.topic_status, payload=payload, qos=1, retain=True)
        info.wait_for_publish()
        print(f"[STATUS] Đã cập nhật trạng thái thiết bị sang: {state.upper()}")


def run_auto_demo(sim: GSPEdgeSimulator, interval: float) -> None:
    print("\n=====================================================================")
    print("  CHẾ ĐỘ CHẠY TỰ ĐỘNG (AUTO DEMO) - Nhấn Ctrl+C để dừng")
    print("  Chu kỳ gửi: %.1f giây/bản tin" % interval)
    print("=====================================================================\n")
    step = 0
    try:
        while True:
            step += 1
            if step % 8 == 0:
                sim.send_excursion_alert()
            elif step % 11 == 0:
                sim.send_sd_backfill_sync()
            elif step % 15 == 0:
                sim.send_tampered_packet()
            else:
                sim.send_normal_telemetry()
            time.sleep(interval)
    except KeyboardInterrupt:
        print("\n[DỪNG] Đang đóng kết nối Simulator...")


def run_interactive_menu(sim: GSPEdgeSimulator, interval: float) -> None:
    menu_text = """
=====================================================================
   BẢNG ĐIỀU KHIỂN KIỂM THỬ HỆ THỐNG GSP COLD-CHAIN IOT (SIMULATOR)
=====================================================================
  [1] Gửi 1 bản tin Telemetry chuẩn GSP (+3.8°C .. +5.4°C) -> ECDSA Verified
  [2] Gửi 1 bản tin Vượt ngưỡng (> +9.2°C) + Cảnh báo khẩn cấp -> Nhấp nháy ĐỎ
  [3] Giả lập Đẩy bù thẻ nhớ MicroSD (3 bản ghi + 1 bản ghi trùng lặp) -> AUTO-SYNC
  [4] Giả lập Tấn công sửa đổi nhiệt độ trái phép -> Phát hiện TAMPERED
  [5] Giả lập Mất kết nối đột ngột (Bắn bản tin LWT OFFLINE)
  [6] Khôi phục trạng thái thiết bị ONLINE
  [7] Chạy tự động liên hoàn tất cả kịch bản (Auto Demo Loop)
  [0] Thoát chương trình
====================================================================="""
    try:
        while True:
            print(menu_text)
            choice = input("Nhập lựa chọn của bạn [0-7]: ").strip()
            if choice == "1":
                sim.send_normal_telemetry()
            elif choice == "2":
                sim.send_excursion_alert()
            elif choice == "3":
                sim.send_sd_backfill_sync()
            elif choice == "4":
                sim.send_tampered_packet()
            elif choice == "5":
                sim.toggle_status("OFFLINE")
            elif choice == "6":
                sim.toggle_status("ONLINE")
            elif choice == "7":
                run_auto_demo(sim, interval)
            elif choice == "0":
                break
            else:
                print("Lựa chọn không hợp lệ, vui lòng nhập từ 0 đến 7.")
    except KeyboardInterrupt:
        print("\nĐã nhận lệnh ngắt từ bàn phím.")


def main() -> None:
    parser = argparse.ArgumentParser(description="GSP Cold-Chain IoT Edge Simulator")
    parser.add_argument("--host", default=DEFAULT_BROKER_HOST, help="Địa chỉ MQTT Broker (mặc định: localhost)")
    parser.add_argument("--port", type=int, default=DEFAULT_BROKER_PORT, help="Cổng MQTT Broker (mặc định: 1883)")
    parser.add_argument("--device-id", default=DEFAULT_DEVICE_ID, help="Mã thiết bị (mặc định: GSP-TRUCK-01)")
    parser.add_argument(
        "--mode",
        choices=["interactive", "auto"],
        default="interactive",
        help="Chế độ chạy: interactive (menu chọn kịch bản) hoặc auto (tự động xoay vòng)",
    )
    parser.add_argument("--interval", type=float, default=3.0, help="Chu kỳ gửi ở chế độ auto (giây)")
    args = parser.parse_args()

    sim = GSPEdgeSimulator(host=args.host, port=args.port, device_id=args.device_id)
    try:
        sim.connect()
        if args.mode == "auto":
            run_auto_demo(sim, args.interval)
        else:
            run_interactive_menu(sim, args.interval)
    except Exception as exc:
        print(f"[LỖI KẾT NỐI] Không thể kết nối tới MQTT Broker tại {args.host}:{args.port}: {exc}")
        sys.exit(1)
    finally:
        sim.disconnect_gracefully()


if __name__ == "__main__":
    main()