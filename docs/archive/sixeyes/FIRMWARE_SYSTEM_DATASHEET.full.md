# SixEyes Firmware System Datasheet

**Document ID**: SE-FW-DS-001  
**Version**: 0.1 (Draft)  
**Last Updated**: February 2026  
**Maturity**: Engineering Development (not a production release)

---

## 0) How to Use This Datasheet

Use this document as a quick, high-level firmware snapshot.

- Read this first for interfaces, modes, safety behavior, and current limits.
- Use linked deep-dive docs for implementation details, procedures, and troubleshooting.
- Treat this as a living document: if firmware behavior changes, update this sheet and bump the version.

### Revision History

| Version | Date | Summary |
|:--------|:-----|:--------|
| 0.1 | Feb 2026 | Initial firmware system datasheet created |

---

## 1) Product Overview

SixEyes firmware is a dual-mode embedded control stack for ESP32-based robotic arm controllers, designed for safety-gated motion execution, teleoperation workflows, and ROS2-integrated operation.

This system currently targets:
- Deterministic follower control on ESP32-S3
- Leader-side joint streaming on ESP32
- Laptop-supervised safety, orchestration, and logging

---

## 2) System Scope

**In scope**
- Follower firmware (`sixeyes/firmware/follower_esp32`)
- Leader streaming firmware (`sixeyes/firmware/leader_esp32`)
- Host-side teleoperation utilities (`sixeyes/tools/`)

**Related docs**
- Architecture: `docs/firmware/VISUAL_ARCHITECTURE_GUIDE.md`
- Protocol: `docs/protocols/JSON_MESSAGE_PROTOCOL.md`
- Deployment: `docs/deployment/FLASHING_AND_DEPLOYMENT.md`

---

## 3) Supported Hardware (Current Target)

| Item | Current Target |
|:-----|:---------------|
| MCU (Follower) | ESP32-S3 |
| MCU (Leader) | ESP32 |
| Stepper Drivers | TMC2209 |
| Stepper Axes | 4 |
| Servo Channels | 3 |
| Primary Host Link | USB-CDC Serial |

### 3.1 Power Architecture (PCB Revision)

| Rail | Source | Primary Loads |
|:-----|:-------|:--------------|
| 24V | Main DC input (≥10A recommended system budget) | TMC2209 VM + onboard buck converters |
| 6.6V | XL4016 buck on follower PCB | Servo power rail (wrist pitch/yaw + gripper) |
| 3.3V | MP1584 buck on follower PCB | ESP32 logic, TMC2209 VIO, digital/UART logic |

Grounding model: common star-point grounding near main power entry; servo return currents isolated from quiet logic ground paths.

---

## 4) Operating Modes

Mode selection is compile-time on follower firmware (`OPERATION_MODE`).

| Mode | Flag | Role |
|:-----|:-----|:-----|
| VLA Inference | `OPERATION_MODE=1` | Laptop/ROS2 issues commands to follower |
| Teleoperation | `OPERATION_MODE=2` | Leader joint stream mirrored/processed by follower pipeline |

---

## 5) External Interfaces

### 5.1 Safety Heartbeat (ASCII)
- Host → Follower: `HB:source,seq\n`
- Follower → Host: `SB:fault,motors_enabled,ros2_alive\n`

### 5.2 JSON Command Interface
- Transport: line-delimited JSON over USB-CDC/UART
- Typical command families:
  - Motion/actuation: `MOTOR_TARGET`, `SERVO_TARGET`
  - Safety/control: `ENABLE_MOTORS`, `RESET_FAULT`
  - Homing: `HOME_ZERO`, `HOME_STALLGUARD`
  - Teleop stream: `JOINT_STATE` (with `TELEMETRY_STATE` expansion ongoing)

---

## 6) Motion Control Characteristics (Current Implementation)

| Characteristic | Current Implementation |
|:---------------|:-----------------------|
| Control scheduling | 400 Hz control loop framework |
| Step generation | Timer-driven step pulse generation |
| Trajectory shaping | Accel/jerk-limited planner behavior |
| Multi-axis behavior | Synchronized planning by remaining-distance scaling |
| Shoulder constraint | Anti-phase lock for shoulder pair |
| Homing options | Software zero + hybrid StallGuard homing |

---

## 7) Safety Characteristics

| Item | Target Behavior |
|:-----|:----------------|
| Heartbeat timeout | Motor disable when heartbeat path is unhealthy |
| Fault handling | Fault latch + explicit reset path |
| Motor enable gating | Commanded enable must pass safety conditions |
| Teleop loss behavior | Tight timeout behavior for missing teleop stream (under active integration) |

---

## 8) Host Tooling

Current operator/bring-up scripts:
- `sixeyes/tools/teleoperation_bridge.py`
- `sixeyes/tools/operator_control.py`

Example bring-up sequence (teleoperation follower):
```bash
python sixeyes/tools/operator_control.py --port COM6 teleop-ready
python sixeyes/tools/operator_control.py --port COM6 home
python sixeyes/tools/operator_control.py --port COM6 stallguard-home
```

---

## 9) Known Gaps / Limits (As of Feb 2026)

- Full follower `TELEMETRY_STATE` payload still being expanded.
- End-to-end teleop ROS2 runtime path remains under active development.
- Integration/regression coverage for leader→bridge→follower is not yet complete.
- Hardware validation is iterative; not all scenarios are closed for release qualification.

---

## 10) Verification Snapshot

| Area | Current State |
|:-----|:--------------|
| Follower firmware build | Passing |
| Leader firmware build | Passing baseline |
| Unit tests | Baseline tests available |
| Hardware validation | Procedures defined; iterative execution ongoing |

---

## 11) Change Control

This datasheet is a living specification and should be updated whenever one or more of the following changes occur:
- New external commands or message schema changes
- Safety behavior changes (timeouts, fault policy)
- Motion control architecture changes (timing/planner/homing)
- Mode behavior changes (VLA/Teleop)

Owner: SixEyes firmware maintainers.
