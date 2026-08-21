# JSON Message Protocol for SixEyes UART Communication

**Status**: LEGACY — this is the single-MCU USB-CDC protocol used by Alpha/Beta. v1's distributed CAN-node hardware uses a separate protocol; see `docs/protocols/CAN_MESSAGE_PROTOCOL.md` (once written) for the current spec.

Working reference for the extensible JSON message protocol used in SixEyes firmware for UART-based communication with ROS2 and external systems.

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Message Format](#message-format)
3. [Message Types](#message-types)
4. [Command Messages (ROS2 → ESP32)](#command-messages-ros2--esp32)
5. [Response Messages (ESP32 → ROS2)](#response-messages-esp32--ros2)
6. [Error Handling](#error-handling)
7. [Integration Examples](#integration-examples)
8. [Performance & Constraints](#performance--constraints)
9. [Backward Compatibility](#backward-compatibility)

---

## Protocol Overview

### Design Goals

✅ **Extensible**: New message types without breaking existing code
✅ **Human-Readable**: JSON format for debugging in terminal
✅ **Non-Blocking**: Parser never stalls the control loop
✅ **Type-Safe**: C++ polymorphism for message handling
✅ **Efficient**: Minimal RAM overhead (~512 bytes buffer)
✅ **Flexible**: Optional fields for future additions

### Transport Layer

| Property | Value |
|:---------|:--------|
| **Port** | USB CDC (Virtual Serial) or UART2 |
| **Baud Rate** | 115200 bps |
| **Frame Format** | Line-delimited JSON (newline = `\n`) |
| **Encoding** | UTF-8 (ASCII compatible) |
| **Max Message Size** | 512 bytes (configurable) |
| **Duplex** | Full duplex (RX and TX simultaneous) |

---

## Message Format

### Basic Structure

```json
{
  "cmd": "MESSAGE_TYPE",
  "seq": 42,
  "ts": 1000500,
  ... type-specific fields ...
}
```

### Common Fields

| Field | Type | Required | Description |
|:------|:-----|:---------|:------------|
| `cmd` | string | ✓ Required | Message type identifier (e.g., "MOTOR_TARGET") |
| `seq` | uint32 | Optional | Sequence number for ack/tracking |
| `ts` | uint64 | Optional | Timestamp in milliseconds (firmware fills if omitted) |

### Field Types in JSON

```
Integer:  "count": 42
Float:    "value": 3.14
Boolean:  "enable": true
String:   "name": "Motor0"
Array:    "targets": [1.0, 2.0, 3.0, 4.0]
Object:   "config": {"param1": 1.0, "param2": 2.0}
```

---

## Message Types

### Command Message Types (implemented + planned)

| Type | Direction | Purpose | Status |
|:-----|:----------|:--------|:-------|
| `MOTOR_TARGET` | ROS2 → ESP32 | Set stepper motor target positions | ✅ Implemented |
| `SERVO_TARGET` | ROS2 → ESP32 | Set servo PWM values | ✅ Implemented |
| `ENABLE_MOTORS` | ROS2 → ESP32 | Enable/disable motor outputs | ✅ Implemented |
| `RESET_FAULT` | ROS2 → ESP32 | Clear fault state | ✅ Implemented |
| `HOME_ZERO` | ROS2/Laptop → ESP32 | Software zeroing (current pose as logical zero) | ✅ Implemented |
| `HOME_STALLGUARD` | ROS2/Laptop → ESP32 | Hybrid StallGuard homing (seek stall, backoff, re-approach) | ✅ Implemented |
| `JOINT_STATE` | Leader ESP32 → Follower/Laptop | Stream leader arm joint angles (teleoperation) | ✅ Implemented |
| `TUNE_PID` | ROS2 → ESP32 | Update PID gains | ✅ Implemented |
| `CONFIG_PARAM` | ROS2 → ESP32 | Set configuration parameter | ✅ Implemented |
| `CONFIG_SAVE` | ROS2 → ESP32 | Save config to EEPROM | 🔲 Planned |
| `CONFIG_RESET` | ROS2 → ESP32 | Reset to defaults | 🔲 Planned |
| `TELEMETRY_ENABLE` | ROS2 → ESP32 | Enable/disable telemetry | ✅ Implemented |
| `TELEMETRY_RATE` | ROS2 → ESP32 | Set telemetry rate (Hz) | ✅ Implemented |
| `HEARTBEAT` | ROS2 → ESP32 | Keep-alive signal | ✅ Implemented |

### Response Message Types (11 types)

| Type | Direction | Purpose | Status |
|:-----|:----------|:--------|:-------|
| `MOTOR_STATUS` | ESP32 → ROS2 | Current motor state | ✅ Implemented |
| `SERVO_STATUS` | ESP32 → ROS2 | Current servo state | ✅ Implemented |
| `SYSTEM_STATUS` | ESP32 → ROS2 | Overall system health | ✅ Implemented |
| `FAULT_DETAILS` | ESP32 → ROS2 | Detailed fault info | ✅ Implemented |
| `STATISTICS` | ESP32 → ROS2 | Runtime statistics | ✅ Implemented |
| `STATUS_HEARTBEAT` | ESP32 → ROS2 | Status keep-alive | ✅ Implemented |

### Teleoperation Streaming Messages (Phase 2)

| Type | Direction | Purpose | Status |
|:-----|:----------|:--------|:-------|
| `JOINT_STATE` | Leader ESP32 → Laptop/Follower | Stream leader joint angles + valid mask | ✅ Implemented in `leader_esp32` |
| `TELEMETRY_STATE` | Follower ESP32 → Laptop/Leader | Stream real follower feedback for sync/analysis | ✅ Implemented in `follower_esp32` |

---

## Teleoperation Streaming Messages (Phase 2)

These messages are used for the teleoperation pipeline and data collection flow.

### JOINT_STATE - Leader Joint Streaming

**Purpose**: Publish leader-arm joint angles continuously for mirroring.

**Source**: `sixeyes/firmware/leader_esp32`.

**Nominal rate**: 100 Hz (every 10 ms).

**Format**:
```json
{
  "cmd": "JOINT_STATE",
  "seq": 42,
  "ts": 1000500,
  "leader_joints": [0.0, 45.0, 90.0, 135.0, 90.0, 90.0],
  "valid_mask": [1, 1, 1, 1, 1, 1]
}
```

**Fields**:
| Field | Type | Description |
|:------|:-----|:------------|
| `leader_joints` | float[6] | Leader joint angles (degrees) |
| `valid_mask` | uint8[6] | Per-joint validity (`1` valid, `0` invalid/disconnected) |

### Leader Local Calibration Commands (USB-CDC)

These commands are consumed by `leader_esp32` over its own USB serial console and are used to set potentiometer home/zero before teleoperation runs.

| Command | Accepted Forms | Effect |
|:--------|:---------------|:-------|
| `HOME_ZERO` | `HOME_ZERO` or `{"cmd":"HOME_ZERO"}` | Captures current pot pose as the zero offset |
| `CAPTURE_ZERO` | `CAPTURE_ZERO` or `{"cmd":"CAPTURE_ZERO"}` | Alias for zero capture (same behavior as `HOME_ZERO`) |

Notes:
- Offsets are applied to outgoing `JOINT_STATE.leader_joints`.
- Offsets are runtime-only (reboot clears them).
- Success log line: `[CAL] Leader pot zero captured from current pose`.

### TELEMETRY_STATE - Follower Telemetry Streaming

**Purpose**: Return follower state during teleoperation (for sync, logging, model training).

**Status**: Implemented in follower teleoperation pipeline.

**Current format**:
```json
{
  "cmd": "TELEMETRY_STATE",
  "seq": 42,
  "ts": 1000500,
  "follower_joints": [0.0, 45.0, 89.5, 134.2, 90.0, 91.2],
  "follower_joint_velocities": [3.1, 5.4, 4.8, 0.0, 0.0, 0.0],
  "errors": [0.0, 0.2, 0.5, 0.8, 0.0, 1.2],
  "faults": [0, 0, 0, 0, 0, 0],
  "valid_mask": [1, 1, 1, 1, 1, 1],
  "motor_positions_deg": [0.0, 1125.0, -1125.0, 2240.0],
  "motor_velocities_deg_s": [80.0, 92.0, -92.0, 77.0],
  "motor_errors_deg": [1.0, -2.0, 2.0, 1.5],
  "motor_currents_ma": [850, 900, 900, 870],
  "motors_en": 1,
  "fault_code": 0,
  "homing": 0,
  "telemetry_rate_hz": 100
}
```

**Fields**:
| Field | Type | Description |
|:------|:-----|:------------|
| `follower_joints` | float[6] | Follower joint state in teleoperation joint space (base, shoulder, elbow, wrist pitch, wrist yaw, gripper) |
| `follower_joint_velocities` | float[6] | Estimated joint velocities in teleoperation joint space (deg/s) |
| `errors` | float[6] | Leader-minus-follower joint error (deg) for each teleop joint |
| `faults` | uint8[6] | Per-joint fault indicator (global/system + input validity aware) |
| `valid_mask` | uint8[6] | Echo of most recent leader validity mask (`1` valid, `0` invalid) |
| `motor_positions_deg` | float[4] | Raw stepper motor shaft positions (deg) |
| `motor_velocities_deg_s` | float[4] | Raw stepper motor shaft velocities (deg/s) |
| `motor_errors_deg` | float[4] | Raw motor target-tracking errors (deg) |
| `motor_currents_ma` | uint16[4] | Configured/diagnostic current values from TMC2209 driver path |
| `motors_en` | uint8 | Motor enable state (`1` enabled, `0` disabled) |
| `fault_code` | uint32 | Current fault code from `FaultManager` |
| `homing` | uint8 | Homing state (`1` active, `0` idle) |
| `telemetry_rate_hz` | uint16 | Active telemetry stream rate |

**Emission behavior**:
- Emitted immediately after valid `JOINT_STATE` commands.
- Also emitted periodically from control loop at configured rate.
- Controlled by `TELEMETRY_ENABLE` and `TELEMETRY_RATE` commands.

---

## Command Messages (ROS2 → ESP32)

### 1. MOTOR_TARGET - Set Stepper Positions

**Purpose**: Command stepper motors to absolute positions

**Format**:
```json
{
  "cmd": "MOTOR_TARGET",
  "seq": 1,
  "targets": [0.0, 45.0, 90.0, 135.0]
}
```

**Fields**:
| Field | Type | Range | Description |
|:------|:-----|:------|:------------|
| `targets` | float[] | 0-360 deg | Absolute positions for each motor (degrees) |

**Constraints**:
- Maximum 4 values (NUM_STEPPERS)
- All motors updated atomically
- Values beyond 0-360 wrap around

**Response**:
ESP32 will execute motion and send `MOTOR_STATUS` at telemetry rate.

**Example**: Move all motors to safe position
```bash
echo '{"cmd":"MOTOR_TARGET","seq":1,"targets":[90.0,90.0,90.0,90.0]}' > /dev/ttyUSB0
```

---

### 2. SERVO_TARGET - Set Servo PWM

**Purpose**: Command servo motors to specific PWM or degrees

**Format (PWM microseconds)**:
```json
{
  "cmd": "SERVO_TARGET",
  "seq": 2,
  "pwm_us": [1500, 1400, 1600]
}
```

**Format (Degrees)**:
```json
{
  "cmd": "SERVO_TARGET",
  "seq": 2,
  "degrees": [90, 80, 100]
}
```

**Fields**:
| Field | Type | Range | Description |
|:------|:-----|:------|:------------|
| `pwm_us` (alt) | float[] | 1000-2000 µs | PWM pulse width in microseconds |
| `degrees` (alt) | float[] | 0-180 deg | Servo position in degrees |

**Constraints**:
- Use either `pwm_us` OR `degrees`, not both
- Default range: 1000-2000 µs (standard servo)
- 1500 µs = 90° (neutral)

**Conversion**:
- 0° → 1000 µs (full counter-clockwise)
- 90° → 1500 µs (neutral)
- 180° → 2000 µs (full clockwise)

**Example**: Open gripper (0°)
```bash
echo '{"cmd":"SERVO_TARGET","seq":2,"degrees":[0,90,90]}' > /dev/ttyUSB0
```

---

### 3. ENABLE_MOTORS - Control Motor Power

**Purpose**: Enable or disable all motor outputs

**Format**:
```json
{
  "cmd": "ENABLE_MOTORS",
  "seq": 3,
  "enable": true
}
```

**Fields**:
| Field | Type | Description |
|:------|:-----|:------------|
| `enable` | bool | true=enable, false=disable |

**Safety Notes**:
- Disabling immediately sets EN pin LOW
- Motors freewheel when disabled
- Does NOT affect current faults
- Requires explicit RESET_FAULT to recover

**Example**: Disable motors for safety
```bash
echo '{"cmd":"ENABLE_MOTORS","seq":3,"enable":false}' > /dev/ttyUSB0
```

---

### 4. RESET_FAULT - Clear Fault State

**Purpose**: Acknowledge fault and attempt recovery

**Format**:
```json
{
  "cmd": "RESET_FAULT",
  "seq": 4,
  "ack": true
}
```

**Fields**:
| Field | Type | Description |
|:------|:-----|:------------|
| `ack` | bool (optional) | Acknowledgment flag (default: false) |

**Behavior**:
1. Clears latched fault bits
2. Re-enables motors if safe
3. Logs fault details for debugging

**Example**: Clear fault after thermal shutdown
```bash
echo '{"cmd":"RESET_FAULT","seq":4}' > /dev/ttyUSB0
```

---

### 5. TUNE_PID - Update PID Gains

**Purpose**: Dynamically adjust motor PID controller parameters

**Format**:
```json
{
  "cmd": "TUNE_PID",
  "seq": 5,
  "motor": 0,
  "kp": 2.5,
  "ki": 0.1,
  "kd": 0.2,
  "antiwindup": true
}
```

**Fields**:
| Field | Type | Default | Description |
|:------|:-----|:--------|:------------|
| `motor` | uint8 | 0 | Motor index (0-3) |
| `kp` | float | 2.0 | Proportional gain |
| `ki` | float | 0.05 | Integral gain |
| `kd` | float | 0.1 | Derivative gain |
| `antiwindup` | bool | true | Enable anti-windup |

**Tuning Guidelines**:
- **High Kp**: Fast response, but may oscillate
- **High Ki**: Eliminates steady-state error, but slow response
- **High Kd**: Damping, but noise-sensitive
- **Start with**: Kp=2.0, Ki=0.05, Kd=0.1

**Example**: Increase responsiveness
```bash
echo '{"cmd":"TUNE_PID","seq":5,"motor":0,"kp":3.0,"ki":0.08,"kd":0.15}' > /dev/ttyUSB0
```

---

### 4a. HOME_ZERO - Latch Current Position as Zero

**Purpose**: Define current stepper positions as logical joint zero.

**Format**:
```json
{
  "cmd": "HOME_ZERO",
  "seq": 40
}
```

**Behavior**:
1. Latches each motor's current position as zero offset
2. Resets target/current logical joint deltas around new zero frame
3. Keeps safety and fault state unchanged

**Example**:
```bash
echo '{"cmd":"HOME_ZERO","seq":40}' > /dev/ttyUSB0
```

---

### 4b. HOME_STALLGUARD - Hybrid Mechanical + Logical Homing

**Purpose**: Execute coarse mechanical reference via StallGuard, then align logical zero.

**Format**:
```json
{
  "cmd": "HOME_STALLGUARD",
  "seq": 41
}
```

**Behavior**:
1. Enables StallGuard check on configured axes
2. Seeks stall in homing direction
3. Backs off and re-approaches for repeatability
4. Stores the homed position as logical zero offset

**Example**:
```bash
echo '{"cmd":"HOME_STALLGUARD","seq":41}' > /dev/ttyUSB0
```

---

### 6. CONFIG_PARAM - Set Configuration Parameter

**Purpose**: Modify firmware configuration parameters

**Format**:
```json
{
  "cmd": "CONFIG_PARAM",
  "seq": 6,
  "param": "MAX_MOTOR_SPEED",
  "value": 300.0
}
```

**Available Parameters**:
| Parameter | Type | Default | Description |
|:----------|:-----|:--------|:------------|
| `MAX_MOTOR_SPEED` | float | 360.0 | Max motor speed (deg/s) |
| `MAX_SERVO_SPEED` | float | 60.0 | Max servo speed (deg/s) |
| `HEARTBEAT_TIMEOUT_MS` | uint32 | 500 | ROS2 heartbeat timeout |
| `MOTOR_CURRENT_LIMIT_MA` | float | 1500.0 | Stepper current limit |

**Example**: Reduce max motor speed for safety
```bash
echo '{"cmd":"CONFIG_PARAM","seq":6,"param":"MAX_MOTOR_SPEED","value":180.0}' > /dev/ttyUSB0
```

---

### 7. TELEMETRY_ENABLE - Enable/Disable Telemetry Stream

**Purpose**: Control whether ESP32 sends unsolicited status updates

**Format**:
```json
{
  "cmd": "TELEMETRY_ENABLE",
  "seq": 7,
  "enable": true
}
```

**Behavior**:
- When enabled: ESP32 sends `STATUS_HEARTBEAT` at configured rate
- When disabled: Only send responses to queries
- Default: Enabled after startup

**Example**: Stop telemetry for bandwidth testing
```bash
echo '{"cmd":"TELEMETRY_ENABLE","seq":7,"enable":false}' > /dev/ttyUSB0
```

---

### 8. TELEMETRY_RATE - Set Telemetry Update Rate

**Purpose**: Change how often ESP32 broadcasts status

**Format**:
```json
{
  "cmd": "TELEMETRY_RATE",
  "seq": 8,
  "hz": 10
}
```

**Fields**:
| Field | Type | Range | Default | Description |
|:------|:-----|:------|:--------|:------------|
| `hz` | uint16 | 1-100 | 10 | Update frequency in Hz |

**Constraints**:
- Control loop runs at 400 Hz
- Telemetry rate should be ≤ 100 Hz
- Each packet is ~200 bytes
- At 100 Hz: ~20 KB/s bandwidth

**Example**: Increase telemetry rate for diagnostics
```bash
echo '{"cmd":"TELEMETRY_RATE","seq":8,"hz":50}' > /dev/ttyUSB0
```

---

### 9. HEARTBEAT - Keep-Alive Signal (JSON Version)

**Purpose**: Tell ESP32 that ROS2 is still alive

**Format**:
```json
{
  "cmd": "HEARTBEAT",
  "seq": 123
}
```

**Behavior**:
- Updates ROS2 heartbeat timer (prevents motor disable)
- Response: `STATUS_HEARTBEAT` sent at next telemetry interval
- Default rate from ROS2: ≥50 Hz

**Note**: ASCII version (`HB:0,N\n`) still supported for backward compatibility.

**Example**: Send heartbeat at 50 Hz
```bash
while true; do
  echo '{"cmd":"HEARTBEAT","seq":'$(( $(date +%s%N) / 1000000 ))'}' > /dev/ttyUSB0
  sleep 0.02  # 50 Hz = 20 ms
done
```

---

## Response Messages (ESP32 → ROS2)

All response messages are unsolicited updates sent at the configured telemetry rate, unless otherwise noted.

### 1. MOTOR_STATUS - Motor State Update

**Format**:
```json
{
  "cmd": "MOTOR_STATUS",
  "seq": 100,
  "motor": 0,
  "pos": 45.23,
  "vel": 12.5,
  "current": 1.2,
  "temp": 42.0,
  "state": 0
}
```

**Fields**:
| Field | Type | Unit | Description |
|:------|:-----|:-----|:------------|
| `motor` | uint8 | - | Motor index (0-3) |
| `pos` | float | deg | Current position |
| `vel` | float | deg/s | Current velocity |
| `current` | float | mA | Motor current draw |
| `temp` | float | °C | Driver temperature |
| `state` | uint8 | - | 0=idle, 1=moving, 2=fault |

**Frequency**: Sent at telemetry rate for each motor (one msg per motor per update)

---

### 2. SERVO_STATUS - Servo State Update

**Format**:
```json
{
  "cmd": "SERVO_STATUS",
  "seq": 101,
  "servo": 0,
  "pwm_us": 1500,
  "current": 250,
  "temp": 35.0,
  "state": 0
}
```

**Fields**:
| Field | Type | Unit | Description |
|:------|:-----|:-----|:------------|
| `servo` | uint8 | - | Servo index (0-2) |
| `pwm_us` | float | µs | Current PWM pulse width |
| `current` | float | mA | Servo current |
| `temp` | float | °C | Estimated temperature |
| `state` | uint8 | - | 0=idle, 1=moving, 2=error |

---

### 3. SYSTEM_STATUS - Overall Health Report

**Format**:
```json
{
  "cmd": "SYSTEM_STATUS",
  "seq": 102,
  "uptime_ms": 12345600,
  "heap_free": 65536,
  "heap_total": 327680,
  "core0_cpu": 25.3,
  "core1_cpu": 5.2,
  "fault_count": 0,
  "motors_en": 1,
  "ros2_alive": 1
}
```

**Fields**:
| Field | Type | Unit | Description |
|:------|:-----|:-----|:------------|
| `uptime_ms` | uint32 | ms | System uptime since boot |
| `heap_free` | uint32 | bytes | Available heap memory |
| `heap_total` | uint32 | bytes | Total heap memory |
| `core0_cpu` | float | % | Core 0 CPU utilization |
| `core1_cpu` | float | % | Core 1 CPU utilization |
| `fault_count` | uint8 | - | Number of faults occurred |
| `motors_en` | bool | - | Motor outputs enabled |
| `ros2_alive` | bool | - | ROS2 heartbeat received |

---

### 4. FAULT_DETAILS - Fault Information

**Format**:
```json
{
  "cmd": "FAULT_DETAILS",
  "seq": 103,
  "fault_code": 0x0001,
  "description": "Motor 0 overcurrent",
  "timestamp_ms": 12345600,
  "severity": 2,
  "is_latched": true
}
```

**Fault Codes**:
| Code | Name | Severity | Description |
|:-----|:-----|:---------|:------------|
| 0x0001 | MOTOR_OVERCURRENT | HIGH | Motor current exceeded limit |
| 0x0002 | THERMAL_FAULT | HIGH | Driver overtemperature |
| 0x0004 | STALL_DETECTED | MEDIUM | Stepper stall detected |
| 0x0008 | HEARTBEAT_TIMEOUT | HIGH | ROS2 heartbeat lost |
| 0x0010 | UART_ERROR | LOW | Communication error |

---

### 5. STATUS_HEARTBEAT - Keep-Alive Response

**Format**:
```json
{
  "cmd": "STATUS_HEARTBEAT",
  "seq": 200,
  "fault": 0,
  "motors_en": 1,
  "ros2_alive": 1,
  "freq": 400.2
}
```

**Fields**:
| Field | Type | Description |
|:------|:-----|:------------|
| `fault` | uint8 | Fault present flag (0=no fault, 1=fault) |
| `motors_en` | uint8 | Motors enabled (0=disabled, 1=enabled) |
| `ros2_alive` | uint8 | ROS2 heartbeat received (0=timeout, 1=alive) |
| `freq` | float | Measured control loop frequency |

**Purpose**: Compact status update sent at telemetry rate

---

## Error Handling

### Parse Errors

If the ESP32 cannot parse a message, it responds with error details:

```json
{
  "error": "Missing required field: cmd",
  "line": 42,
  "timestamp_ms": 12345600
}
```

**Common Errors**:
| Error | Cause | Solution |
|:------|:------|:---------|
| `Missing required field: cmd` | No "cmd" field in JSON | Add "cmd" field |
| `Unknown command type: TYPO` | Wrong message type name | Check spelling |
| `RX buffer overflow` | Message too long | Split into smaller messages |
| `Handler table full` | Too many handlers registered | Unregister unused handlers |

### Message Validation

```
INPUT: "{"cmd":"MOTOR_TARGET"}\n"    (missing "targets")
OUTPUT: [Error logged, message dropped]

INPUT: "{"cmd":"SERVO_TARGET","degrees":[1,2]}\n"
OUTPUT: Processed successfully (2 servos updated, 3rd unchanged)
```

---

## Integration Examples

### Example 1: Basic Motor Control Loop (Python/ROS2)

```python
#!/usr/bin/env python3
import serial
import json
import time

# Connect to ESP32
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# Send motor targets
motor_targets = [0.0, 45.0, 90.0, 135.0]
msg = {
    "cmd": "MOTOR_TARGET",
    "seq": 1,
    "targets": motor_targets
}
ser.write((json.dumps(msg) + '\n').encode())

# Wait for response
time.sleep(0.5)
while ser.in_waiting:
    response = ser.readline().decode().strip()
    data = json.loads(response)
    print(f"Motor {data['motor']}: pos={data['pos']:.2f}°, current={data['current']:.2f} mA")
```

### Example 2: PID Tuning (Bash)

```bash
#!/bin/bash
# Tune Motor 0 for faster response

echo "Current gains..."
echo '{"cmd":"TUNE_PID","seq":1,"motor":0,"kp":2.0,"ki":0.05,"kd":0.1}' > /dev/ttyUSB0
sleep 0.1

echo "Increased Kp..."
echo '{"cmd":"TUNE_PID","seq":2,"motor":0,"kp":3.0,"ki":0.05,"kd":0.1}' > /dev/ttyUSB0
sleep 1

echo "Check for oscillation..."
# Read 20 motor status updates
for i in {1..20}; do
  read -t 0.1 status < /dev/ttyUSB0
  echo "$status" | grep MOTOR_STATUS && echo "$status" | jq '.pos'
done
```

### Example 3: Fault Recovery (C++)

```cpp
#include <serial/serial.h>
#include <ArduinoJson.h>

serial::Serial ser("/dev/ttyUSB0", 115200);

void handleFaultRecovery() {
    // 1. Disable motors (safe state)
    StaticJsonDocument<256> disable_msg;
    disable_msg["cmd"] = "ENABLE_MOTORS";
    disable_msg["seq"] = 10;
    disable_msg["enable"] = false;
    
    String output;
    serializeJson(disable_msg, output);
    ser.write(output + "\n");
    
    // 2. Wait for fault to clear
    delay(2000);
    
    // 3. Reset fault
    StaticJsonDocument<256> reset_msg;
    reset_msg["cmd"] = "RESET_FAULT";
    reset_msg["seq"] = 11;
    
    serializeJson(reset_msg, output);
    ser.write(output + "\n");
    
    // 4. Re-enable motors
    disable_msg["enable"] = true;
    disable_msg["seq"] = 12;
    serializeJson(disable_msg, output);
    ser.write(output + "\n");
}
```

### Example 4: Teleoperation Bridge (Leader → Follower)

```bash
cd sixeyes/tools
python teleoperation_bridge.py --leader-port COM5 --follower-port COM6 --log-file teleop_session.jsonl
```

Behavior:
- Reads line-delimited JSON from leader serial.
- Forwards validated `JOINT_STATE` payloads to follower serial.
- Receives `TELEMETRY_STATE` from follower in parallel (joint + motor feedback payload).
- Drops malformed packets and tracks counters.
- Optionally writes both directions to JSONL for dataset collection.

### Example 5: Operator Command Sequence (Python Helper)

```bash
cd sixeyes/tools
python operator_control.py --port COM6 teleop-ready
python operator_control.py --port COM6 home
python operator_control.py --port COM6 stallguard-home
```

Behavior:
- Sends heartbeat and safety-aware command sequence
- Enables motors and supports fault reset flow
- Triggers software zero or StallGuard hybrid homing

---

## Performance & Constraints

### Bandwidth

| Metric | Value |
|:-------|:------|
| Baud Rate | 115200 bps |
| Bytes per Bit | ~87 µs |
| Average Message Size | 150-300 bytes |
| Max Message Rate | ~100-300 messages/sec (without saturation) |
| Typical Telemetry @ 10 Hz | ~1.5-3 KB/sec |

### Latency

| Operation | Latency | Notes |
|:----------|:--------|:------|
| Parse + Dispatch | < 1 ms | Depends on JSON complexity |
| Motor Response | 2.5 ms | Next control loop execution |
| Servo Response | 2.5 ms | Next control loop execution |
| Status Telemetry | 100 ms | At 10 Hz default rate |

### Memory

```
RX Buffer:              512 bytes (configurable)
JSON Document:          512 bytes (stack-allocated per message)
Handler Table:          ~100 bytes (16 max handlers)
─────────────────────────────────
Total Overhead:         ~1.1 KB
Available After Parsing: 319 KB (of 320 KB total)
```

---

## Backward Compatibility

### ASCII Heartbeat Support

The firmware continues to support the original ASCII heartbeat protocol:

```
ASCII (Still Supported):     HB:0,42\n
JSON (New Alternative):      {"cmd":"HEARTBEAT","seq":42}\n
```

Both formats are accepted and equivalent. Mixing them in the same stream is safe.

### Migration Path

1. **Phase 1** (Current): ASCII heartbeats work, JSON optional
2. **Phase 2** (v1.1.0): Both protocols supported equally
3. **Phase 3** (v2.0.0): JSON preferred, ASCII deprecated (but still work)

### Coexistence

```
ROS2 → ESP32:
  50 Hz ASCII heartbeats   (HB:0,N)
  + occasional JSON cmds   ({"cmd":"TUNE_PID",...})

ESP32 → ROS2:
  Status heartbeats        (SB:f,e,r ASCII)
  + JSON telemetry         (STATUS_HEARTBEAT JSON)
```

---

## Extensions & Future Work

### Planned Message Types

- **CONFIG_SAVE / CONFIG_RESET**: EEPROM management
- **MOTOR_VELOCITY_TARGET**: Direct velocity commands
- **ADVANCED_PID**: Gain scheduling, feed-forward
- **TRAJECTORY_PLANNING**: Multi-point motion profiles
- **VISION_FEEDBACK**: Camera integration messages

### Custom Message Support

The parser is designed to be extended. Example custom message:

```cpp
// In main.cpp, during initialization:
UartJsonParser::instance().registerHandler(
    MessageType::MOTOR_TARGET,
    [](const BaseMessage& msg) {
        auto* motor_msg = static_cast<const MotorTargetMessage*>(&msg);
        Serial.printf("Received targets: [%.1f, %.1f, %.1f, %.1f]\n",
            motor_msg->targets[0], motor_msg->targets[1],
            motor_msg->targets[2], motor_msg->targets[3]);
    }
);
```

---

## Testing & Validation

### Unit Test Example

```bash
# Test 1: Valid motor command
echo '{"cmd":"MOTOR_TARGET","seq":1,"targets":[45,90,135,180]}' > /dev/ttyUSB0
# Expected: Message parsed, motors move to targets

# Test 2: Invalid JSON
echo '{invalid json}' > /dev/ttyUSB0
# Expected: Error logged, message ignored

# Test 3: Missing field
echo '{"cmd":"MOTOR_TARGET","seq":1}' > /dev/ttyUSB0
# Expected: Error (missing "targets" field)

# Test 4: Mixed protocol
echo 'HB:0,100' > /dev/ttyUSB0
echo '{"cmd":"HEARTBEAT","seq":101}' > /dev/ttyUSB0
# Expected: Both heartbeats work
```

---

**JSON protocol is actively evolving alongside firmware and ROS2 integration.**

For implementation details, see `sixeyes/firmware/follower_esp32/src/modules/comms/uart_json_parser/uart_json_parser.cpp` and `sixeyes/firmware/follower_esp32/src/modules/comms/uart_json_parser/uart_json_messages.h`.
