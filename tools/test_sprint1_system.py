#!/usr/bin/env python3
"""Sprint 1 regression using an isolated simulated node; requires Docker Compose."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
import traceback
import urllib.error
import urllib.request
import uuid

from dummy_simulator import GSPEdgeSimulator

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser', action='store_true')
    parser.add_argument('--evidence-dir', type=Path, default=ROOT /
                        'docs/02_testing_and_handover/evidence' /
                        ('sprint1-' + datetime.now().strftime('%Y-%m-%d')))
    args = parser.parse_args()
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    device = 'GSP-S1-' + uuid.uuid4().hex[:12]
    results = {'device_id': device, 'source': 'simulator, not physical ESP32',
               'started_at': datetime.now(timezone.utc).isoformat(), 'checks': []}
    sim = GSPEdgeSimulator('127.0.0.1', 1883, device)
    sim.seq_no = 1000

    def check(description, predicate, timeout=30):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                if predicate():
                    results['checks'].append({'name': description, 'status': 'PASS'})
                    print('PASS', description, flush=True)
                    return
            except (urllib.error.URLError, TimeoutError, ConnectionError):
                pass
            time.sleep(0.2)
        results['checks'].append({'name': description, 'status': 'FAIL'})
        raise AssertionError(description)

    def api(path):
        with urllib.request.urlopen('http://127.0.0.1:8000' + path, timeout=5) as response:
            return json.load(response)

    def db_code(code):
        return subprocess.run(['docker', 'compose', 'exec', '-T', 'backend', 'python',
                               '-c', code, device], cwd=ROOT, check=True,
                              capture_output=True, text=True, timeout=30).stdout

    def node():
        return next((d for d in api('/api/v1/devices')['items'] if d['device_id'] == device), {})

    def rows():
        return api(f'/api/v1/devices/{device}/telemetry?limit=500')['items']

    def publish(value, channel='telemetry'):
        info = sim.client.publish(f'gsp/v1/{device}/{channel}', json.dumps(value), qos=1)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise TimeoutError('MQTT publish timed out')

    def payload(temp, humidity, stamp, alert=0):
        value, _ = sim.create_signed_payload(temp, humidity, alert, ts_override=stamp)
        value.update(bat_v=0.0, ecdsa_sig='DUMMY_SIG_OK_ESP32_SECP256R1')
        return value

    connected = False
    try:
        db_code('''import sys
from app.db import get_connection
with get_connection() as conn:
    with conn.cursor() as cur:
        cur.execute("""INSERT INTO devices
            (device_id, device_name, storage_type, ecdsa_pub_key, mqtt_token_hash)
            VALUES (%s, %s, 'MEDICAL_2_8C', 'DUMMY_PUBLIC_KEY', 'test_only')""",
            (sys.argv[1], 'Sprint 1 automated test (simulation)'))
''')
        check('Isolated simulated device is registered', lambda: bool(node()))
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discovery:
            discovery.settimeout(5)
            discovery.sendto(b'WHO_IS_IOT_SERVER', ('127.0.0.1', 8888))
            reply, _ = discovery.recvfrom(100)
        check('UDP discovery returns IOT_SERVER_HERE', lambda: reply == b'IOT_SERVER_HERE')
        sim.connect()
        connected = True
        check('MQTT retained ONLINE reaches device API', lambda: node().get('is_online'))
        base = int(time.time()) - 10
        normal = payload(4.5, 62.5, base)
        publish(normal)
        check('ESP32-format normal telemetry persists with valid hash and bat_v=0',
              lambda: any(r['seq_no'] == normal['seq_no'] and r['sig_verified']
                          and r['bat_v'] == 0 and not r['is_backfill'] for r in rows()))
        alert = payload(9.5, 62.5, base + 1, 1)
        publish(alert)
        publish({**alert, 'severity': 'CRITICAL', 'message': 'Sprint 1 simulated test'}, 'alert')
        check('GSP test 9.50C persists with alert_code=1',
              lambda: any(r['seq_no'] == alert['seq_no'] and r['alert_code'] == 1
                          and r['sig_verified'] for r in rows()))
        backfill = [payload(4.1 + i * 0.25, 61.5, base - 180 + i * 60) for i in range(3)]
        for record in backfill:
            publish(record, 'sync')
        publish(backfill[-1], 'sync')
        check('3 backfill records persist; duplicate is removed',
              lambda: len([r for r in rows() if r['is_backfill']]) == 3)
        tamper = payload(9.8, 64, base + 2, 1)
        tamper.update(temp_c=4.5, alert_code=0)
        publish(tamper)
        check('Altered payload is stored as TAMPERED',
              lambda: any(r['seq_no'] == tamper['seq_no'] and not r['sig_verified'] for r in rows()))
        check('Exactly six unique telemetry records persisted', lambda: len(rows()) == 6)
        sim.toggle_status('OFFLINE')
        check('Explicit OFFLINE reaches API', lambda: node().get('is_online') is False)
        sim.toggle_status('ONLINE')
        check('ONLINE recovery reaches API', lambda: node().get('is_online') is True)
        child = '''import os, sys
import paho.mqtt.client as mqtt
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id='lwt-' + sys.argv[1])
client.will_set('gsp/v1/' + sys.argv[1] + '/status', '{"state":"OFFLINE"}', qos=1, retain=True)
client.connect('127.0.0.1', 1883, keepalive=10)
client.loop_start()
info = client.publish('gsp/v1/' + sys.argv[1] + '/status', '{"state":"ONLINE"}', qos=1, retain=True)
info.wait_for_publish(timeout=5)
assert info.is_published()
os._exit(0)
'''
        subprocess.run([sys.executable, '-c', child, device], check=True, timeout=15)
        check('Actual broker LWT after process exit marks device OFFLINE',
              lambda: node().get('is_online') is False)
        sim.toggle_status('ONLINE')
        check('Device recovers ONLINE after LWT test', lambda: node().get('is_online') is True)
        db_result = json.loads(db_code('''import json, sys
from app.db import get_connection
conn = get_connection()
checks = {}
with conn.cursor() as cur:
    cur.execute('SELECT COUNT(*) AS n FROM alerts WHERE device_id=%s', (sys.argv[1],))
    checks['alert_persisted'] = cur.fetchone()['n'] == 1
    for verb in ('UPDATE', 'DELETE'):
        try:
            sql = ('UPDATE telemetry_logs SET temp_c=5 WHERE device_id=%s' if verb == 'UPDATE'
                   else 'DELETE FROM telemetry_logs WHERE device_id=%s')
            cur.execute(sql, (sys.argv[1],))
            checks[verb] = False
        except Exception as exc:
            checks[verb] = getattr(exc, 'pgcode', None) == '42501'
        finally:
            conn.rollback()
conn.close()
print(json.dumps(checks))
'''))
        check('Alert channel persists in alerts table', lambda: db_result['alert_persisted'])
        check('Database rejects UPDATE and DELETE with SQLSTATE 42501',
              lambda: db_result['UPDATE'] and db_result['DELETE'])
        chain = api(f'/api/v1/devices/{device}/verify-chain')
        check('Chain audit detects exactly one tampered record',
              lambda: chain['tampered_records'] == 1 and not chain['chain_intact'])
        results['chain'] = chain

        if args.browser:
            from playwright.sync_api import sync_playwright, expect
            with sync_playwright() as pw:
                browser = pw.chromium.launch(headless=True)
                page = browser.new_page(viewport={'width': 1440, 'height': 1000})
                errors, events = [], []
                page.on('pageerror', lambda exc: errors.append(str(exc)))

                def on_frame(frame):
                    try:
                        events.append(json.loads(frame))
                    except (ValueError, TypeError):
                        pass

                page.on('websocket', lambda ws: ws.on('framereceived', on_frame))
                page.goto('http://127.0.0.1:5173/?device=' + device, wait_until='domcontentloaded')
                expect(page.get_by_text('WS Realtime Connected', exact=True)).to_be_visible(timeout=20000)
                expect(page.locator('tbody tr')).to_have_count(6)
                fresh = payload(6.73, 61.23, int(time.time()))
                publish(fresh)
                expect(page.locator('span.text-3xl').filter(has_text='6.73°C')).to_be_visible(timeout=15000)
                expect(page.get_by_text('CHƯA ĐO PIN', exact=True)).to_be_visible()
                check('Browser receives NEW MQTT value via WebSocket; no false low-battery alarm', lambda: True)
                old = payload(3.21, 60, base - 500)
                publish(old, 'sync')
                expect(page.locator('tbody tr')).to_have_count(8)
                expect(page.locator('span.text-3xl').filter(has_text='6.73°C')).to_be_visible()
                check('Old sync does not overwrite current browser temperature', lambda: True)
                duplicate = payload(7.77, 60, fresh['ts_rtc'])
                publish(duplicate)
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline and not any(
                    e.get('inserted') is False and e.get('data', {}).get('seq_no') == duplicate['seq_no'] for e in events
                ):
                    page.wait_for_timeout(100)
                assert any(e.get('inserted') is False and e.get('data', {}).get('seq_no') == duplicate['seq_no'] for e in events)
                expect(page.locator('tbody tr')).to_have_count(8)
                expect(page.locator('span.text-3xl').filter(has_text='6.73°C')).to_be_visible()
                check('Rejected duplicate cannot change browser state', lambda: True)
                page.screenshot(path=str(evidence / 'dashboard.png'), full_page=True)
                check('Browser has no JavaScript page errors', lambda: not errors)
                results['browser'] = {'page_errors': errors, 'received_events': events}
                browser.close()
        results['telemetry'] = rows()
        results['status'] = 'PASS'
    except Exception as exc:
        results['status'] = 'FAIL'
        results['error'] = str(exc)
        traceback.print_exc()
    finally:
        if connected:
            try:
                sim.disconnect_gracefully()
            except Exception as exc:
                results['cleanup_error'] = str(exc)
                results['status'] = 'FAIL'
        results['finished_at'] = datetime.now(timezone.utc).isoformat()
        (evidence / 'system_results.json').write_text(json.dumps(results, indent=2, ensure_ascii=False))
        print('Evidence:', evidence, flush=True)
    return 0 if results['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
