#!/usr/bin/env python3
"""Compile and run Sprint 1 C++ logic checks on the host; no physical board needed."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory() as tmp:
        host = ROOT / 'firmware/test/host'
        for name in ('logic', 'hardware'):
            binary = str(Path(tmp) / ('sprint1_' + name))
            sources = [str(host / f'test_{name}.cpp')]
            includes = ['-I', str(ROOT / 'firmware/include')]
            if name == 'hardware':
                sources.append(str(ROOT / 'firmware/src/Sprint1Hardware.cpp'))
                includes += ['-I', str(host / 'mocks')]
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-g', *includes, *sources,
                            '-o', binary], check=True)
            subprocess.run([binary], check=True)


if __name__ == '__main__':
    main()
