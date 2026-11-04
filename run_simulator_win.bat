@echo off
title GSP Simulator
echo === Khoi chay GSP Simulator tren Windows ===

REM Chuyển đến thư mục gốc của dự án (nơi chứa file này)
cd /d "%~dp0"

REM 1. Tạo môi trường ảo nếu chưa tồn tại
if not exist ".venv\" (
    echo -^> Dang tao moi truong ao .venv...
    python -m venv .venv
)

REM 2. Kích hoạt môi trường ảo
echo -^> Dang kich hoat moi truong ao...
call .venv\Scripts\activate

REM 3. Cài đặt thư viện
echo -^> Dang cai dat thu vien paho-mqtt...
python -m pip install --upgrade pip --quiet
pip install paho-mqtt customtkinter --quiet

REM 4. Chạy kịch bản giả lập
echo -^> Dang chay Simulator...
python tools\gui_simulator.py

pause