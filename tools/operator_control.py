#!/usr/bin/env python3
"""
SixEyes Operator Control Utility

Purpose:
- Send ROS2-style heartbeat packets to follower safety bridge.
- Send one-shot JSON commands for operator workflows:
  - home (RESET_FAULT + ENABLE_MOTORS + HOME_ZERO)
  - teleop-ready (RESET_FAULT + ENABLE_MOTORS)
  - vla-ready (RESET_FAULT + ENABLE_MOTORS)

Notes:
- Follower mode selection is compile-time (`OPERATION_MODE`), not runtime.
- This tool does not switch firmware mode; it prepares the currently flashed mode.
"""

from __future__ import annotations

import argparse
import json
import threading
import time
from dataclasses import dataclass
from typing import Optional

import serial


@dataclass
class HeartbeatConfig:
    source_id: int = 0
    hz: float = 50.0


class OperatorControl:
    def __init__(self, port: str, baud: int, hb: HeartbeatConfig):
        self.port = port
        self.baud = baud
        self.hb = hb
        self._ser: Optional[serial.Serial] = None
        self._serial_lock = threading.Lock()
        self._running = False
        self._hb_thread: Optional[threading.Thread] = None
        self._hb_seq = 0

    def connect(self) -> None:
        self._ser = serial.Serial(self.port, self.baud, timeout=0.1, write_timeout=0.2)
        print(f"[INFO] Connected follower serial: {self.port} @ {self.baud}")

    def close(self) -> None:
        self.stop_heartbeat()
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    def _write_line(self, line: str) -> None:
        assert self._ser is not None
        with self._serial_lock:
            self._ser.write((line + "\n").encode("utf-8"))

    def send_json(self, payload: dict) -> None:
        line = json.dumps(payload, separators=(",", ":"))
        self._write_line(line)
        print(f"[TX JSON] {line}")

    def start_heartbeat(self) -> None:
        if self._running:
            return
        self._running = True
        self._hb_thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
        self._hb_thread.start()
        print(f"[INFO] Heartbeat started: source={self.hb.source_id}, rate={self.hb.hz:.1f} Hz")

    def stop_heartbeat(self) -> None:
        self._running = False
        if self._hb_thread:
            self._hb_thread.join(timeout=1.0)
            self._hb_thread = None

    def _heartbeat_loop(self) -> None:
        period_s = max(0.005, 1.0 / max(1.0, self.hb.hz))
        while self._running:
            line = f"HB:{self.hb.source_id},{self._hb_seq}"
            try:
                self._write_line(line)
            except Exception as exc:
                print(f"[ERROR] Heartbeat write failed: {exc}")
                self._ser = None
                print(f"[INFO] Attempting to reconnect to {self.port}...")
                while self._running:
                    time.sleep(2.0)
                    try:
                        self._ser = serial.Serial(self.port, self.baud,
                                                  timeout=0.1, write_timeout=0.2)
                        print(f"[INFO] Reconnected to {self.port}")
                        break
                    except serial.SerialException:
                        pass
            self._hb_seq += 1
            time.sleep(period_s)


def _arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="SixEyes follower operator control")
    parser.add_argument("--port", required=True, help="Follower serial port (e.g. COM8 or /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--hb-source", type=int, default=0, help="Heartbeat source ID (default: 0)")
    parser.add_argument("--hb-hz", type=float, default=50.0, help="Heartbeat frequency in Hz (default: 50)")
    parser.add_argument("--seq", type=int, default=1, help="Starting JSON sequence number (default: 1)")
    parser.add_argument(
        "--hold-seconds",
        type=float,
        default=2.0,
        help="Seconds to keep heartbeat running after sending commands (default: 2.0)",
    )

    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("home", help="RESET_FAULT + ENABLE_MOTORS(true) + HOME_ZERO")
    sg = sub.add_parser(
        "stallguard-home",
        help="RESET_FAULT + ENABLE_MOTORS(true) + HOME_STALLGUARD (hybrid coarse homing)",
    )
    sg.add_argument("--motor-mask", type=lambda v: int(v, 0), default=0x0F, help="Bitmask for motors (default: 0x0F)")
    sg.add_argument("--sensitivity", type=int, default=100, help="StallGuard sensitivity 0-255 (default: 100)")
    sg.add_argument("--homing-hold-seconds", type=float, default=12.0, help="Heartbeat hold duration while homing runs")

    teleop = sub.add_parser("teleop-ready", help="Prepare teleoperation run (RESET_FAULT + ENABLE_MOTORS)")
    teleop.add_argument("--no-enable", action="store_true", help="Skip ENABLE_MOTORS command")

    vla = sub.add_parser("vla-ready", help="Prepare VLA run (RESET_FAULT + ENABLE_MOTORS)")
    vla.add_argument("--no-enable", action="store_true", help="Skip ENABLE_MOTORS command")

    send = sub.add_parser("send", help="Send a raw JSON payload")
    send.add_argument("--json", required=True, help="Raw JSON payload string")

    return parser


