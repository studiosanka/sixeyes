#!/usr/bin/env python3
"""
SixEyes Teleoperation Bridge (Phase 3)

Bridges leader_esp32 JOINT_STATE messages to follower_esp32 over serial.
Optionally logs forwarded packets to JSONL for dataset collection.

Protocol:
  Leader -> Bridge:   {"cmd":"JOINT_STATE", ...}\n
  Bridge -> Follower: {"cmd":"JOINT_STATE", ...}\n
Usage:
  python teleoperation_bridge.py --leader-port COM5 --follower-port COM6
  python teleoperation_bridge.py --leader-port /dev/ttyUSB0 --follower-port /dev/ttyUSB1 --log-file teleop.jsonl
    python validate_teleop_log.py --input teleop.jsonl
"""

import argparse
import json
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, TextIO

import serial
from teleop_log_schema import SCHEMA_VERSION, validate_log_record


@dataclass
class BridgeStats:
    packets_in: int = 0
    packets_forwarded: int = 0
    packets_dropped: int = 0
    parse_errors: int = 0
    telemetry_in: int = 0
    telemetry_parse_errors: int = 0
    last_seq: Optional[int] = None
    seq_gaps: int = 0
    out_of_order: int = 0
    source_latency_ms_avg: float = 0.0
    source_latency_ms_max: float = 0.0
    interarrival_ms_avg: float = 0.0
    interarrival_jitter_ms_avg: float = 0.0
    last_leader_ts_ms: Optional[int] = None
    last_rx_monotonic_s: Optional[float] = None
    last_interarrival_ms: Optional[float] = None
    log_records_written: int = 0
    log_records_rejected: int = 0


