FROM python:3.11-slim

# Ngăn Python tạo file .pyc và bật chế độ xuất log trực tiếp ra stdout/stderr
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

# Cài đặt các thư viện phụ thuộc trước để tận dụng Docker Layer Caching
COPY requirements.txt .
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy toàn bộ mã nguồn ứng dụng Backend
COPY ./app ./app

EXPOSE 8000

# Khởi chạy FastAPI server kèm WebSocket & MQTT Worker
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000", "--reload"]