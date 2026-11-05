#!/usr/bin/env python3
"""
GSP Cold-Chain IoT - ESP32-S3 Hardware-in-the-Loop (HIL) Desktop GUI Simulator
"""

import csv
import hashlib
import json
import os
import random
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import customtkinter as ctk
import paho.mqtt.client as mqtt

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

DEFAULT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "localhost")
DEFAULT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "1883"))
DEFAULT_DEVICE_ID = os.getenv("DEVICE_ID", "GSP-TRUCK-01")

SD_CARD_FILE = Path(__file__).resolve().parent / "sd_card_buffer.csv"
SD_CSV_HEADERS = [
    "device_id",
    "seq_no",
    "ts_rtc",
    "temp_c",
    "hum_pct",
    "bat_v",
    "alert_code",
    "sha256_hash",
    "ecdsa_sig",
    "synced",
]


def build_canonical_string(
    device_id: str,
    seq_no: int,
    ts_rtc: int,
    temp_c: float,
    hum_pct: float,
    alert_code: int,
) -> str:
    return f"{device_id}|{int(seq_no)}|{int(ts_rtc)}|{float(temp_c):.2f}|{float(hum_pct):.2f}|{int(alert_code)}"


def compute_sha256_hex(canonical_str: str) -> str:
    return hashlib.sha256(canonical_str.encode("utf-8")).hexdigest()


