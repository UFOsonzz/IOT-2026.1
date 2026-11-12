#!/usr/bin/env python3
"""Run the complete Sprint 1 software checks and save real logs; no USB required."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser', action='store_true', help='Also verify Chromium rendering/WebSocket')
    parser.add_argument('--evidence-dir', type=Path, default=ROOT /
                        'docs/02_testing_and_handover/evidence' /
                        ('sprint1-' + datetime.now().strftime('%Y-%m-%d')))
    args = parser.parse_args()
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    pio = Path(sys.executable).parent / 'pio'
    pio_cmd = str(pio) if pio.exists() else 'pio'
    steps = [
        ('compose_config', ['docker', 'compose', 'config', '--quiet'], ROOT, 30),
        ('host_tests', [sys.executable, 'tools/test_sprint1.py'], ROOT, 120),
        ('firmware_build', [pio_cmd, 'run', '-d', 'firmware', '-e', 'esp32dev'], ROOT, 600),
        ('web_build', ['npm', 'run', 'build'], ROOT / 'web', 180),
        ('usb_ports', [sys.executable, 'tools/sprint1_serial.py', '--list'], ROOT, 30),
        ('system_tests', [sys.executable, '-u', 'tools/test_sprint1_system.py',
                          '--evidence-dir', str(evidence)] + (['--browser'] if args.browser else []), ROOT, 240),
    ]
    report = {'started_at': datetime.now(timezone.utc).isoformat(), 'browser_requested': args.browser,
              'hardware_acceptance': 'PENDING: ESP32 not connected; no hardware results inferred', 'checks': []}
    for name, command, cwd, timeout in steps:
        print('RUN', name, flush=True)
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, timeout=timeout)
            output, code = result.stdout, result.returncode
        except (OSError, subprocess.TimeoutExpired) as exc:
            output, code = str(exc), 1
            if isinstance(exc, subprocess.TimeoutExpired) and exc.output:
                output += '\n' + (exc.output.decode(errors='replace') if isinstance(exc.output, bytes) else exc.output)
        (evidence / (name + '.log')).write_text(output, encoding='utf-8')
        report['checks'].append({'name': name, 'command': command, 'exit_code': code,
                                 'seconds': round(time.monotonic() - start, 2),
                                 'status': 'PASS' if code == 0 else 'FAIL'})
        print(('PASS' if code == 0 else 'FAIL'), name, flush=True)
        if code:
            print(output[-3500:], flush=True)
    binary = ROOT / 'firmware/.pio/build/esp32dev/firmware.bin'
    if binary.exists():
        report['firmware'] = {'path': str(binary.relative_to(ROOT)), 'bytes': binary.stat().st_size,
                              'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
    report['status'] = 'PASS' if all(c['exit_code'] == 0 for c in report['checks']) else 'FAIL'
    report['finished_at'] = datetime.now(timezone.utc).isoformat()
    (evidence / 'software_results.json').write_text(json.dumps(report, indent=2, ensure_ascii=False))
    print('Software:', report['status'], '| Hardware: PENDING', flush=True)
    print('Evidence:', evidence, flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
