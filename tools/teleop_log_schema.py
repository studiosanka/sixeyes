#!/usr/bin/env python3
"""Schema validation helpers for SixEyes teleoperation JSONL logs."""

from __future__ import annotations

from typing import Any

SCHEMA_VERSION = "1.0"


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_number(value: Any) -> bool:
    return (isinstance(value, (int, float)) and not isinstance(value, bool))


def _validate_numeric_array(value: Any, expected_len: int, field_name: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, list):
        return [f"{field_name} must be a list"]
    if len(value) != expected_len:
        return [f"{field_name} must have length {expected_len}, got {len(value)}"]

    for idx, element in enumerate(value):
        if not _is_number(element):
            errors.append(f"{field_name}[{idx}] must be numeric")
    return errors


def _validate_mask_array(value: Any, expected_len: int, field_name: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, list):
        return [f"{field_name} must be a list"]
    if len(value) != expected_len:
        return [f"{field_name} must have length {expected_len}, got {len(value)}"]

    for idx, element in enumerate(value):
        if isinstance(element, bool):
            continue
        if not _is_int(element):
            errors.append(f"{field_name}[{idx}] must be int/bool")
            continue
        if element not in (0, 1):
            errors.append(f"{field_name}[{idx}] must be 0/1")
    return errors


def validate_joint_state_payload(payload: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    if payload.get("cmd") != "JOINT_STATE":
        errors.append("cmd must be JOINT_STATE")

    if not _is_int(payload.get("seq")):
        errors.append("seq must be int")

    if not _is_int(payload.get("ts")):
        errors.append("ts must be int")

    errors.extend(_validate_numeric_array(payload.get("leader_joints"), 6, "leader_joints"))
    errors.extend(_validate_mask_array(payload.get("valid_mask"), 6, "valid_mask"))

    return errors


def validate_telemetry_state_payload(payload: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    if payload.get("cmd") != "TELEMETRY_STATE":
        errors.append("cmd must be TELEMETRY_STATE")

    if not _is_int(payload.get("seq")):
        errors.append("seq must be int")

    if not _is_int(payload.get("ts")):
        errors.append("ts must be int")

    errors.extend(_validate_numeric_array(payload.get("follower_joints"), 6, "follower_joints"))
    errors.extend(_validate_numeric_array(payload.get("errors"), 6, "errors"))
    errors.extend(_validate_mask_array(payload.get("faults"), 6, "faults"))

    if "valid_mask" in payload:
        errors.extend(_validate_mask_array(payload.get("valid_mask"), 6, "valid_mask"))

    if "follower_joint_velocities" in payload:
        errors.extend(
            _validate_numeric_array(
                payload.get("follower_joint_velocities"),
                6,
                "follower_joint_velocities",
            )
        )

    if "motor_positions_deg" in payload:
        errors.extend(_validate_numeric_array(payload.get("motor_positions_deg"), 4, "motor_positions_deg"))

    if "motor_velocities_deg_s" in payload:
        errors.extend(
            _validate_numeric_array(payload.get("motor_velocities_deg_s"), 4, "motor_velocities_deg_s")
        )

    if "motor_errors_deg" in payload:
        errors.extend(_validate_numeric_array(payload.get("motor_errors_deg"), 4, "motor_errors_deg"))

    if "motor_currents_ma" in payload:
        errors.extend(_validate_numeric_array(payload.get("motor_currents_ma"), 4, "motor_currents_ma"))

    for int_field in ("motors_en", "fault_code", "homing", "telemetry_rate_hz"):
        if int_field in payload and not _is_int(payload.get(int_field)):
            errors.append(f"{int_field} must be int")

    return errors


def validate_log_record(record: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    if not _is_int(record.get("bridge_ts")):
        errors.append("bridge_ts must be int")

    direction = record.get("direction")
    if direction not in ("leader_to_follower", "follower_to_laptop"):
        errors.append("direction must be leader_to_follower or follower_to_laptop")

    payload = record.get("payload")
    if not isinstance(payload, dict):
        errors.append("payload must be object")
        return errors

    if direction == "leader_to_follower":
        errors.extend(validate_joint_state_payload(payload))
    elif direction == "follower_to_laptop":
        errors.extend(validate_telemetry_state_payload(payload))

    return errors
