#!/usr/bin/env python3
"""Validate SixEyes teleoperation JSONL logs against schema rules."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from teleop_log_schema import SCHEMA_VERSION, validate_log_record


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate teleoperation JSONL logs")
    parser.add_argument("--input", required=True, help="Path to JSONL log file")
    parser.add_argument(
        "--max-errors",
        type=int,
        default=20,
        help="Maximum validation errors to print (default: 20)",
    )
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()
    input_path = Path(args.input)

    if not input_path.exists():
        print(f"[ERROR] Log file not found: {input_path}")
        return 1

    total = 0
    valid = 0
    invalid = 0
    printed = 0

    print(f"[INFO] Validating {input_path} with schema v{SCHEMA_VERSION}")

    with input_path.open("r", encoding="utf-8") as handle:
        for line_no, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                continue

            total += 1

            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                invalid += 1
                if printed < args.max_errors:
                    print(f"[INVALID] line {line_no}: JSON parse error: {error}")
                    printed += 1
                continue

            if not isinstance(record, dict):
                invalid += 1
                if printed < args.max_errors:
                    print(f"[INVALID] line {line_no}: record must be JSON object")
                    printed += 1
                continue

            errors = validate_log_record(record)
            if errors:
                invalid += 1
                if printed < args.max_errors:
                    print(f"[INVALID] line {line_no}: {'; '.join(errors)}")
                    printed += 1
            else:
                valid += 1

    print("\n[SUMMARY]")
    print(f"  Total records: {total}")
    print(f"  Valid records: {valid}")
    print(f"  Invalid records: {invalid}")

    if invalid > 0:
        print("[RESULT] FAILED: one or more records are invalid")
        return 2

    print("[RESULT] PASS: all records valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
