#!/usr/bin/env python3
"""List USB ports, set DS3231 to PC UTC, and inspect Sprint 1 firmware."""
import argparse
from contextlib import nullcontext
from pathlib import Path
import time

import serial
from serial.tools import list_ports


def show_ports():
    found = False
    for port in list_ports.comports():
        if port.vid is not None or not port.device.startswith('/dev/ttyS'):
            found = True
            print(f'{port.device}: {port.description}')
    if not found:
        print('No USB serial device detected. Connect the ESP32 with a data cable and run --list again.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--port', help='/dev/ttyUSB0, /dev/ttyACM0 or COM3')
    parser.add_argument('--set-time', action='store_true', help='Write PC UTC into DS3231')
    parser.add_argument('--command', action='append', default=[], help='STATUS, SCAN, WAKE, ACK, TEST')
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--status-interval', type=float, default=5,
                        help='Repeat STATUS every N seconds; 0 disables it')
    parser.add_argument('--log', type=Path, help='Save actual received Serial lines as evidence')
    args = parser.parse_args()
    if args.list:
        show_ports()
        return
    if not args.port:
        parser.error('Choose --port, or use --list first')
    if args.seconds <= 0 or args.status_interval < 0:
        parser.error('--seconds must be positive; --status-interval must be nonnegative')
    if args.log:
        args.log.parent.mkdir(parents=True, exist_ok=True)
    try:
        with (args.log.open('w', encoding='utf-8') if args.log else nullcontext()) as log, \
                serial.Serial(args.port, 115200, timeout=0.2, write_timeout=2) as link:
            # Allow the USB serial connection to settle before sending commands.
            time.sleep(2)
            if args.set_time:
                link.write(f'TIME {int(time.time())}\n'.encode())
            for command in args.command or ['STATUS']:
                link.write((command + '\n').encode('ascii'))
            deadline = time.monotonic() + args.seconds
            next_status = time.monotonic() + args.status_interval
            time_set_ok = False
            while time.monotonic() < deadline:
                if args.status_interval and time.monotonic() >= next_status:
                    link.write(b'STATUS\n')
                    next_status = time.monotonic() + args.status_interval
                line = link.readline()
                if line:
                    message = line.decode('utf-8', errors='replace').rstrip()
                    time_set_ok |= '[RTC SET OK]' in message
                    print(message, flush=True)
                    if log:
                        log.write(message + '\n')
                        log.flush()
            if args.set_time and not time_set_ok:
                parser.exit(1, 'RTC set was not acknowledged. Check the Serial log and DS3231 wiring.\n')
    except (serial.SerialException, OSError) as exc:
        parser.exit(1, f'USB error: {exc}\nClose Serial Monitor/gateway and check port/permissions.\n')


if __name__ == '__main__':
    main()
