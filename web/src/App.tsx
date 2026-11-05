import React, { useEffect, useState, useCallback, useRef } from "react";
import {
  Thermometer,
  Droplets,
  BatteryCharging,
  ShieldCheck,
  ShieldAlert,
  Wifi,
  WifiOff,
  AlertTriangle,
  RefreshCw,
  Database,
  CheckCircle2,
  XCircle,
  HardDriveDownload,
  Lock,
} from "lucide-react";
import {
  Device,
  TelemetryRecord,
  VerifyChainSummary,
  RealtimeEvent,
  RealtimeAlertPayload,
} from "./types";

const API_BASE_URL =
  import.meta.env.VITE_API_BASE_URL || "http://localhost:8000";
const WS_URL =
  import.meta.env.VITE_WS_URL || "ws://localhost:8000/ws/realtime";
const DEFAULT_DEVICE_ID = "GSP-TRUCK-01";
const MAX_TABLE_ROWS = 50;

function formatUtcTimestamp(ts: string | number | null | undefined): string {
  if (ts === null || ts === undefined) return "—";
  const dateObj =
    typeof ts === "number" ? new Date(ts * 1000) : new Date(String(ts));
  if (Number.isNaN(dateObj.getTime())) return String(ts);
  return dateObj.toLocaleString("vi-VN", {
    hour12: false,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function shortenHash(hash: string): string {
  if (!hash || hash.length < 16) return hash || "—";
  return `${hash.slice(0, 10)}...${hash.slice(-8)}`;
}

export default function App() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] =
    useState<string>(DEFAULT_DEVICE_ID);
  const [telemetryList, setTelemetryList] = useState<TelemetryRecord[]>([]);
  const [verifySummary, setVerifySummary] =
    useState<VerifyChainSummary | null>(null);
  const [verifying, setVerifying] = useState<boolean>(false);
  const [wsConnected, setWsConnected] = useState<boolean>(false);
  const [latestAlert, setLatestAlert] = useState<RealtimeAlertPayload | null>(
    null
  );
  const [loadingInitial, setLoadingInitial] = useState<boolean>(true);
  const wsRef = useRef<WebSocket | null>(null);

  const fetchInitialData = useCallback(async (deviceId: string) => {
    setLoadingInitial(true);
    try {
      const [devRes, telRes] = await Promise.all([
        fetch(`${API_BASE_URL}/api/v1/devices`),
        fetch(
          `${API_BASE_URL}/api/v1/devices/${encodeURIComponent(
            deviceId
          )}/telemetry?limit=20`
        ),
      ]);

      if (devRes.ok) {
        const devJson = await devRes.json();
        setDevices(devJson.items || []);
      }

      if (telRes.ok) {
        const telJson = await telRes.json();
        setTelemetryList(telJson.items || []);
      }
    } catch (err) {
      console.error("Lỗi tải dữ liệu khởi tạo:", err);
    } finally {
      setLoadingInitial(false);
    }
  }, []);

  const handleVerifyChain = async () => {
    setVerifying(true);
    try {
      const res = await fetch(
        `${API_BASE_URL}/api/v1/devices/${encodeURIComponent(
          selectedDeviceId
        )}/verify-chain`
      );
      if (res.ok) {
        const summary: VerifyChainSummary = await res.json();
        setVerifySummary(summary);
      }
    } catch (err) {
      console.error("Lỗi kiểm toán chuỗi chữ ký GSP:", err);
    } finally {
      setVerifying(false);
    }
  };

  useEffect(() => {
    fetchInitialData(selectedDeviceId);
  }, [selectedDeviceId, fetchInitialData]);

  useEffect(() => {
    let reconnectTimer: number | undefined;
    let pingInterval: number | undefined;

    const connectWebSocket = () => {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        setWsConnected(true);
        pingInterval = window.setInterval(() => {
          if (ws.readyState === WebSocket.OPEN) {
            ws.send("ping");
          }
        }, 20000);
      };

      ws.onmessage = (msgEvent) => {
        try {
          const payload: RealtimeEvent = JSON.parse(msgEvent.data);

          if (payload.event === "telemetry" || payload.event === "sync") {
            const record = payload.data;

            setDevices((prevDevices) =>
              prevDevices.map((dev) =>
                dev.device_id === record.device_id
                  ? {
                      ...dev,
                      is_online: true,
                      last_seen_at: record.ts_iso,
                      latest_seq_no: record.seq_no,
                      latest_ts_rtc: record.ts_iso,
                      temp_c: record.temp_c,
                      hum_pct: record.hum_pct,
                      bat_v: record.bat_v,
                      alert_code: record.alert_code,
                      is_backfill: record.is_backfill,
                      sig_verified: record.sig_verified,
                    }
                  : dev
              )
            );

            if (record.device_id === selectedDeviceId) {
              setTelemetryList((prev) => {
                const deduplicated = prev.filter(
                  (item) => item.seq_no !== record.seq_no
                );
                return [
                  {
                    ...record,
                    ts_rtc: record.ts_iso || record.ts_rtc,
                  },
                  ...deduplicated,
                ].slice(0, MAX_TABLE_ROWS);
              });
            }
          } else if (payload.event === "status") {
            const st = payload.data;
            setDevices((prevDevices) =>
              prevDevices.map((dev) =>
                dev.device_id === st.device_id
                  ? {
                      ...dev,
                      is_online: st.is_online,
                      last_seen_at: st.updated_at,
                    }
                  : dev
              )
            );
          } else if (payload.event === "alert") {
            setLatestAlert(payload.data);
          }
        } catch (err) {
          console.error("Lỗi phân tích gói tin WebSocket:", err);
        }
      };

      ws.onclose = () => {
        setWsConnected(false);
        if (pingInterval) window.clearInterval(pingInterval);
        reconnectTimer = window.setTimeout(connectWebSocket, 3000);
      };

      ws.onerror = () => {
        ws.close();
      };
    };

    connectWebSocket();

    return () => {
      if (reconnectTimer) window.clearTimeout(reconnectTimer);
      if (pingInterval) window.clearInterval(pingInterval);
      if (wsRef.current) wsRef.current.close();
    };
  }, [selectedDeviceId]);

  const activeDevice: Device | undefined =
    devices.find((d) => d.device_id === selectedDeviceId) || devices[0];

  const latestRow: TelemetryRecord | undefined = telemetryList[0];

  const currentTemp =
    latestRow?.temp_c ?? activeDevice?.temp_c ?? null;
  const currentHum =
    latestRow?.hum_pct ?? activeDevice?.hum_pct ?? null;
  const currentBat =
    latestRow?.bat_v ?? activeDevice?.bat_v ?? null;
  const currentAlertCode =
    latestRow?.alert_code ?? activeDevice?.alert_code ?? 0;
  const isOnline = activeDevice?.is_online ?? false;

  const minTempLimit = activeDevice?.temp_min_c ?? 2.0;
  const maxTempLimit = activeDevice?.temp_max_c ?? 8.0;

  const isTempViolation =
    currentTemp !== null &&
    (currentTemp > maxTempLimit ||
      currentTemp < minTempLimit ||
      currentAlertCode > 0);

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 p-4 md:p-6">
      {/* Header */}
      <header className="max-w-7xl mx-auto flex flex-col lg:flex-row lg:items-center lg:justify-between gap-4 border-b border-slate-800 pb-5">
        <div>
          <div className="flex items-center gap-2 text-cyan-400 text-xs font-semibold uppercase tracking-wider">
            <Lock className="w-4 h-4" />
            <span>Unidirectional & Append-Only GSP Architecture</span>
          </div>
          <h1 className="text-2xl md:text-3xl font-bold text-white mt-1">
            GSP Cold-Chain IoT Monitor
          </h1>
          <p className="text-slate-400 text-sm mt-0.5">
            Thiết bị: {activeDevice?.device_name || "Xe tải Vaccine 29H-123.45"}{" "}
            ({selectedDeviceId}) • Ngưỡng chuẩn: +{minTempLimit.toFixed(1)}°C
            đến +{maxTempLimit.toFixed(1)}°C
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-3">
          {/* Trạng thái Thiết bị (MQTT LWT / Status) */}
          <div
            className={`flex items-center gap-2 px-3.5 py-2 rounded-lg text-xs font-bold border ${
              isOnline
                ? "bg-emerald-950/60 border-emerald-600 text-emerald-300"
                : "bg-rose-950/60 border-rose-600 text-rose-300"
            }`}
          >
            {isOnline ? (
              <Wifi className="w-4 h-4 text-emerald-400" />
            ) : (
              <WifiOff className="w-4 h-4 text-rose-400" />
            )}
            <span>THIẾT BỊ: {isOnline ? "ONLINE" : "OFFLINE (LWT)"}</span>
          </div>

          {/* Trạng thái kênh WebSocket */}
          <div
            className={`flex items-center gap-2 px-3 py-2 rounded-lg text-xs font-medium border ${
              wsConnected
                ? "bg-slate-900 border-slate-700 text-cyan-300"
                : "bg-amber-950/50 border-amber-700 text-amber-300"
            }`}
          >
            <span
              className={`w-2 h-2 rounded-full ${
                wsConnected ? "bg-cyan-400 animate-ping" : "bg-amber-400"
              }`}
            />
            <span>{wsConnected ? "WS Realtime Connected" : "WS Reconnecting..."}</span>
          </div>

          {/* Nút Kiểm toán chuỗi chữ ký GSP */}
          <button
            onClick={handleVerifyChain}
            disabled={verifying}
            className="flex items-center gap-2 bg-cyan-600 hover:bg-cyan-500 disabled:opacity-50 text-white font-semibold text-xs px-4 py-2.5 rounded-lg shadow transition cursor-pointer"
          >
            <ShieldCheck className="w-4 h-4" />
            <span>
              {verifying
                ? "Đang kiểm toán..."
                : "Kiểm toán chuỗi chữ ký GSP"}
            </span>
          </button>

          <button
            onClick={() => fetchInitialData(selectedDeviceId)}
            className="p-2.5 rounded-lg bg-slate-900 hover:bg-slate-800 border border-slate-700 text-slate-300 transition cursor-pointer"
            title="Làm mới dữ liệu"
          >
            <RefreshCw className="w-4 h-4" />
          </button>
        </div>
      </header>

      <main className="max-w-7xl mx-auto mt-6 space-y-6">
        {/* Banner cảnh báo khẩn cấp nếu vượt ngưỡng */}
        {(isTempViolation || latestAlert) && (
          <div className="bg-red-950/90 border-2 border-red-500 rounded-xl p-4 flex items-center justify-between animate-pulse">
            <div className="flex items-center gap-3">
              <AlertTriangle className="w-7 h-7 text-red-400 shrink-0" />
              <div>
                <h2 className="text-red-200 font-bold text-sm md:text-base">
                  CẢNH BÁO VI PHẠM NGƯỠNG VI KHÍ HẬU GSP (ALERT CODE:{" "}
                  {currentAlertCode})
                </h2>
                <p className="text-red-300 text-xs md:text-sm">
                  {latestAlert
                    ? `${latestAlert.message} (Nhiệt độ: ${latestAlert.temp_c.toFixed(
                        2
                      )}°C lúc ${formatUtcTimestamp(latestAlert.ts_iso)})`
                    : `Nhiệt độ hiện tại (${currentTemp?.toFixed(
                        2
                      )}°C) nằm ngoài dải bảo quản an toàn [+${minTempLimit.toFixed(
                        1
                      )}°C .. +${maxTempLimit.toFixed(1)}°C]!`}
                </p>
              </div>
            </div>
            {latestAlert && (
              <button
                onClick={() => setLatestAlert(null)}
                className="text-xs px-3 py-1 rounded bg-red-900 hover:bg-red-800 text-red-100 border border-red-600"
              >
                Đã xem
              </button>
            )}
          </div>
        )}

        {/* 4 Thẻ chỉ số chính (KPI Cards) */}
        <section className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
          {/* Thẻ Nhiệt độ - Đổi sang Đỏ nhấp nháy khi vi phạm */}
          <div
            className={`rounded-xl p-5 border transition ${
              isTempViolation
                ? "bg-red-950/80 border-red-500 animate-pulse shadow-lg shadow-red-950/50"
                : "bg-slate-900/90 border-slate-800"
            }`}
          >
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold uppercase tracking-wider text-slate-400">
                Nhiệt độ Kho / Thùng xe
              </span>
              <Thermometer
                className={`w-5 h-5 ${
                  isTempViolation ? "text-red-400" : "text-cyan-400"
                }`}
              />
            </div>
            <div className="mt-3 flex items-baseline gap-2">
              <span
                className={`text-3xl font-extrabold ${
                  isTempViolation ? "text-red-300" : "text-white"
                }`}
              >
                {currentTemp !== null ? `${currentTemp.toFixed(2)}°C` : "—"}
              </span>
              <span
                className={`text-xs font-semibold px-2 py-0.5 rounded ${
                  isTempViolation
                    ? "bg-red-500/30 text-red-200"
                    : "bg-emerald-500/20 text-emerald-300"
                }`}
              >
                {isTempViolation ? "VƯỢT NGƯỠNG" : "ĐẠT CHUẨN GSP"}
              </span>
            </div>
            <p className="text-xs text-slate-400 mt-2">
              Ngưỡng cho phép: +{minTempLimit.toFixed(2)}°C → +
              {maxTempLimit.toFixed(2)}°C
            </p>
          </div>

          {/* Thẻ Độ ẩm */}
          <div className="rounded-xl p-5 bg-slate-900/90 border border-slate-800">
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold uppercase tracking-wider text-slate-400">
                Độ ẩm Tương đối (SHT30)
              </span>
              <Droplets className="w-5 h-5 text-sky-400" />
            </div>
            <div className="mt-3 flex items-baseline gap-2">
              <span className="text-3xl font-extrabold text-white">
                {currentHum !== null ? `${currentHum.toFixed(2)}%` : "—"}
              </span>
              <span className="text-xs text-sky-300 bg-sky-500/15 px-2 py-0.5 rounded">
                ±2.0% RH
              </span>
            </div>
            <p className="text-xs text-slate-400 mt-2">
              Cảm biến I2C0 (0x44) • Chu kỳ đọc realtime
            </p>
          </div>

          {/* Thẻ Điện áp Pin 18650 */}
          <div className="rounded-xl p-5 bg-slate-900/90 border border-slate-800">
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold uppercase tracking-wider text-slate-400">
                Nguồn Pin Dự phòng 18650
              </span>
              <BatteryCharging className="w-5 h-5 text-emerald-400" />
            </div>
            <div className="mt-3 flex items-baseline gap-2">
              <span className="text-3xl font-extrabold text-white">
                {currentBat !== null ? `${currentBat.toFixed(2)} V` : "—"}
              </span>
              <span
                className={`text-xs font-semibold px-2 py-0.5 rounded ${
                  currentBat !== null && currentBat < 3.5
                    ? "bg-amber-500/20 text-amber-300"
                    : "bg-emerald-500/20 text-emerald-300"
                }`}
              >
                {currentBat !== null && currentBat < 3.5 ? "PIN YẾU" : "ỔN ĐỊNH"}
              </span>
            </div>
            <p className="text-xs text-slate-400 mt-2">
              ADC1_CH3 (GPIO4) • Dự phòng ≥ 8 giờ
            </p>
          </div>

          {/* Thẻ Trạng thái Chữ ký số ECDSA bản tin mới nhất */}
          <div className="rounded-xl p-5 bg-slate-900/90 border border-slate-800">
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold uppercase tracking-wider text-slate-400">
                Toàn vẹn Chữ ký Số (FR-05)
              </span>
              {latestRow?.sig_verified === false ? (
                <ShieldAlert className="w-5 h-5 text-rose-400" />
              ) : (
                <ShieldCheck className="w-5 h-5 text-emerald-400" />
              )}
            </div>
            <div className="mt-3 flex items-center gap-2">
              {latestRow ? (
                latestRow.sig_verified ? (
                  <span className="inline-flex items-center gap-1.5 text-sm font-bold text-emerald-300 bg-emerald-950 border border-emerald-600 px-3 py-1 rounded-lg">
                    <CheckCircle2 className="w-4 h-4" />
                    ECDSA VERIFIED
                  </span>
                ) : (
                  <span className="inline-flex items-center gap-1.5 text-sm font-bold text-rose-200 bg-rose-950 border border-rose-500 px-3 py-1 rounded-lg animate-bounce">
                    <XCircle className="w-4 h-4" />
                    TAMPERED
                  </span>
                )
              ) : (
                <span className="text-slate-500 text-sm">Chưa có bản ghi</span>
              )}
            </div>
            <p className="text-xs text-slate-400 mt-2 font-mono truncate">
              Seq #{latestRow?.seq_no ?? "—"} • SHA-256 + secp256r1
            </p>
          </div>
        </section>

        {/* Kết quả Kiểm toán chuỗi chữ ký GSP (Hiển thị khi bấm nút) */}
        {verifySummary && (
          <section
            className={`rounded-xl p-5 border ${
              verifySummary.chain_intact
                ? "bg-emerald-950/30 border-emerald-700"
                : "bg-rose-950/40 border-rose-600"
            }`}
          >
            <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4">
              <div className="flex items-start gap-3">
                {verifySummary.chain_intact ? (
                  <ShieldCheck className="w-7 h-7 text-emerald-400 shrink-0 mt-0.5" />
                ) : (
                  <ShieldAlert className="w-7 h-7 text-rose-400 shrink-0 mt-0.5" />
                )}
                <div>
                  <h3 className="text-base font-bold text-white">
                    Kết quả Kiểm toán Chuỗi Chữ ký Số GSP:{" "}
                    <span
                      className={
                        verifySummary.chain_intact
                          ? "text-emerald-400"
                          : "text-rose-400"
                      }
                    >
                      {verifySummary.gsp_compliance_status}
                    </span>
                  </h3>
                  <p className="text-xs text-slate-300 mt-1">
                    Thiết bị <strong>{verifySummary.device_id}</strong> • Từ:{" "}
                    {formatUtcTimestamp(verifySummary.first_record_at)} đến{" "}
                    {formatUtcTimestamp(verifySummary.last_record_at)}
                  </p>
                </div>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 text-center">
                <div className="bg-slate-900/90 px-3 py-2 rounded-lg border border-slate-800">
                  <div className="text-xs text-slate-400">Tổng bản ghi</div>
                  <div className="text-lg font-bold text-white">
                    {verifySummary.total_records}
                  </div>
                </div>
                <div className="bg-slate-900/90 px-3 py-2 rounded-lg border border-emerald-800/60">
                  <div className="text-xs text-emerald-400">Hợp lệ (Verified)</div>
                  <div className="text-lg font-bold text-emerald-300">
                    {verifySummary.valid_records}
                  </div>
                </div>
                <div className="bg-slate-900/90 px-3 py-2 rounded-lg border border-rose-800/60">
                  <div className="text-xs text-rose-400">Giả mạo (Tampered)</div>
                  <div className="text-lg font-bold text-rose-300">
                    {verifySummary.tampered_records}
                  </div>
                </div>
                <div className="bg-slate-900/90 px-3 py-2 rounded-lg border border-amber-800/60">
                  <div className="text-xs text-amber-400">Đẩy bù SD (Sync)</div>
                  <div className="text-lg font-bold text-amber-300">
                    {verifySummary.backfill_records}
                  </div>
                </div>
              </div>
            </div>
          </section>
        )}

        {/* Bảng Lịch sử Bản ghi Bất biến (Tối đa 50 dòng mới nhất) */}
        <section className="bg-slate-900/90 border border-slate-800 rounded-xl overflow-hidden">
          <div className="px-5 py-4 border-b border-slate-800 flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
            <div className="flex items-center gap-2">
              <Database className="w-5 h-5 text-cyan-400" />
              <h2 className="text-base font-bold text-white">
                Nhật ký Đo
              </h2>
            </div>
            <span className="text-xs text-slate-400">
              Hiển thị {telemetryList.length}/{MAX_TABLE_ROWS} bản ghi gần nhất •
              UPDATE và DELETE bị khóa
            </span>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse text-xs md:text-sm">
              <thead>
                <tr className="bg-slate-950/70 text-slate-400 border-b border-slate-800 uppercase text-[11px] tracking-wider">
                  <th className="py-3 px-4">Seq No</th>
                  <th className="py-3 px-4">Thời gian</th>
                  <th className="py-3 px-4">Kênh truyền</th>
                  <th className="py-3 px-4">Nhiệt độ</th>
                  <th className="py-3 px-4">Độ ẩm</th>
                  <th className="py-3 px-4">Pin 18650</th>
                  <th className="py-3 px-4">Mã băm SHA-256</th>
                  <th className="py-3 px-4">Xác thực ECDSA</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800/80">
                {loadingInitial && telemetryList.length === 0 ? (
                  <tr>
                    <td
                      colSpan={8}
                      className="py-10 text-center text-slate-400"
                    >
                      Đang tải dữ liệu từ TimescaleDB...
                    </td>
                  </tr>
                ) : telemetryList.length === 0 ? (
                  <tr>
                    <td
                      colSpan={8}
                      className="py-10 text-center text-slate-400"
                    >
                      Chưa có bản ghi nào. Hãy chạy Dummy Simulator hoặc bật mạch
                      ESP32-S3 để đẩy dữ liệu!
                    </td>
                  </tr>
                ) : (
                  telemetryList.map((row) => {
                    const rowTempViolation =
                      row.temp_c > maxTempLimit ||
                      row.temp_c < minTempLimit ||
                      row.alert_code > 0;

                    return (
                      <tr
                        key={`${row.device_id}-${row.seq_no}-${row.ts_rtc}`}
                        className={`hover:bg-slate-800/50 transition ${
                          !row.sig_verified
                            ? "bg-rose-950/30"
                            : rowTempViolation
                            ? "bg-red-950/20"
                            : ""
                        }`}
                      >
                        <td className="py-3 px-4 font-mono font-semibold text-cyan-300">
                          #{row.seq_no}
                        </td>
                        <td className="py-3 px-4 text-slate-200 whitespace-nowrap">
                          {formatUtcTimestamp(row.ts_iso || row.ts_rtc)}
                        </td>
                        <td className="py-3 px-4">
                          {row.is_backfill ? (
                            <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded text-[11px] font-semibold bg-amber-500/20 text-amber-300 border border-amber-600/40">
                              <HardDriveDownload className="w-3 h-3" />
                              AUTO-SYNC (SD)
                            </span>
                          ) : (
                            <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded text-[11px] font-semibold bg-cyan-500/15 text-cyan-300 border border-cyan-600/30">
                              REALTIME
                            </span>
                          )}
                        </td>
                        <td
                          className={`py-3 px-4 font-bold ${
                            rowTempViolation
                              ? "text-red-400"
                              : "text-emerald-300"
                          }`}
                        >
                          {Number(row.temp_c).toFixed(2)}°C
                        </td>
                        <td className="py-3 px-4 text-sky-300">
                          {Number(row.hum_pct).toFixed(2)}%
                        </td>
                        <td className="py-3 px-4 text-slate-300">
                          {Number(row.bat_v).toFixed(2)}V
                        </td>
                        <td
                          className="py-3 px-4 font-mono text-xs text-slate-400"
                          title={row.sha256_hash}
                        >
                          {shortenHash(row.sha256_hash)}
                        </td>
                        <td className="py-3 px-4">
                          {row.sig_verified ? (
                            <span className="inline-flex items-center gap-1 px-2.5 py-0.5 rounded-full text-xs font-bold bg-emerald-500/20 text-emerald-300 border border-emerald-500/40">
                              <CheckCircle2 className="w-3.5 h-3.5" />
                              ECDSA Verified
                            </span>
                          ) : (
                            <span className="inline-flex items-center gap-1 px-2.5 py-0.5 rounded-full text-xs font-bold bg-rose-600/30 text-rose-200 border border-rose-500 animate-pulse">
                              <XCircle className="w-3.5 h-3.5" />
                              TAMPERED
                            </span>
                          )}
                        </td>
                      </tr>
                    );
                  })
                )}
              </tbody>
            </table>
          </div>
        </section>
      </main>
    </div>
  );
}