#!/usr/bin/env python3
"""Live integration tests; start Docker Compose first. See docs/02_testing_and_handover/how_to_test.md."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import unittest
from urllib.error import HTTPError
from urllib.parse import urlencode
from urllib.request import urlopen
import uuid

import paho.mqtt.client as mqtt
import psycopg2
from websockets.sync.client import connect

ROOT = Path(__file__).resolve().parents[1]
API = os.getenv("TEST_API_URL", "http://localhost:8000")
WEB = os.getenv("TEST_WEB_URL", "http://localhost:5173")
WS = os.getenv("TEST_WS_URL", "ws://localhost:8000/ws/realtime")
DB = os.getenv("TEST_DATABASE_URL", "postgresql://gsp_user:gsp_pass@localhost:5432/gsp_coldchain")
HOST = os.getenv("TEST_MQTT_HOST", "localhost")
PORT = int(os.getenv("TEST_MQTT_PORT", "1883"))
OPTIONS = None


def api(path):
    try:
        with urlopen(API + path, timeout=10) as response:
            return response.status, json.load(response)
    except HTTPError as exc:
        return exc.code, json.load(exc)


def wait_for(predicate, timeout=20):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            last = predicate()
            if last:
                return last
        except Exception as exc:
            last = repr(exc)
        time.sleep(0.2)
    raise AssertionError(f"Condition not met after {timeout}s; last={last}")


def compose(*args):
    subprocess.run(["docker", "compose", *args], cwd=ROOT, check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=240)


class SystemTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = "TEST-" + uuid.uuid4().hex[:12]
        cls.counter = 0
        cls.base_ts = int(time.time()) - 3600
        with psycopg2.connect(DB) as conn:
            with conn.cursor() as cur:
                cur.execute("""INSERT INTO devices
                    (device_id, device_name, storage_type, ecdsa_pub_key, mqtt_token_hash)
                    VALUES (%s, 'Automated test device', 'MEDICAL_2_8C', 'TEST_KEY', 'TEST_TOKEN')""",
                            (cls.device,))
        cls.client = mqtt.Client(client_id="tests-" + uuid.uuid4().hex[:12])
        cls.client.connect(HOST, PORT, 30)
        cls.client.loop_start()
        wait_for(cls.client.is_connected)
        # REST readiness alone does not prove MQTT subscription readiness.
        def subscribed():
            with connect(WS, open_timeout=5) as ws:
                cls.publish("status", {"state": "OFFLINE"})
                cls.event(ws, lambda e: e["event"] == "status")
            return True
        wait_for(subscribed)

    @classmethod
    def tearDownClass(cls):
        cls.publish("status", {"state": "OFFLINE"})
        cls.client.disconnect()
        cls.client.loop_stop()
        # Keep immutable telemetry as evidence. Each run uses a new device ID.

    @classmethod
    def publish(cls, channel, payload, topic_device=None):
        body = payload if isinstance(payload, (str, bytes)) else json.dumps(payload)
        info = cls.client.publish(f"gsp/v1/{topic_device or cls.device}/{channel}", body, qos=1)
        info.wait_for_publish(timeout=10)
        if not info.is_published():
            raise AssertionError("MQTT PUBACK timeout")

    @classmethod
    def packet(cls, **overrides):
        cls.counter += 1
        p = dict(device_id=cls.device, seq_no=cls.counter,
                 ts_rtc=cls.base_ts + cls.counter, temp_c=4.25, hum_pct=62.5,
                 bat_v=4.1, alert_code=0, ecdsa_sig="DUMMY_SIG_OK_TEST")
        p.update(overrides)
        canonical = (f"{p['device_id']}|{p['seq_no']}|{p['ts_rtc']}|"
                     f"{p['temp_c']:.2f}|{p['hum_pct']:.2f}|{p['alert_code']}")
        p["sha256_hash"] = hashlib.sha256(canonical.encode()).hexdigest()
        return p

    @classmethod
    def event(cls, ws, predicate, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            e = json.loads(ws.recv(timeout=max(0.01, deadline - time.monotonic())))
            if e.get("data", {}).get("device_id") == cls.device and predicate(e):
                return e
        raise AssertionError("Expected WebSocket event not received")

    def send(self, packet, channel="telemetry"):
        with connect(WS) as ws:
            self.publish(channel, packet)
            return self.event(ws, lambda e: e.get("data", {}).get("seq_no") == packet["seq_no"])

    def rows(self):
        code, data = api(f"/api/v1/devices/{self.device}/telemetry?limit=500")
        self.assertEqual(code, 200)
        return data["items"]

    def test_01_api_devices(self):
        code, data = api("/api/v1/devices")
        self.assertEqual(code, 200)
        self.assertEqual(data["count"], len(data["items"]))
        self.assertIn(self.device, [d["device_id"] for d in data["items"]])

    def test_02_telemetry_database_websocket(self):
        p = self.packet()
        started = time.monotonic()
        e = self.send(p)
        self.assertTrue(e["inserted"])
        self.assertTrue(e["data"]["sig_verified"])
        row = next(r for r in self.rows() if r["seq_no"] == p["seq_no"])
        self.assertEqual(row["sha256_hash"], p["sha256_hash"])
        self.assertLess(time.monotonic() - started, 2, "Local MQTT → DB → WS exceeded 2s")

    def test_03_alert_persisted_and_broadcast(self):
        p = self.packet(temp_c=9.8, alert_code=1)
        p.update(message="Automated high-temperature test", severity="CRITICAL")
        with connect(WS) as ws:
            self.publish("alert", p)
            e = self.event(ws, lambda e: e["event"] == "alert")
        self.assertIsNotNone(e["data"]["alert_id"])
        with psycopg2.connect(DB) as conn:
            with conn.cursor() as cur:
                cur.execute("SELECT temp_c FROM alerts WHERE alert_id=%s", (e["data"]["alert_id"],))
                self.assertAlmostEqual(float(cur.fetchone()[0]), 9.8)

    def test_04_sync_duplicate(self):
        p = self.packet()
        first = self.send(p, "sync")
        second = self.send(p, "sync")
        self.assertTrue(first["inserted"])
        self.assertFalse(second["inserted"])
        self.assertTrue(first["data"]["is_backfill"])
        self.assertEqual(sum(r["seq_no"] == p["seq_no"] for r in self.rows()), 1)

    def test_05_modified_temperature_is_tampered(self):
        p = self.packet(temp_c=9.8, alert_code=1)
        p.update(temp_c=4.5, alert_code=0)
        self.assertFalse(self.send(p)["data"]["sig_verified"])
        _, result = api(f"/api/v1/devices/{self.device}/verify-chain")
        self.assertFalse(result["chain_intact"])
        self.assertGreaterEqual(result["tampered_records"], 1)

    def test_06_online_offline(self):
        for state in ("ONLINE", "OFFLINE"):
            with connect(WS) as ws:
                self.publish("status", {"state": state})
                e = self.event(ws, lambda e: e["event"] == "status")
            self.assertEqual(e["data"]["is_online"], state == "ONLINE")
            _, data = api("/api/v1/devices")
            d = next(d for d in data["items"] if d["device_id"] == self.device)
            self.assertEqual(d["is_online"], state == "ONLINE")

    def test_07_actual_broker_lwt(self):
        c = mqtt.Client(client_id="lwt-test-" + uuid.uuid4().hex[:8])
        c.will_set(f"gsp/v1/{self.device}/status", '{"state":"OFFLINE"}', qos=1)
        try:
            c.connect(HOST, PORT, 5)
            c.loop_start()
            wait_for(c.is_connected)
            with connect(WS) as ws:
                self.publish("status", {"state": "ONLINE"})
                self.event(ws, lambda e: e["event"] == "status" and e["data"]["is_online"])
                # Close transport without an MQTT DISCONNECT so the broker emits the will.
                c.socket().shutdown(socket.SHUT_RDWR)
                c.loop_stop()
                e = self.event(ws, lambda e: e["event"] == "status" and not e["data"]["is_online"])
                self.assertEqual(e["data"]["state"], "OFFLINE")
        finally:
            c.disconnect()
            c.loop_stop()

    def test_08_malformed_messages_do_not_stop_worker(self):
        for bad in ("not-json", "[]", "null", "{}", '{"temp_c":"abc"}'):
            self.publish("telemetry", bad)
        self.assertTrue(self.send(self.packet())["inserted"])

    def test_09_pagination_and_validation(self):
        for _ in range(3):
            self.send(self.packet())
        _, first = api(f"/api/v1/devices/{self.device}/telemetry?limit=2")
        _, second = api(f"/api/v1/devices/{self.device}/telemetry?" + urlencode({"limit": 2, "cursor": first["next_cursor"]}))
        self.assertTrue(all(a["ts_rtc"] > b["ts_rtc"] for a in first["items"] for b in second["items"]))
        for query, expected in (("limit=0", 422), ("limit=501", 422), ("cursor=invalid", 400)):
            self.assertEqual(api(f"/api/v1/devices/{self.device}/telemetry?{query}")[0], expected)

    def test_10_immutable_update_delete(self):
        self.send(self.packet())
        for sql in ("UPDATE telemetry_logs SET temp_c=5 WHERE device_id=%s",
                    "DELETE FROM telemetry_logs WHERE device_id=%s"):
            conn = psycopg2.connect(DB)
            try:
                with conn.cursor() as cur:
                    with self.assertRaises(psycopg2.Error) as error:
                        cur.execute(sql, (self.device,))
                    self.assertEqual(error.exception.pgcode, "42501")
            finally:
                conn.rollback()
                conn.close()

    def test_11_udp_discovery(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(5)
            sock.sendto(b"WHO_IS_IOT_SERVER", ("127.0.0.1", 8888))
            self.assertEqual(sock.recvfrom(1024)[0], b"IOT_SERVER_HERE")

    def test_12_burst_100_unique_plus_100_duplicates(self):
        packets = [self.packet() for _ in range(100)]
        for p in packets + packets:
            self.publish("sync", p)
        expected = {p["seq_no"] for p in packets}
        rows = wait_for(lambda: (r if expected <= {v["seq_no"] for v in r} else None) if (r := self.rows()) else None, 30)
        self.assertEqual(sum(r["seq_no"] in expected for r in rows), 100)

    def test_13_forged_signature_must_be_rejected(self):
        p = self.packet(ecdsa_sig="X" * 64)
        self.assertFalse(self.send(p)["data"]["sig_verified"], "Arbitrary 64-character signature accepted as verified")

    def test_14_modified_battery_must_be_detected(self):
        p = self.packet()
        p["bat_v"] = 3.2
        self.assertFalse(self.send(p)["data"]["sig_verified"], "Modified battery value is outside canonical hash")

    def test_15_topic_payload_identity_must_match(self):
        p = self.packet()
        with connect(WS) as ws:
            self.publish("telemetry", p, topic_device="OTHER-DEVICE")
            # Ordered barrier: once status arrives, the preceding telemetry has been processed.
            self.publish("status", {"state": "OFFLINE"})
            self.event(ws, lambda e: e["event"] == "status")
        self.assertNotIn(p["seq_no"], [r["seq_no"] for r in self.rows()], "Topic identity mismatch was stored")

    def test_16_backend_stop_queued_messages_replayed(self):
        if not OPTIONS.with_restarts:
            self.skipTest("Use --with-restarts to exercise service outages")
        packets = [self.packet() for _ in range(10)]
        try:
            compose("stop", "backend")
            for p in packets:
                self.publish("telemetry", p)
        finally:
            compose("start", "backend")
        expected = {p["seq_no"] for p in packets}
        wait_for(lambda: expected <= {r["seq_no"] for r in self.rows()}, 40)

    def test_17_database_outage_must_not_lose_messages(self):
        if not OPTIONS.with_restarts:
            self.skipTest("Use --with-restarts to exercise service outages")
        p = self.packet()
        try:
            compose("stop", "--timeout", "180", "timescaledb")
            with connect(WS) as ws:
                self.publish("telemetry", p)
                self.publish("status", {"state": "OFFLINE"})
                # Status event confirms worker passed the failed DB insert before DB returns.
                self.event(ws, lambda e: e["event"] == "status")
        finally:
            compose("start", "timescaledb")
        wait_for(lambda: api("/api/v1/devices")[0] == 200, 120)
        # A fresh successful insert is a processing barrier after recovery.
        self.assertTrue(self.send(self.packet())["inserted"])
        self.assertIn(p["seq_no"], [r["seq_no"] for r in self.rows()], "QoS1 packet acknowledged despite failed DB insert; no replay")

    def browser_case(self, scenario):
        if not OPTIONS.browser:
            self.skipTest("Use --browser for real Chromium checks")
        from playwright.sync_api import sync_playwright, expect
        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            try:
                page = browser.new_page(viewport={"width": 1440, "height": 1000})
                errors = []
                page.on("pageerror", lambda exc: errors.append(str(exc)))
                page.goto(WEB, wait_until="domcontentloaded")
                expect(page.get_by_text("GSP Cold-Chain IoT Monitor", exact=True)).to_be_visible()
                expect(page.get_by_text("WS Realtime Connected", exact=True)).to_be_visible()
                default = "GSP-TRUCK-01"
                _, previous = api(f"/api/v1/devices/{default}/telemetry?limit=1")
                if previous["items"]:
                    wait_for(lambda: int(time.time()) > previous["items"][0]["ts_epoch"], 20)
                p = self.packet(device_id=default, ts_rtc=int(time.time()), temp_c=4.25)
                self.publish("telemetry", p, topic_device=default)
                expect(page.locator("tbody tr").first).to_contain_text(f"#{p['seq_no']}")
                expect(page.locator("tbody tr").first).to_contain_text("4.25°C")
                failure = None
                if scenario == "normal":
                    expect(page.get_by_text("THIẾT BỊ: ONLINE", exact=True)).to_be_visible()
                elif scenario == "alert":
                    wait_for(lambda: int(time.time()) > p["ts_rtc"])
                    p = self.packet(device_id=default, ts_rtc=int(time.time()), temp_c=9.8, alert_code=1)
                    self.publish("telemetry", p, topic_device=default)
                    expect(page.get_by_text("VƯỢT NGƯỠNG", exact=True)).to_be_visible()
                elif scenario == "tampered":
                    wait_for(lambda: int(time.time()) > p["ts_rtc"])
                    p = self.packet(device_id=default, ts_rtc=int(time.time()), temp_c=9.8)
                    p["temp_c"] = 5.5
                    self.publish("telemetry", p, topic_device=default)
                    expect(page.locator("tbody tr").first).to_contain_text("TAMPERED")
                    page.get_by_role("button", name="Kiểm toán chuỗi chữ ký GSP").click()
                    expect(page.get_by_text("COMPROMISED", exact=True)).to_be_visible()
                elif scenario == "old-sync":
                    old = self.packet(device_id=default, ts_rtc=p["ts_rtc"] - 600, temp_c=3.25)
                    self.publish("sync", old, topic_device=default)
                    expect(page.locator("tbody tr").filter(has_text=f"#{old['seq_no']}")).to_be_visible()
                    failure = (page.locator("tbody tr").first.inner_text(), f"#{p['seq_no']}",
                               "Old backfill replaced latest reading on dashboard")
                elif scenario == "duplicate":
                    dup = copy.deepcopy(p)
                    dup["temp_c"] = 7.75
                    self.publish("telemetry", dup, topic_device=default)
                    # A status barrier ensures the preceding WS event reached the browser.
                    self.publish("status", {"state": "OFFLINE"}, topic_device=default)
                    expect(page.get_by_text("THIẾT BỊ: OFFLINE (LWT)", exact=True)).to_be_visible()
                    _, records = api(f"/api/v1/devices/{default}/telemetry?limit=500")
                    stored = next(r for r in records["items"] if r["ts_epoch"] == p["ts_rtc"])
                    failure = (page.locator("tbody tr").first.inner_text(), f"{stored['temp_c']:.2f}°C",
                               "UI accepted inserted=false duplicate; differs from DB")
                screenshot = OPTIONS.output.parent / f"ui-{scenario}.png"
                page.screenshot(path=str(screenshot), full_page=True)
                self.assertEqual(errors, [])
                if failure:
                    self.assertIn(failure[1], failure[0], failure[2])
            finally:
                self.publish("status", {"state": "OFFLINE"}, topic_device="GSP-TRUCK-01")
                browser.close()

    def test_18_browser_normal(self):
        self.browser_case("normal")

    def test_19_browser_alert(self):
        self.browser_case("alert")

    def test_20_browser_tampered_and_audit(self):
        self.browser_case("tampered")

    def test_21_browser_old_sync_preserves_latest(self):
        self.browser_case("old-sync")

    def test_22_browser_duplicate_preserves_database_value(self):
        self.browser_case("duplicate")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--with-restarts", action="store_true", help="Temporarily stop/start backend and DB")
    parser.add_argument("--browser", action="store_true", help="Requires playwright install chromium; writes sample data to GSP-TRUCK-01")
    parser.add_argument("--output", type=Path, default=ROOT / "playground/test-results.json")
    OPTIONS = parser.parse_args()
    OPTIONS.output.parent.mkdir(parents=True, exist_ok=True)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SystemTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = dict(timestamp_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                  device_id=getattr(SystemTests, "device", None), tests=result.testsRun,
                  passed=result.testsRun-len(result.failures)-len(result.errors)-len(result.skipped),
                  failures=[dict(test=str(t), traceback=s) for t, s in result.failures],
                  errors=[dict(test=str(t), traceback=s) for t, s in result.errors],
                  skipped=[dict(test=str(t), reason=s) for t, s in result.skipped])
    OPTIONS.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"JSON report: {OPTIONS.output}")
    raise SystemExit(0 if result.wasSuccessful() else 1)