class ESP32S3GuiSimulator(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()

        self.title("GSP ESP32-S3 Hardware Twin & Attack Simulator (macOS Native)")
        self.geometry("1220x820")
        self.minsize(1100, 760)

        # Trạng thái phần cứng ESP32-S3
        self.device_id = DEFAULT_DEVICE_ID
        self.broker_host = DEFAULT_BROKER_HOST
        self.broker_port = DEFAULT_BROKER_PORT
        self.seq_no = int(time.time()) % 10000 + 1000

        self.temp_c = 4.50
        self.hum_pct = 62.50
        self.bat_v = 4.18
        self.sample_interval_sec = 3.0

        # Các cờ trạng thái vật lý & kịch bản
        self.mcu_powered = True
        self.ac_power_online = True
        self.wifi_enabled = True
        self.mqtt_connected = False
        self.mqtt_connecting = False
        self.sd_inserted = True
        self.alarm_muted = False
        self.relay_active = False
        self.mitm_attack_next = False
        self.dup_sync_test = True

        # Topics MQTT
        self.topic_telemetry = f"gsp/v1/{self.device_id}/telemetry"
        self.topic_sync = f"gsp/v1/{self.device_id}/sync"
        self.topic_alert = f"gsp/v1/{self.device_id}/alert"
        self.topic_status = f"gsp/v1/{self.device_id}/status"

        self.mqtt_lock = threading.Lock()
        self.mqtt_client: mqtt.Client | None = None

        self._init_sd_card_file()
        self._build_ui()

        # Kết nối MQTT lần đầu
        self._log_serial(f"[BOOT] Khởi động ESP32-S3-N16R8 ({self.device_id})...")
        self._start_mqtt_session()

        self.running = True
        self.last_sample_ts = 0.0
        self.last_reconnect_try_ts = 0.0
        self.worker_thread = threading.Thread(target=self._hardware_loop, daemon=True)
        self.worker_thread.start()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # =========================================================================
    # 1. QUẢN LÝ FILE THẺ NHỚ MICROSD (CSV)
    # =========================================================================
    def _init_sd_card_file(self) -> None:
        if not SD_CARD_FILE.exists():
            with open(SD_CARD_FILE, mode="w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=SD_CSV_HEADERS)
                writer.writeheader()

    def _read_sd_records(self) -> list[dict[str, str]]:
        if not SD_CARD_FILE.exists():
            return []
        with open(SD_CARD_FILE, mode="r", newline="", encoding="utf-8") as f:
            return list(csv.DictReader(f))

    def _write_sd_records(self, rows: list[dict[str, Any]]) -> None:
        with open(SD_CARD_FILE, mode="w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=SD_CSV_HEADERS)
            writer.writeheader()
            writer.writerows(rows)

    def _append_sd_record(self, record: dict[str, Any]) -> None:
        rows = self._read_sd_records()
        rows.append({
            "device_id": record["device_id"],
            "seq_no": str(record["seq_no"]),
            "ts_rtc": str(record["ts_rtc"]),
            "temp_c": f"{float(record['temp_c']):.2f}",
            "hum_pct": f"{float(record['hum_pct']):.2f}",
            "bat_v": f"{float(record['bat_v']):.2f}",
            "alert_code": str(record["alert_code"]),
            "sha256_hash": record["sha256_hash"],
            "ecdsa_sig": record["ecdsa_sig"],
            "synced": "0",
        })
        self._write_sd_records(rows)
        self.after(0, self._refresh_sd_ui_list)

    # =========================================================================
    # 2. XÂY DỰNG GIAO DIỆN DESKTOP (CUSTOMTKINTER)
    # =========================================================================
    def _build_ui(self) -> None:
        self.grid_columnconfigure(0, weight=5)
        self.grid_columnconfigure(1, weight=6)
        self.grid_rowconfigure(1, weight=1)

        header = ctk.CTkFrame(self, corner_radius=10, fg_color="#0f172a")
        header.grid(row=0, column=0, columnspan=2, sticky="ew", padx=12, pady=(12, 6))
        header.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(
            header,
            text="MÔ PHỎNG PHẦN CỨNG ESP32",
            font=ctk.CTkFont(size=18, weight="bold"),
            text_color="#38bdf8",
        ).grid(row=0, column=0, padx=16, pady=10, sticky="w")

        self.status_badge = ctk.CTkLabel(
            header,
            text="● MQTT: ĐANG KẾT NỐI...",
            font=ctk.CTkFont(size=13, weight="bold"),
            fg_color="#334155",
            corner_radius=6,
            padx=12,
            pady=4,
        )
        self.status_badge.grid(row=0, column=2, padx=16, pady=10, sticky="e")

        # --- CỘT TRÁI ---
        left_col = ctk.CTkFrame(self, fg_color="transparent")
        left_col.grid(row=1, column=0, sticky="nsew", padx=(12, 6), pady=6)
        left_col.grid_columnconfigure(0, weight=1)

        oled_card = ctk.CTkFrame(left_col, corner_radius=10)
        oled_card.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        oled_card.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            oled_card,
            text="MÀN HÌNH VÀ BÁO ĐỘNG TẠI CHỖ",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color="#94a3b8",
        ).grid(row=0, column=0, padx=14, pady=(10, 6), sticky="w")

        self.oled_screen = ctk.CTkLabel(
            oled_card,
            text="",
            font=ctk.CTkFont(family="Courier", size=15, weight="bold"),
            fg_color="#020617",
            text_color="#22d3ee",
            corner_radius=8,
            justify="left",
            anchor="w",
            padx=18,
            pady=14,
        )
        self.oled_screen.grid(row=1, column=0, sticky="ew", padx=14, pady=4)

        hw_bar = ctk.CTkFrame(oled_card, fg_color="transparent")
        hw_bar.grid(row=2, column=0, sticky="ew", padx=14, pady=(6, 12))
        hw_bar.grid_columnconfigure((0, 1, 2, 3), weight=1)

        self.relay_indicator = ctk.CTkLabel(
            hw_bar,
            text="🚨 CÒI & ĐÈN (GPIO21): TẮT",
            font=ctk.CTkFont(size=12, weight="bold"),
            fg_color="#1e293b",
            corner_radius=6,
            pady=6,
        )
        self.relay_indicator.grid(row=0, column=0, columnspan=2, sticky="ew", padx=(0, 6))

        self.btn_ack = ctk.CTkButton(
            hw_bar,
            text="BTN2 (Tắt Còi)",
            fg_color="#475569",
            hover_color="#334155",
            command=self._on_press_btn_ack,
        )
        self.btn_ack.grid(row=0, column=2, sticky="ew", padx=4)

        self.btn_test_spike = ctk.CTkButton(
            hw_bar,
            text="BTN3 (Vọt 9.8°C)",
            fg_color="#dc2626",
            hover_color="#b91c1c",
            command=self._on_press_btn_test,
        )
        self.btn_test_spike.grid(row=0, column=3, sticky="ew", padx=(4, 0))

        # Cảm biến SHT30
        sensor_card = ctk.CTkFrame(left_col, corner_radius=10)
        sensor_card.grid(row=1, column=0, sticky="ew", pady=(0, 10))
        sensor_card.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            sensor_card,
            text="🌡️ ĐIỀU KHIỂN SHT30 (NGƯỠNG GSP: 2.0°C đến 8.0°C)",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color="#94a3b8",
        ).grid(row=0, column=0, padx=14, pady=(10, 4), sticky="w")

        self.temp_lbl = ctk.CTkLabel(
            sensor_card,
            text="Nhiệt độ SHT30 hiện tại: +4.50 °C (CHUẨN)",
            font=ctk.CTkFont(size=14, weight="bold"),
            text_color="#4ade80",
        )
        self.temp_lbl.grid(row=1, column=0, padx=14, pady=(4, 0), sticky="w")

        self.temp_slider = ctk.CTkSlider(
            sensor_card,
            from_=-5.0,
            to=18.0,
            number_of_steps=230,
            command=self._on_temp_slider_change,
        )
        self.temp_slider.set(4.50)
        self.temp_slider.grid(row=2, column=0, sticky="ew", padx=14, pady=6)

        preset_frame = ctk.CTkFrame(sensor_card, fg_color="transparent")
        preset_frame.grid(row=3, column=0, sticky="ew", padx=14, pady=(2, 8))
        preset_frame.grid_columnconfigure((0, 1, 2), weight=1)

        ctk.CTkButton(
            preset_frame,
            text="❄️ Dưới ngưỡng",
            height=28,
            fg_color="#0284c7",
            command=lambda: self._set_temp_preset(0.50),
        ).grid(row=0, column=0, sticky="ew", padx=(0, 4))

        ctk.CTkButton(
            preset_frame,
            text="✅ Chuẩn Vaccine",
            height=28,
            fg_color="#16a34a",
            command=lambda: self._set_temp_preset(4.50),
        ).grid(row=0, column=1, sticky="ew", padx=4)

        ctk.CTkButton(
            preset_frame,
            text="🔥 Vượt ngưỡng",
            height=28,
            fg_color="#e11d48",
            command=lambda: self._set_temp_preset(10.20),
        ).grid(row=0, column=2, sticky="ew", padx=(4, 0))

        self.hum_lbl = ctk.CTkLabel(
            sensor_card,
            text="Độ ẩm tương đối (RH): 62.50 %",
            font=ctk.CTkFont(size=13, weight="bold"),
        )
        self.hum_lbl.grid(row=4, column=0, padx=14, pady=(4, 0), sticky="w")

        self.hum_slider = ctk.CTkSlider(
            sensor_card,
            from_=30.0,
            to=95.0,
            number_of_steps=130,
            command=self._on_hum_slider_change,
        )
        self.hum_slider.set(62.50)
        self.hum_slider.grid(row=5, column=0, sticky="ew", padx=14, pady=6)

        self.interval_lbl = ctk.CTkLabel(
            sensor_card,
            text="Chu kỳ đọc và bắn gói tin: 3s/gói",
            font=ctk.CTkFont(size=12),
            text_color="#cbd5e1",
        )
        self.interval_lbl.grid(row=6, column=0, padx=14, pady=(4, 0), sticky="w")

        self.interval_slider = ctk.CTkSlider(
            sensor_card,
            from_=1.0,
            to=10.0,
            number_of_steps=18,
            command=self._on_interval_slider_change,
        )
        self.interval_slider.set(3.0)
        self.interval_slider.grid(row=7, column=0, sticky="ew", padx=14, pady=(4, 12))

        # Nguồn & Mạng
        power_net_card = ctk.CTkFrame(left_col, corner_radius=10)
        power_net_card.grid(row=2, column=0, sticky="ew")
        power_net_card.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            power_net_card,
            text="NGUỒN ĐIỆN VÀ KẾT NỐI MẠNG WI-FI",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color="#94a3b8",
        ).grid(row=0, column=0, padx=14, pady=(10, 6), sticky="w")

        self.ac_switch = ctk.CTkSwitch(
            power_net_card,
            text="Nguồn điện lưới (tốc độ giảm 0.08V/s nếu ngắt điện)",
            font=ctk.CTkFont(size=13, weight="bold"),
            command=self._on_toggle_ac_power,
        )
        self.ac_switch.select()
        self.ac_switch.grid(row=1, column=0, padx=14, pady=4, sticky="w")

        self.bat_progress = ctk.CTkProgressBar(power_net_card, height=14)
        self.bat_progress.set(1.0)
        self.bat_progress.grid(row=2, column=0, sticky="ew", padx=14, pady=6)

        self.bat_status_lbl = ctk.CTkLabel(
            power_net_card,
            text="Điện áp Pin: 4.18 V (100% - Đang cắm nguồn 5V)",
            font=ctk.CTkFont(size=12, weight="bold"),
            text_color="#34d399",
        )
        self.bat_status_lbl.grid(row=3, column=0, padx=14, pady=(0, 8), sticky="w")

        net_btn_frame = ctk.CTkFrame(power_net_card, fg_color="transparent")
        net_btn_frame.grid(row=4, column=0, sticky="ew", padx=14, pady=(4, 12))
        net_btn_frame.grid_columnconfigure((0, 1), weight=1)

        self.wifi_switch = ctk.CTkSwitch(
            net_btn_frame,
            text="Kết nối Wi-Fi",
            font=ctk.CTkFont(size=13, weight="bold"),
            command=self._on_toggle_wifi,
        )
        self.wifi_switch.select()
        self.wifi_switch.grid(row=0, column=0, sticky="w")

        self.btn_lwt_kill = ctk.CTkButton(
            net_btn_frame,
            text="Mất kết nối đột ngột",
            fg_color="#7c2d12",
            hover_color="#9a3412",
            command=self._on_simulate_unplug_lwt,
        )
        self.btn_lwt_kill.grid(row=0, column=1, sticky="e")

        # --- CỘT PHẢI ---
        right_col = ctk.CTkFrame(self, fg_color="transparent")
        right_col.grid(row=1, column=1, sticky="nsew", padx=(6, 12), pady=6)
        right_col.grid_columnconfigure(0, weight=1)
        right_col.grid_rowconfigure(1, weight=1)

        sd_card_frame = ctk.CTkFrame(right_col, corner_radius=10)
        sd_card_frame.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        sd_card_frame.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            sd_card_frame,
            text="💾 THẺ NHỚ MICROSD VÀ THÍ NGHIỆM TẤN CÔNG SỬA DỮ LIỆU",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color="#f59e0b",
        ).grid(row=0, column=0, padx=14, pady=(10, 4), sticky="w")

        sd_top_bar = ctk.CTkFrame(sd_card_frame, fg_color="transparent")
        sd_top_bar.grid(row=1, column=0, sticky="ew", padx=14, pady=4)
        sd_top_bar.grid_columnconfigure(1, weight=1)

        self.btn_eject_sd = ctk.CTkButton(
            sd_top_bar,
            text="⏏️ Rút thẻ nhớ ra để sửa trộm",
            fg_color="#d97706",
            hover_color="#b45309",
            command=self._on_toggle_sd_card,
        )
        self.btn_eject_sd.grid(row=0, column=0, sticky="w")

        self.sd_status_lbl = ctk.CTkLabel(
            sd_top_bar,
            text="Trạng thái: ĐANG CẮM TRONG MẠCH (0 bản ghi chờ Sync)",
            font=ctk.CTkFont(size=12, weight="bold"),
            text_color="#fde047",
        )
        self.sd_status_lbl.grid(row=0, column=1, padx=10, sticky="e")

        self.sd_textbox = ctk.CTkTextbox(
            sd_card_frame,
            height=135,
            font=ctk.CTkFont(family="Courier", size=12),
            fg_color="#090d16",
            text_color="#e2e8f0",
        )
        self.sd_textbox.grid(row=2, column=0, sticky="ew", padx=14, pady=6)

        tamper_bar = ctk.CTkFrame(sd_card_frame, fg_color="#1e1b4b", corner_radius=8)
        tamper_bar.grid(row=3, column=0, sticky="ew", padx=14, pady=6)
        tamper_bar.grid_columnconfigure((1, 3), weight=1)

        ctk.CTkLabel(
            tamper_bar,
            text="🕵️ Sửa lậu bản ghi SD (Seq #):",
            font=ctk.CTkFont(size=12, weight="bold"),
        ).grid(row=0, column=0, padx=8, pady=8)

        self.entry_tamper_seq = ctk.CTkEntry(
            tamper_bar, width=80, placeholder_text="VD: 1005"
        )
        self.entry_tamper_seq.grid(row=0, column=1, padx=4, pady=8, sticky="ew")

        ctk.CTkLabel(
            tamper_bar,
            text="Sửa Nhiệt độ giả (°C):",
            font=ctk.CTkFont(size=12, weight="bold"),
        ).grid(row=0, column=2, padx=8, pady=8)

        self.entry_tamper_temp = ctk.CTkEntry(
            tamper_bar, width=70, placeholder_text="4.50"
        )
        self.entry_tamper_temp.insert(0, "4.50")
        self.entry_tamper_temp.grid(row=0, column=3, padx=4, pady=8, sticky="ew")

        self.btn_apply_sd_tamper = ctk.CTkButton(
            tamper_bar,
            text="💉 Ghi đè vào thẻ SD",
            fg_color="#be123c",
            hover_color="#9f1239",
            command=self._on_tamper_sd_record,
        )
        self.btn_apply_sd_tamper.grid(row=0, column=4, padx=8, pady=8)

        attack_action_bar = ctk.CTkFrame(sd_card_frame, fg_color="transparent")
        attack_action_bar.grid(row=4, column=0, sticky="ew", padx=14, pady=(2, 12))
        attack_action_bar.grid_columnconfigure((0, 1, 2), weight=1)

        self.btn_mitm_attack = ctk.CTkButton(
            attack_action_bar,
            text="⚡ Bắn 1 gói MITM Tampered ngay",
            fg_color="#9333ea",
            hover_color="#7e22ce",
            command=self._on_trigger_mitm_packet,
        )
        self.btn_mitm_attack.grid(row=0, column=0, padx=(0, 4), sticky="ew")

        self.btn_force_sync = ctk.CTkButton(
            attack_action_bar,
            text="🔄 Đẩy bù thẻ SD ngay",
            fg_color="#0d9488",
            hover_color="#0f766e",
            command=lambda: threading.Thread(target=self._sync_sd_card_to_broker, daemon=True).start(),
        )
        self.btn_force_sync.grid(row=0, column=1, padx=4, sticky="ew")

        self.btn_clear_sd = ctk.CTkButton(
            attack_action_bar,
            text="🗑️ Xóa sạch thẻ SD",
            fg_color="#475569",
            hover_color="#334155",
            command=self._on_clear_sd_card,
        )
        self.btn_clear_sd.grid(row=0, column=2, padx=(4, 0), sticky="ew")

        log_card = ctk.CTkFrame(right_col, corner_radius=10)
        log_card.grid(row=1, column=0, sticky="nsew")
        log_card.grid_columnconfigure(0, weight=1)
        log_card.grid_rowconfigure(1, weight=1)

        ctk.CTkLabel(
            log_card,
            text="📟 SERIAL MONITOR",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color="#94a3b8",
        ).grid(row=0, column=0, padx=14, pady=(10, 4), sticky="w")

        self.serial_log = ctk.CTkTextbox(
            log_card,
            font=ctk.CTkFont(family="Courier", size=12),
            fg_color="#020617",
            text_color="#38bdf8",
        )
        self.serial_log.grid(row=1, column=0, sticky="nsew", padx=14, pady=(4, 12))

        self._refresh_sd_ui_list()
        self._update_oled_display()

    # =========================================================================
    # 3. QUẢN LÝ PHIÊN KẾT NỐI MQTT (ĐÃ SỬA LỖI RECONNECT PAHO-MQTT)
    # =========================================================================
    def _create_mqtt_client_instance(self) -> mqtt.Client:
        client_id = f"esp32s3-gui-{self.device_id}-{random.randint(100, 999)}"
        if hasattr(mqtt, "CallbackAPIVersion"):
            client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION1,
                client_id=client_id,
                clean_session=True,
            )
        else:
            client = mqtt.Client(client_id=client_id, clean_session=True)

        lwt_payload = json.dumps({"state": "OFFLINE"})
        client.will_set(self.topic_status, payload=lwt_payload, qos=1, retain=True)
        client.on_connect = self._on_mqtt_connect
        client.on_disconnect = self._on_mqtt_disconnect
        return client

    def _start_mqtt_session(self) -> None:
        """Khởi tạo mới hoàn toàn phiên MQTT và vòng lặp mạng trên luồng phụ để không treo GUI."""
        if self.mqtt_connecting or not (self.wifi_enabled and self.mcu_powered):
            return

        self.mqtt_connecting = True
        self.after(0, lambda: self.status_badge.configure(text="● MQTT: ĐANG KẾT NỐI...", fg_color="#d97706"))

        def _connect_worker() -> None:
            with self.mqtt_lock:
                try:
                    # Dọn dẹp triệt để client cũ (nếu có) trước khi mở phiên mới
                    if self.mqtt_client is not None:
                        try:
                            self.mqtt_client.loop_stop()
                        except Exception:
                            pass
                        self.mqtt_client = None

                    client = self._create_mqtt_client_instance()
                    self.mqtt_client = client
                    self._log_serial(f"[MQTT] Đang kết nối tới Broker {self.broker_host}:{self.broker_port}...")
                    client.connect(self.broker_host, self.broker_port, keepalive=15)
                    client.loop_start()
                except Exception as exc:
                    self.mqtt_connected = False
                    self._log_serial(f"[LỖI MQTT] Kết nối thất bại: {exc} (Sẽ tự thử lại sau 3s)")
                    self.after(0, lambda: self.status_badge.configure(text="● MQTT: LỖI KẾT NỐI", fg_color="#e11d48"))
                finally:
                    self.mqtt_connecting = False

        threading.Thread(target=_connect_worker, daemon=True).start()

    def _stop_mqtt_session_offline(self) -> None:
        """Bắn bản tin OFFLINE (LWT) và đóng hoàn toàn vòng lặp mạng paho-mqtt."""
        self.mqtt_connected = False
        self.after(0, lambda: self.status_badge.configure(text="● MQTT: OFFLINE (LWT)", fg_color="#e11d48"))
        self.after(0, self._update_oled_display)

        def _disconnect_worker() -> None:
            with self.mqtt_lock:
                client = self.mqtt_client
                self.mqtt_client = None
                if client is not None:
                    try:
                        info = client.publish(
                            self.topic_status, json.dumps({"state": "OFFLINE"}), qos=1, retain=True
                        )
                        info.wait_for_publish(timeout=1.0)
                        client.disconnect()
                    except Exception:
                        pass
                    try:
                        client.loop_stop()
                    except Exception:
                        pass

        threading.Thread(target=_disconnect_worker, daemon=True).start()

    def _on_mqtt_connect(self, client: mqtt.Client, userdata: Any, flags: dict, rc: int) -> None:
        if rc == 0 and self.wifi_enabled and self.mcu_powered:
            self.mqtt_connected = True
            client.publish(self.topic_status, json.dumps({"state": "ONLINE"}), qos=1, retain=True)
            self._log_serial("[Core 1] Đã kết nối MQTT Broker -> Đã phát trạng thái ONLINE (Retain=True)!")
            self.after(0, lambda: self.status_badge.configure(text="● MQTT: ONLINE", fg_color="#059669"))
            self.after(0, self._update_oled_display)

            # Tự động đẩy bù dữ liệu thẻ nhớ SD ngay khi có mạng lại
            threading.Thread(target=self._sync_sd_card_to_broker, daemon=True).start()
        else:
            self.mqtt_connected = False

    def _on_mqtt_disconnect(self, client: mqtt.Client, userdata: Any, rc: int) -> None:
        # Chỉ xử lý nếu đây là client hiện hành
        if client is self.mqtt_client:
            self.mqtt_connected = False
            self.after(0, lambda: self.status_badge.configure(text="● MQTT: OFFLINE", fg_color="#e11d48"))
            self.after(0, self._update_oled_display)
            if rc != 0:
                self._log_serial(f"[Core 1] Mất kết nối tới Broker (rc={rc}). Đang chờ tự động kết nối lại...")

    # =========================================================================
    # 4. VÒNG LẶP PHẦN CỨNG (MÔ PHỎNG FREERTOS CORE 0 & CORE 1)
    # =========================================================================
    def _hardware_loop(self) -> None:
        while self.running:
            now = time.time()

            # 4.1. Quản lý nguồn điện 5V & Pin 18650
            if self.ac_power_online:
                if self.bat_v < 4.18:
                    self.bat_v = min(4.18, round(self.bat_v + 0.06, 2))
                if not self.mcu_powered and self.bat_v >= 3.50:
                    self.mcu_powered = True
                    self._log_serial("[POWER] Đã có điện lưới 5V -> ESP32-S3 khởi động lại!")
                    if self.wifi_enabled:
                        self._start_mqtt_session()
            else:
                self.bat_v = max(2.90, round(self.bat_v - 0.08, 2))
                if self.bat_v <= 3.00 and self.mcu_powered:
                    self.mcu_powered = False
                    self.relay_active = False
                    self._log_serial("[BROWNOUT RESET] Điện áp Pin sụt dưới 3.00V -> ESP32-S3 SẬP NGUỒN!")
                    self._stop_mqtt_session_offline()

            self.after(0, self._update_battery_ui)

            if not self.mcu_powered:
                self.after(0, self._update_oled_display)
                time.sleep(0.5)
                continue

            # 4.2. Cơ chế Watchdog tự động kết nối lại MQTT mỗi 3 giây (Giống TaskCore1 trong main.cpp)
            if (
                self.wifi_enabled
                and not self.mqtt_connected
                and not self.mqtt_connecting
                and (now - self.last_reconnect_try_ts >= 3.0)
            ):
                self.last_reconnect_try_ts = now
                self._start_mqtt_session()

            # 4.3. Chu kỳ lấy mẫu cảm biến (Core 0)
            if now - self.last_sample_ts >= self.sample_interval_sec:
                self.last_sample_ts = now
                self._execute_sensing_and_send_cycle()

            time.sleep(0.25)

    def _execute_sensing_and_send_cycle(self) -> None:
        self.seq_no += 1
        ts_rtc = int(datetime.now(timezone.utc).timestamp())

        noisy_temp = round(self.temp_c + random.uniform(-0.02, 0.02), 2)
        noisy_hum = round(self.hum_pct + random.uniform(-0.10, 0.10), 2)

        alert_code = 0
        if noisy_temp > 8.00:
            alert_code = 1
        elif noisy_temp < 2.00:
            alert_code = 2

        if alert_code > 0:
            if not self.alarm_muted:
                self.relay_active = True
        else:
            self.alarm_muted = False
            self.relay_active = False

        canonical = build_canonical_string(
            device_id=self.device_id,
            seq_no=self.seq_no,
            ts_rtc=ts_rtc,
            temp_c=noisy_temp,
            hum_pct=noisy_hum,
            alert_code=alert_code,
        )
        sha256_hex = compute_sha256_hex(canonical)

        packet = {
            "device_id": self.device_id,
            "seq_no": self.seq_no,
            "ts_rtc": ts_rtc,
            "temp_c": noisy_temp,
            "hum_pct": noisy_hum,
            "bat_v": self.bat_v,
            "alert_code": alert_code,
            "sha256_hash": sha256_hex,
            "ecdsa_sig": "DUMMY_SIG_OK_ESP32S3_SECP256R1",
        }

        if self.mitm_attack_next:
            self.mitm_attack_next = False
            real_signed_temp = 9.80
            canonical_attack = build_canonical_string(
                self.device_id, self.seq_no, ts_rtc, real_signed_temp, noisy_hum, 1
            )
            packet["sha256_hash"] = compute_sha256_hex(canonical_attack)
            packet["temp_c"] = 4.50
            packet["alert_code"] = 0
            self._log_serial(
                f"[MITM ATTACK] Seq=#{self.seq_no}: Đã ký SHA-256 ở 9.80°C nhưng sửa JSON thành 4.50°C!"
            )

        self.after(0, self._update_oled_display)

        client = self.mqtt_client
        if self.wifi_enabled and self.mqtt_connected and client is not None:
            client.publish(self.topic_telemetry, json.dumps(packet), qos=1, retain=False)
            self._log_serial(
                f"[Core 1 -> MQTT] Seq=#{self.seq_no} | T={packet['temp_c']:.2f}°C | "
                f"H={packet['hum_pct']:.2f}% | Bat={self.bat_v:.2f}V | Alert={alert_code}"
            )

            if alert_code > 0:
                alert_payload = {
                    "device_id": self.device_id,
                    "seq_no": self.seq_no,
                    "ts_rtc": ts_rtc,
                    "alert_code": alert_code,
                    "severity": "CRITICAL",
                    "temp_c": noisy_temp,
                    "hum_pct": noisy_hum,
                    "message": f"CẢNH BÁO GSP: Nhiệt độ ({noisy_temp:.2f}°C) vi phạm ngưỡng [+2..+8°C]!",
                }
                client.publish(self.topic_alert, json.dumps(alert_payload), qos=1, retain=False)
                self._log_serial(f"[Core 1 -> ALERT] Đã bắn cảnh báo khẩn cấp (Temp={noisy_temp:.2f}°C)!")
        else:
            if self.sd_inserted:
                self._append_sd_record(packet)
                self._log_serial(
                    f"[Core 1 -> SD CARD] Mất mạng! Đã ghi đệm Seq=#{self.seq_no} "
                    f"(T={packet['temp_c']:.2f}°C, ts={ts_rtc}) vào MicroSD."
                )
            else:
                self._log_serial(
                    f"[LỖI MẤT DỮ LIỆU!] Mất mạng Wi-Fi VÀ Thẻ nhớ SD đang bị rút ra -> Rớt gói Seq=#{self.seq_no}!"
                )

    # =========================================================================
    # 5. ĐỒNG BỘ BÙ TỪ THẺ NHỚ MICROSD (AUTO-SYNC) & XỬ LÝ TẤN CÔNG THẺ NHỚ
    # =========================================================================
    def _sync_sd_card_to_broker(self) -> None:
        if not self.sd_inserted:
            self._log_serial("[AUTO-SYNC] Không thể đồng bộ vì thẻ MicroSD đang bị rút ra ngoài!")
            return
        client = self.mqtt_client
        if not (self.wifi_enabled and self.mqtt_connected and client is not None):
            self._log_serial("[AUTO-SYNC] Chưa có kết nối MQTT để đẩy bù dữ liệu SD.")
            return

        rows = self._read_sd_records()
        unsynced = [r for r in rows if r.get("synced") == "0"]
        if not unsynced:
            return

        self._log_serial(f"[AUTO-SYNC] Phát hiện {len(unsynced)} bản ghi Offline trong thẻ SD. Bắt đầu đẩy bù...")
        last_sent_payload = None

        for row in rows:
            if row.get("synced") == "0":
                payload = {
                    "device_id": row["device_id"],
                    "seq_no": int(row["seq_no"]),
                    "ts_rtc": int(row["ts_rtc"]),
                    "temp_c": float(row["temp_c"]),
                    "hum_pct": float(row["hum_pct"]),
                    "bat_v": float(row["bat_v"]),
                    "alert_code": int(row["alert_code"]),
                    "sha256_hash": row["sha256_hash"],
                    "ecdsa_sig": row["ecdsa_sig"],
                }
                info = client.publish(self.topic_sync, json.dumps(payload), qos=1, retain=False)
                info.wait_for_publish(timeout=1.5)
                row["synced"] = "1"
                last_sent_payload = payload
                self._log_serial(
                    f"  -> [SYNC OK] Đã đẩy bù Seq=#{payload['seq_no']} (T={payload['temp_c']:.2f}°C) lên kênh sync"
                )
                time.sleep(0.30)

        if self.dup_sync_test and last_sent_payload is not None:
            self._log_serial(
                f"  -> [TEST DEDUP] Cố tình bắn lặp lại Seq=#{last_sent_payload['seq_no']} "
                "để kiểm chứng ON CONFLICT DO NOTHING..."
            )
            client.publish(self.topic_sync, json.dumps(last_sent_payload), qos=1, retain=False)

        self._write_sd_records(rows)
        self.after(0, self._refresh_sd_ui_list)
        self._log_serial("[AUTO-SYNC] Hoàn tất đồng bộ bù 100% dữ liệu từ thẻ nhớ MicroSD!")

    def _on_toggle_sd_card(self) -> None:
        self.sd_inserted = not self.sd_inserted
        if self.sd_inserted:
            self.btn_eject_sd.configure(
                text="⏏️ Rút thẻ MicroSD ra khỏi mạch để sửa file",
                fg_color="#d97706",
            )
            self._log_serial("[SD CARD] Đã cắm thẻ nhớ MicroSD trở lại khe SPI của ESP32-S3.")
            self._refresh_sd_ui_list()
            if self.wifi_enabled and self.mqtt_connected:
                threading.Thread(target=self._sync_sd_card_to_broker, daemon=True).start()
        else:
            self.btn_eject_sd.configure(
                text="📥 Cắm lại thẻ MicroSD vào mạch ESP32-S3",
                fg_color="#16a34a",
            )
            self._log_serial(
                "[SD CARD] ĐÃ RÚT THẺ MICROSD, có thể sửa lậu nhiệt độ bản ghi SD ngay bên dưới."
            )
            self._refresh_sd_ui_list()

    def _on_tamper_sd_record(self) -> None:
        if self.sd_inserted:
            self._log_serial(
                "[CẢNH BÁO] Hãy bấm nút 'Rút thẻ MicroSD ra khỏi mạch' trước khi sửa file CSV!"
            )
            return

        target_seq = self.entry_tamper_seq.get().strip()
        fake_temp_str = self.entry_tamper_temp.get().strip()
        try:
            fake_temp = float(fake_temp_str)
        except ValueError:
            self._log_serial("[LỖI] Giá trị nhiệt độ giả mạo không hợp lệ!")
            return

        rows = self._read_sd_records()
        modified = False
        for row in rows:
            if row["seq_no"] == target_seq or (not target_seq and row.get("synced") == "0"):
                old_temp = row["temp_c"]
                row["temp_c"] = f"{fake_temp:.2f}"
                row["alert_code"] = "0"
                row["synced"] = "0"
                modified = True
                self._log_serial(
                    f"[HACK SD CARD] Đã sửa lậu bản ghi Seq=#{row['seq_no']} trên thẻ SD: "
                    f"Nhiệt độ {old_temp}°C -> {fake_temp:.2f}°C (Giữ nguyên SHA-256 gốc)!"
                )
                break

        if not modified:
            self._log_serial("[HACK SD CARD] Không tìm thấy bản ghi phù hợp trong thẻ SD để sửa!")
        else:
            self._write_sd_records(rows)
            self._refresh_sd_ui_list()

    def _on_clear_sd_card(self) -> None:
        self._write_sd_records([])
        self._refresh_sd_ui_list()
        self._log_serial("[SD CARD] Đã xóa sạch toàn bộ dữ liệu trên thẻ nhớ MicroSD.")

    # =========================================================================
    # 6. CÁC SỰ KIỆN TƯƠNG TÁC TRÊN GUI
    # =========================================================================
    def _on_temp_slider_change(self, val: float) -> None:
        self.temp_c = round(float(val), 2)
        if 2.0 <= self.temp_c <= 8.0:
            status_txt = "ĐẠT CHUẨN GSP"
            color = "#4ade80"
        elif self.temp_c > 8.0:
            status_txt = "VƯỢT NGƯỠNG TRÊN (>8°C)!"
            color = "#f87171"
        else:
            status_txt = "VI PHẠM NGƯỠNG DƯỚI (<2°C)!"
            color = "#38bdf8"

        self.temp_lbl.configure(
            text=f"Nhiệt độ SHT30 hiện tại: {self.temp_c:+.2f} °C ({status_txt})",
            text_color=color,
        )
        self._update_oled_display()

    def _set_temp_preset(self, val: float) -> None:
        self.temp_slider.set(val)
        self._on_temp_slider_change(val)

    def _on_hum_slider_change(self, val: float) -> None:
        self.hum_pct = round(float(val), 2)
        self.hum_lbl.configure(text=f"Độ ẩm tương đối (RH): {self.hum_pct:.2f} %")
        self._update_oled_display()

    def _on_interval_slider_change(self, val: float) -> None:
        self.sample_interval_sec = round(float(val), 1)
        self.interval_lbl.configure(
            text=f"Chu kỳ đọc & bắn gói tin (Sample Interval): {self.sample_interval_sec:.1f} giây/gói"
        )

    def _on_press_btn_ack(self) -> None:
        self.alarm_muted = True
        self.relay_active = False
        self._log_serial("[HMI GPIO6] Người vận hành nhấn nút ACK -> Đã tắt tiếng còi hú tại chỗ!")
        self._update_oled_display()

    def _on_press_btn_test(self) -> None:
        self.alarm_muted = False
        self._set_temp_preset(9.80)
        self._log_serial("[HMI GPIO7] Nhấn nút TEST vượt ngưỡng -> Nhiệt độ vọt lên +9.80°C!")
        self.last_sample_ts = 0.0

    def _on_toggle_ac_power(self) -> None:
        self.ac_power_online = bool(self.ac_switch.get())
        if self.ac_power_online:
            self._log_serial("[POWER] Đã cấp lại điện lưới 5V VBUS -> Đang sạc lại Pin dự phòng 18650.")
        else:
            self._log_serial(
                "[POWER] MẤT ĐIỆN LƯỚI 5V! Mạch chuyển sang chạy Pin 18650 (Xả nhanh 0.08V/giây)..."
            )

    def _on_toggle_wifi(self) -> None:
        self.wifi_enabled = bool(self.wifi_switch.get())
        if self.wifi_enabled:
            self._log_serial("[WIFI] Đã bật lại Wi-Fi STA -> Đang khởi tạo lại phiên MQTT...")
            self.last_reconnect_try_ts = time.time()
            self._start_mqtt_session()
        else:
            self._log_serial(
                "[WIFI] ĐÃ NGẮT MẠNG WI-FI! Thiết bị chuyển sang lưu đệm Offline vào thẻ nhớ MicroSD."
            )
            self._stop_mqtt_session_offline()

    def _on_simulate_unplug_lwt(self) -> None:
        self.wifi_switch.deselect()
        self.wifi_enabled = False
        self._stop_mqtt_session_offline()
        self._log_serial("[LWT TEST] Đã giả lập mất kết nối đột ngột -> Đã bắn bản tin LWT OFFLINE lên Broker!")

    def _on_trigger_mitm_packet(self) -> None:
        self.mitm_attack_next = True
        self.last_sample_ts = 0.0
        self._log_serial("[ATTACK QUEUED] Chuẩn bị tiêm 1 gói tin bị sửa đổi (MITM Tampered) lên đường truyền...")

    # =========================================================================
    # 7. CẬP NHẬT GIAO DIỆN
    # =========================================================================
    def _update_oled_display(self) -> None:
        if not self.mcu_powered:
            self.oled_screen.configure(
                text="\n   [ MÀN HÌNH TẮT - MCU SẬP NGUỒN DO CẠN PIN 18650 (< 3.00V) ]   \n"
                "   -> Hãy bật lại công tắc 'Nguồn điện lưới 5V VBUS' để sạc lại! \n",
                text_color="#64748b",
            )
            self.relay_indicator.configure(text="CÒI, ĐÈN: TẮT", fg_color="#1e293b")
            return

        net_str = "WIFI+MQTT: ONLINE " if (self.wifi_enabled and self.mqtt_connected) else "WIFI: OFFLINE (SD)"
        sd_str = "SD: READY" if self.sd_inserted else "SD: EJECTED!"
        pwr_str = "5V-AC" if self.ac_power_online else "BAT-18650"
        alarm_str = "ALARM: VIOLATION!" if (self.temp_c > 8.0 or self.temp_c < 2.0) else "GSP: OK (+2..+8C)"

        oled_text = (
            f"┌──────────────────────────────────────────────────────────────┐\n"
            f"│ ID: {self.device_id:<14} │ {net_str:<18} │ {sd_str:<12} │\n"
            f"├──────────────────────────────────────────────────────────────┤\n"
            f"│ TEMP : {self.temp_c:+6.2f} °C   │ HUMID : {self.hum_pct:6.2f} %RH │ SEQ: #{self.seq_no:<7} │\n"
            f"│ POWER: {pwr_str:<10} │ BAT_V : {self.bat_v:6.2f} V   │ {alarm_str:<16} │\n"
            f"└──────────────────────────────────────────────────────────────┘"
        )
        self.oled_screen.configure(
            text=oled_text,
            text_color="#f87171" if (self.temp_c > 8.0 or self.temp_c < 2.0) else "#22d3ee",
        )

        if self.relay_active:
            self.relay_indicator.configure(
                text="CÒI HÚ, ĐÈN CHỚP: ĐANG BÁO ĐỘNG!",
                fg_color="#dc2626",
            )
        elif self.alarm_muted and (self.temp_c > 8.0 or self.temp_c < 2.0):
            self.relay_indicator.configure(
                text="CÒI HÚ: ĐÃ TẮT TIẾNG BỞI TÀI XẾ (ACK)",
                fg_color="#d97706",
            )
        else:
            self.relay_indicator.configure(
                text="CÒI, ĐÈN: TẮT",
                fg_color="#1e293b",
            )

    def _update_battery_ui(self) -> None:
        pct = max(0.0, min(1.0, (self.bat_v - 3.00) / (4.20 - 3.00)))
        self.bat_progress.set(pct)
        pct_int = int(pct * 100)

        if not self.mcu_powered:
            self.bat_status_lbl.configure(
                text=f"🪫 Pin 18650: {self.bat_v:.2f} V (0% - SẬP NGUỒN!)",
                text_color="#ef4444",
            )
        elif self.ac_power_online:
            self.bat_status_lbl.configure(
                text=f"Pin 18650: {self.bat_v:.2f} V ({pct_int}% - Đang cắm điện lưới 5V)",
                text_color="#34d399",
            )
        else:
            color = "#f59e0b" if self.bat_v > 3.40 else "#ef4444"
            self.bat_status_lbl.configure(
                text=f"⚠️ ĐANG CHẠY PIN DỰ PHÒNG 18650: {self.bat_v:.2f} V ({pct_int}% - Đang xả nhanh...)",
                text_color=color,
            )

    def _refresh_sd_ui_list(self) -> None:
        rows = self._read_sd_records()
        unsynced = [r for r in rows if r.get("synced") == "0"]

        state_str = "ĐANG CẮM" if self.sd_inserted else "ĐÃ RÚT (sửa trong file csv)"
        self.sd_status_lbl.configure(
            text=f"{state_str} ({len(unsynced)} Synced / {len(rows)})",
            text_color="#fde047" if self.sd_inserted else "#f87171",
        )

        self.sd_textbox.delete("1.0", "end")
        if not rows:
            self.sd_textbox.insert("end", "Thẻ nhớ MicroSD đang trống (Chưa có bản ghi Offline).\n")
            return

        header_line = f"{'SEQ_NO':<8} | {'TS_RTC':<11} | {'TEMP_C':<8} | {'HUM_%':<7} | {'ALERT':<5} | {'SYNCED':<6} | {'SHA256_HASH'}\n"
        self.sd_textbox.insert("end", header_line + "-" * 86 + "\n")
        for r in rows[-15:]:
            sync_tag = "YES" if r.get("synced") == "1" else "NO (*)"
            line = (
                f"#{r['seq_no']:<7} | {r['ts_rtc']:<11} | {r['temp_c']:>6}°C | "
                f"{r['hum_pct']:>5}% |   {r['alert_code']}   | {sync_tag:<6} | {r['sha256_hash'][:16]}...\n"
            )
            self.sd_textbox.insert("end", line)

        if unsynced:
            self.entry_tamper_seq.delete(0, "end")
            self.entry_tamper_seq.insert(0, unsynced[-1]["seq_no"])

    def _log_serial(self, message: str) -> None:
        ts_str = datetime.now().strftime("%H:%M:%S")
        line = f"[{ts_str}] {message}\n"

        def _append() -> None:
            self.serial_log.insert("end", line)
            self.serial_log.see("end")

        self.after(0, _append)

    def _on_close(self) -> None:
        self.running = False
        self._stop_mqtt_session_offline()
        time.sleep(0.25)
        self.destroy()


if __name__ == "__main__":
    app = ESP32S3GuiSimulator()
    app.mainloop()