class TeleoperationBridge:
    def __init__(
        self,
        leader_port: str,
        follower_port: str,
        baudrate: int = 115200,
        log_file: Optional[str] = None,
        schema_strict: bool = True,
    ):
        self.leader_port = leader_port
        self.follower_port = follower_port
        self.baudrate = baudrate
        self.log_file_path = Path(log_file) if log_file else None
        self.schema_strict = schema_strict

        self.leader_serial: Optional[serial.Serial] = None
        self.follower_serial: Optional[serial.Serial] = None
        self.log_handle: Optional[TextIO] = None

        self.running = False
        self.stats = BridgeStats()
        self.lock = threading.Lock()
        self.forward_thread: Optional[threading.Thread] = None
        self.telemetry_thread: Optional[threading.Thread] = None

    @staticmethod
    def _ewma(prev: float, value: float, alpha: float = 0.1) -> float:
        if prev <= 0.0:
            return value
        return (alpha * value) + ((1.0 - alpha) * prev)

    def _update_stream_metrics(self, payload: dict) -> None:
        seq_val = payload.get("seq")
        ts_val = payload.get("ts")
        now_ms = int(time.time() * 1000)
        now_mono = time.monotonic()

        with self.lock:
            if isinstance(seq_val, int):
                if self.stats.last_seq is not None:
                    if seq_val <= self.stats.last_seq:
                        self.stats.out_of_order += 1
                    elif seq_val > (self.stats.last_seq + 1):
                        self.stats.seq_gaps += (seq_val - self.stats.last_seq - 1)
                self.stats.last_seq = seq_val

            if isinstance(ts_val, int):
                latency = float(max(0, now_ms - ts_val))
                self.stats.source_latency_ms_avg = self._ewma(self.stats.source_latency_ms_avg, latency)
                if latency > self.stats.source_latency_ms_max:
                    self.stats.source_latency_ms_max = latency
                self.stats.last_leader_ts_ms = ts_val

            if self.stats.last_rx_monotonic_s is not None:
                interarrival_ms = (now_mono - self.stats.last_rx_monotonic_s) * 1000.0
                self.stats.interarrival_ms_avg = self._ewma(self.stats.interarrival_ms_avg, interarrival_ms)

                if self.stats.last_interarrival_ms is not None:
                    jitter_ms = abs(interarrival_ms - self.stats.last_interarrival_ms)
                    self.stats.interarrival_jitter_ms_avg = self._ewma(self.stats.interarrival_jitter_ms_avg, jitter_ms)

                self.stats.last_interarrival_ms = interarrival_ms
            self.stats.last_rx_monotonic_s = now_mono

    def connect(self) -> bool:
        try:
            self.leader_serial = serial.Serial(
                port=self.leader_port,
                baudrate=self.baudrate,
                timeout=0.1,
                write_timeout=0.2,
            )
            self.follower_serial = serial.Serial(
                port=self.follower_port,
                baudrate=self.baudrate,
                timeout=0.1,
                write_timeout=0.2,
            )

            if self.log_file_path:
                self.log_file_path.parent.mkdir(parents=True, exist_ok=True)
                self.log_handle = self.log_file_path.open("a", encoding="utf-8")

            print(f"[INFO] Leader serial connected: {self.leader_port} @ {self.baudrate}")
            print(f"[INFO] Follower serial connected: {self.follower_port} @ {self.baudrate}")
            if self.log_file_path:
                print(f"[INFO] Logging forwarded packets to: {self.log_file_path}")
                print(f"[INFO] Log schema version: {SCHEMA_VERSION} (strict={self.schema_strict})")
            return True

        except serial.SerialException as error:
            print(f"[ERROR] Serial connect failed: {error}")
            self.disconnect()
            return False

    def disconnect(self) -> None:
        if self.log_handle:
            self.log_handle.close()
            self.log_handle = None

        if self.leader_serial and self.leader_serial.is_open:
            self.leader_serial.close()
        if self.follower_serial and self.follower_serial.is_open:
            self.follower_serial.close()

        self.leader_serial = None
        self.follower_serial = None

    def start(self) -> bool:
        if not self.leader_serial or not self.follower_serial:
            print("[ERROR] Bridge not connected. Call connect() first.")
            return False

        self.running = True
        self.forward_thread = threading.Thread(target=self._forward_loop, daemon=True)
        self.forward_thread.start()

        self.telemetry_thread = threading.Thread(target=self._telemetry_loop, daemon=True)
        self.telemetry_thread.start()
        print("[INFO] Teleoperation bridge started")
        return True

    def stop(self) -> None:
        self.running = False
        if self.forward_thread:
            self.forward_thread.join(timeout=2.0)
        if self.telemetry_thread:
            self.telemetry_thread.join(timeout=2.0)
        self.disconnect()

    def _forward_loop(self) -> None:
        assert self.leader_serial is not None
        assert self.follower_serial is not None

        while self.running:
            try:
                raw_line = self.leader_serial.readline()
                if not raw_line:
                    continue

                line = raw_line.decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                with self.lock:
                    self.stats.packets_in += 1

                payload = self._parse_and_validate(line)
                if payload is None:
                    continue

                self._update_stream_metrics(payload)

                encoded = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
                self.follower_serial.write(encoded)

                with self.lock:
                    self.stats.packets_forwarded += 1

                if self.log_handle:
                    log_record = {
                        "bridge_ts": int(time.time() * 1000),
                        "direction": "leader_to_follower",
                        "payload": payload,
                    }
                    self._log_record(log_record)

            except serial.SerialException as error:
                print(f"[ERROR] Serial forwarding failed: {error}")
                self.running = False
                break
            except Exception as error:
                print(f"[ERROR] Unexpected forwarding error: {error}")
                with self.lock:
                    self.stats.packets_dropped += 1

    def _telemetry_loop(self) -> None:
        assert self.follower_serial is not None

        while self.running:
            try:
                raw_line = self.follower_serial.readline()
                if not raw_line:
                    continue

                line = raw_line.decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                payload = self._parse_telemetry(line)
                if payload is None:
                    continue

                print(f"[TELEMETRY] seq={payload.get('seq', '?')} ts={payload.get('ts', '?')}")

                if self.log_handle:
                    log_record = {
                        "bridge_ts": int(time.time() * 1000),
                        "direction": "follower_to_laptop",
                        "payload": payload,
                    }
                    self._log_record(log_record)

            except serial.SerialException as error:
                print(f"[ERROR] Telemetry receive failed: {error}")
                self.running = False
                break
            except Exception as error:
                print(f"[ERROR] Unexpected telemetry error: {error}")

    def _parse_telemetry(self, line: str) -> Optional[dict]:
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            with self.lock:
                self.stats.telemetry_parse_errors += 1
            return None

        if payload.get("cmd") != "TELEMETRY_STATE":
            return None

        with self.lock:
            self.stats.telemetry_in += 1

        return payload

    def _log_record(self, record: dict) -> None:
        if not self.log_handle:
            return

        schema_errors = validate_log_record(record)
        if schema_errors:
            with self.lock:
                self.stats.log_records_rejected += 1

            if self.schema_strict:
                print(f"[WARN] Dropping invalid log record: {'; '.join(schema_errors)}")
                return

            record = dict(record)
            record["schema_errors"] = schema_errors

        self.log_handle.write(json.dumps(record) + "\n")
        self.log_handle.flush()
        with self.lock:
            self.stats.log_records_written += 1

    def _parse_and_validate(self, line: str) -> Optional[dict]:
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            with self.lock:
                self.stats.parse_errors += 1
                self.stats.packets_dropped += 1
            return None

        if payload.get("cmd") != "JOINT_STATE":
            with self.lock:
                self.stats.packets_dropped += 1
            return None

        joints = payload.get("leader_joints")
        valid_mask = payload.get("valid_mask")

        if not isinstance(joints, list) or not isinstance(valid_mask, list):
            with self.lock:
                self.stats.packets_dropped += 1
            return None

        if len(joints) != 6 or len(valid_mask) != 6:
            with self.lock:
                self.stats.packets_dropped += 1
            return None

        return payload

    def print_stats(self) -> None:
        with self.lock:
            print("\n[STATS] Teleoperation Bridge")
            print(f"  Packets in:        {self.stats.packets_in}")
            print(f"  Packets forwarded: {self.stats.packets_forwarded}")
            print(f"  Packets dropped:   {self.stats.packets_dropped}")
            print(f"  Parse errors:      {self.stats.parse_errors}")
            print(f"  Telemetry in:      {self.stats.telemetry_in}")
            print(f"  Telemetry parse errors: {self.stats.telemetry_parse_errors}")
            print(f"  Last sequence:     {self.stats.last_seq}")
            print(f"  Sequence gaps:     {self.stats.seq_gaps}")
            print(f"  Out-of-order:      {self.stats.out_of_order}")
            print(f"  Source latency avg/max (ms): {self.stats.source_latency_ms_avg:.2f} / {self.stats.source_latency_ms_max:.2f}")
            print(f"  Interarrival avg (ms): {self.stats.interarrival_ms_avg:.2f}")
            print(f"  Jitter avg (ms):   {self.stats.interarrival_jitter_ms_avg:.2f}")
            print(f"  Log records written:  {self.stats.log_records_written}")
            print(f"  Log records rejected: {self.stats.log_records_rejected}")
            print()


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="SixEyes Teleoperation Bridge")
    parser.add_argument("--leader-port", required=True, help="Leader ESP32 serial port (e.g., COM5)")
    parser.add_argument("--follower-port", required=True, help="Follower ESP32 serial port (e.g., COM6)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--log-file", default=None, help="Optional JSONL output path for forwarded packets")
    parser.add_argument("--stats-interval", type=float, default=5.0, help="Stats print interval in seconds")

    strict_group = parser.add_mutually_exclusive_group()
    strict_group.add_argument(
        "--schema-strict",
        action="store_true",
        default=True,
        help="Drop invalid log records (default)",
    )
    strict_group.add_argument(
        "--no-schema-strict",
        action="store_false",
        dest="schema_strict",
        help="Keep invalid records and append schema_errors field",
    )

    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()

    bridge = TeleoperationBridge(
        leader_port=args.leader_port,
        follower_port=args.follower_port,
        baudrate=args.baud,
        log_file=args.log_file,
        schema_strict=args.schema_strict,
    )

    if not bridge.connect():
        return 1

    if not bridge.start():
        bridge.disconnect()
        return 1

    print("[INFO] Running. Press Ctrl+C to stop.")

    try:
        last_stats = time.time()
        while True:
            time.sleep(0.25)
            if time.time() - last_stats >= args.stats_interval:
                bridge.print_stats()
                last_stats = time.time()
    except KeyboardInterrupt:
        print("\n[INFO] Stopping bridge...")
        bridge.stop()
        print("[INFO] Bridge stopped")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
