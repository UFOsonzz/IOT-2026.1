#!/bin/bash
echo "=== Khởi chạy GSP Simulator trên macOS/Linux ==="

cd "$(dirname "$0")"

# 1. Tạo môi trường ảo nếu chưa tồn tại
if [ ! -d ".venv" ]; then
    echo "-> Đang tạo môi trường ảo .venv..."
    python3 -m venv .venv
fi

# 2. Kích hoạt môi trường ảo
echo "-> Đang kích hoạt môi trường ảo..."
source .venv/bin/activate

# 3. Cài đặt thư viện
echo "-> Đang cài đặt thư viện paho-mqtt..."
pip install --upgrade pip --quiet
pip install paho-mqtt --quiet

# 4. Chạy kịch bản giả lập
echo "-> Đang chạy Simulator..."
python tools/dummy_simulator.py