def _run_home(ctrl: OperatorControl, seq: int) -> int:
    ctrl.send_json({"cmd": "RESET_FAULT", "seq": seq})
    ctrl.send_json({"cmd": "ENABLE_MOTORS", "seq": seq + 1, "enable": True})
    ctrl.send_json({"cmd": "HOME_ZERO", "seq": seq + 2})
    return seq + 3


def _run_stallguard_home(ctrl: OperatorControl, seq: int, motor_mask: int, sensitivity: int) -> int:
    ctrl.send_json({"cmd": "RESET_FAULT", "seq": seq})
    ctrl.send_json({"cmd": "ENABLE_MOTORS", "seq": seq + 1, "enable": True})
    ctrl.send_json(
        {
            "cmd": "HOME_STALLGUARD",
            "seq": seq + 2,
            "motor_mask": motor_mask & 0x0F,
            "sensitivity": max(0, min(255, sensitivity)),
        }
    )
    return seq + 3


def _run_ready(ctrl: OperatorControl, seq: int, enable: bool) -> int:
    ctrl.send_json({"cmd": "RESET_FAULT", "seq": seq})
    seq += 1
    if enable:
        ctrl.send_json({"cmd": "ENABLE_MOTORS", "seq": seq, "enable": True})
        seq += 1
    return seq


def main() -> int:
    parser = _arg_parser()
    args = parser.parse_args()

    hb = HeartbeatConfig(source_id=args.hb_source, hz=args.hb_hz)
    ctrl = OperatorControl(port=args.port, baud=args.baud, hb=hb)

    try:
        ctrl.connect()
        ctrl.start_heartbeat()

        seq = args.seq
        if args.command == "home":
            _ = _run_home(ctrl, seq)
            print("[INFO] Home sequence sent (software zero at current pose)")
        elif args.command == "stallguard-home":
            _ = _run_stallguard_home(ctrl, seq, args.motor_mask, args.sensitivity)
            print(
                "[INFO] StallGuard homing sequence sent "
                f"(mask=0x{args.motor_mask & 0x0F:02X}, sensitivity={max(0, min(255, args.sensitivity))})"
            )
            args.hold_seconds = max(args.hold_seconds, args.homing_hold_seconds)
        elif args.command == "teleop-ready":
            _ = _run_ready(ctrl, seq, enable=not args.no_enable)
            print("[INFO] Teleoperation-ready sequence sent")
            print("[NOTE] Teleop mode itself must be flashed with OPERATION_MODE=2")
        elif args.command == "vla-ready":
            _ = _run_ready(ctrl, seq, enable=not args.no_enable)
            print("[INFO] VLA-ready sequence sent")
            print("[NOTE] VLA mode itself must be flashed with OPERATION_MODE=1")
        elif args.command == "send":
            payload = json.loads(args.json)
            if not isinstance(payload, dict):
                raise ValueError("JSON payload must be an object")
            ctrl.send_json(payload)
            print("[INFO] Raw JSON sent")
        else:
            parser.error("Unknown command")

        if args.hold_seconds > 0:
            print(f"[INFO] Holding heartbeat for {args.hold_seconds:.1f}s...")
            time.sleep(args.hold_seconds)

        print("[INFO] Done")
        return 0

    except KeyboardInterrupt:
        print("\n[INFO] Interrupted")
        return 130
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1
    finally:
        ctrl.close()


if __name__ == "__main__":
    raise SystemExit(main())
