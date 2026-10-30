import asyncio
import logging
from contextlib import asynccontextmanager
from typing import Optional

from fastapi import FastAPI, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.encoders import jsonable_encoder
from fastapi.middleware.cors import CORSMiddleware

from app.db import get_device_telemetry, get_devices, verify_device_chain
from app.mqtt_worker import GSPMqttWorker

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("gsp.api")


class RealtimeConnectionManager:
    """Quản lý danh sách các kết nối WebSocket đang mở từ giao diện React Dashboard."""

    def __init__(self) -> None:
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket) -> None:
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info("WebSocket client đã kết nối. Tổng số kết nối: %d", len(self.active_connections))

    def disconnect(self, websocket: WebSocket) -> None:
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            logger.info("WebSocket client đã ngắt kết nối. Còn lại: %d", len(self.active_connections))

    async def broadcast(self, message: dict) -> None:
        if not self.active_connections:
            return
        encoded_msg = jsonable_encoder(message)
        stale_connections: list[WebSocket] = []
        for connection in self.active_connections:
            try:
                await connection.send_json(encoded_msg)
            except Exception:
                stale_connections.append(connection)
        for stale in stale_connections:
            self.disconnect(stale)


ws_manager = RealtimeConnectionManager()
mqtt_event_queue: asyncio.Queue = asyncio.Queue()


async def broadcast_worker(queue: asyncio.Queue) -> None:
    """Tác vụ nền đọc sự kiện từ asyncio.Queue và phát trực tiếp tới mọi WebSocket clients."""
    logger.info("Khởi chạy tác vụ nền phát dữ liệu Realtime WebSocket...")
    while True:
        try:
            event = await queue.get()
            await ws_manager.broadcast(event)
            queue.task_done()
        except asyncio.CancelledError:
            break
        except Exception as exc:
            logger.error("Lỗi trong tác vụ broadcast_worker: %s", exc)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Vòng đời ứng dụng: khởi chạy MQTT Worker và trình phát WebSocket khi bật server."""
    loop = asyncio.get_running_loop()
    mqtt_worker = GSPMqttWorker(event_queue=mqtt_event_queue, loop=loop)
    mqtt_worker.start()

    broadcaster_task = asyncio.create_task(broadcast_worker(mqtt_event_queue))
    try:
        yield
    finally:
        broadcaster_task.cancel()
        mqtt_worker.stop()


app = FastAPI(
    title="GSP Cold Chain IoT Backend API",
    description=(
        "Hệ thống Backend giám sát vi khí hậu kho lạnh & xe tải đông lạnh tuân thủ GSP. "
        "Kiến trúc một chiều (Unidirectional) và bất biến (Append-Only)."
    ),
    version="1.0.0",
    lifespan=lifespan,
)

# Bật CORS cho phép ứng dụng Web Frontend (Vite/React) truy cập
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["GET", "OPTIONS"],
    allow_headers=["*"],
)


@app.get("/api/v1/devices")
def api_get_devices():
    """
    Trả về danh sách thiết bị giám sát cùng thông số đo vi khí hậu mới nhất.
    """
    try:
        devices = get_devices()
        return jsonable_encoder({
            "count": len(devices),
            "items": devices,
        })
    except Exception as exc:
        logger.error("Lỗi truy vấn danh sách thiết bị: %s", exc)
        raise HTTPException(status_code=500, detail="Không thể truy vấn danh sách thiết bị") from exc


@app.get("/api/v1/devices/{device_id}/telemetry")
def api_get_device_telemetry(
    device_id: str,
    limit: int = Query(default=50, ge=1, le=500, description="Số bản ghi tối đa trên mỗi trang"),
    cursor: Optional[str] = Query(default=None, description="Con trỏ thời gian ts_rtc (ISO-8601 hoặc Unix epoch)"),
):
    """
    Trả về lịch sử bản ghi đo của thiết bị theo phân trang con trỏ (Cursor-based Pagination).
    """
    try:
        records = get_device_telemetry(device_id=device_id, limit=limit, cursor_ts=cursor)
        next_cursor = None
        if len(records) == limit and records:
            last_ts = records[-1].get("ts_rtc")
            next_cursor = last_ts.isoformat() if hasattr(last_ts, "isoformat") else str(last_ts)

        return jsonable_encoder({
            "device_id": device_id,
            "count": len(records),
            "next_cursor": next_cursor,
            "items": records,
        })
    except ValueError as val_err:
        raise HTTPException(status_code=400, detail=str(val_err)) from val_err
    except Exception as exc:
        logger.error("Lỗi truy vấn lịch sử đo của thiết bị %s: %s", device_id, exc)
        raise HTTPException(status_code=500, detail="Không thể truy vấn lịch sử dữ liệu đo") from exc


@app.get("/api/v1/devices/{device_id}/verify-chain")
def api_verify_device_chain(device_id: str):
    """
    Kiểm tra tính toàn vẹn chữ ký số (SHA-256 + ECDSA) của toàn bộ chuỗi bản ghi thuộc thiết bị.
    Trả về tổng số bản ghi hợp lệ (sig_verified = true) và bị giả mạo (sig_verified = false).
    """
    try:
        verification_result = verify_device_chain(device_id=device_id)
        return jsonable_encoder(verification_result)
    except Exception as exc:
        logger.error("Lỗi kiểm tra chuỗi chữ ký của thiết bị %s: %s", device_id, exc)
        raise HTTPException(status_code=500, detail="Không thể xác thực chuỗi dữ liệu của thiết bị") from exc


@app.websocket("/ws/realtime")
async def websocket_realtime_endpoint(websocket: WebSocket):
    """
    Kênh WebSocket đẩy dữ liệu thời gian thực (telemetry, sync, alert, status) xuống giao diện giám sát.
    """
    await ws_manager.connect(websocket)
    try:
        while True:
            # Duy trì kết nối, chỉ nhận gói ping/keepalive từ trình duyệt (không nhận lệnh điều khiển)
            await websocket.receive_text()
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
    except Exception:
        ws_manager.disconnect(websocket)