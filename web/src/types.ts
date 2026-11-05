export interface Device {
  device_id: string;
  device_name: string;
  storage_type: string;
  temp_min_c: number;
  temp_max_c: number;
  fw_version: string;
  is_online: boolean;
  last_seen_at: string | null;
  created_at: string;
  latest_seq_no: number | null;
  latest_ts_rtc: string | null;
  temp_c: number | null;
  hum_pct: number | null;
  bat_v: number | null;
  alert_code: number | null;
  is_backfill: boolean | null;
  sig_verified: boolean | null;
}

export interface TelemetryRecord {
  log_id?: number;
  device_id: string;
  seq_no: number;
  ts_rtc: string | number;
  ts_epoch?: number;
  ts_iso?: string;
  received_at?: string;
  is_backfill: boolean;
  temp_c: number;
  hum_pct: number;
  bat_v: number;
  alert_code: number;
  sha256_hash: string;
  ecdsa_sig: string;
  sig_verified: boolean;
}

export interface VerifyChainSummary {
  device_id: string;
  total_records: number;
  valid_records: number;
  tampered_records: number;
  backfill_records: number;
  chain_intact: boolean;
  gsp_compliance_status: "VERIFIED" | "COMPROMISED";
  first_record_at: string | null;
  last_record_at: string | null;
}

export interface RealtimeTelemetryPayload extends TelemetryRecord {
  ts_iso: string;
  computed_sha256: string;
}

export interface RealtimeStatusPayload {
  device_id: string;
  state: "ONLINE" | "OFFLINE";
  is_online: boolean;
  updated_at: string;
}

export interface RealtimeAlertPayload {
  alert_id: number | null;
  device_id: string;
  seq_no: number;
  alert_code: number;
  severity: string;
  temp_c: number;
  hum_pct: number;
  message: string;
  ts_rtc: number;
  ts_iso: string;
}

export type RealtimeEvent =
  | {
      event: "telemetry" | "sync";
      inserted: boolean;
      data: RealtimeTelemetryPayload;
    }
  | {
      event: "status";
      data: RealtimeStatusPayload;
    }
  | {
      event: "alert";
      data: RealtimeAlertPayload;
    };