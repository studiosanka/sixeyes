# ROS2 Integration Guide

**Status**: LEGACY — describes the ASCII heartbeat (`HB:`/`SB:`) + JSON protocol used by Alpha/Beta only. v1's distributed CAN-node hardware needs a CAN-relay rework of `usb_bridge_node`; see `docs/protocols/CAN_MESSAGE_PROTOCOL.md` and `docs/V1_TODO.md`.

Covers the heartbeat safety protocol and ROS2 node architecture for legacy SixEyes hardware. Applies to both Alpha and Beta.

---

## Safety Heartbeat Protocol

**ROS2 → ESP32** (≥50 Hz):
```
HB:<source_id>,<seq>\n        e.g. HB:0,42\n
```

**ESP32 → ROS2** (10 Hz):
```
SB:<fault>,<motors_en>,<ros2_alive>\n    e.g. SB:0,1,1\n
```

`fault` = uint32 bitmask (0 = no fault). `motors_en` = 1 when SafetyTask gate is open. `ros2_alive` = 1 when heartbeat timeout not triggered.

**Timeout**: 500 ms without a `HB:` packet → motors disabled, `fault` latched. Recovery requires new heartbeat + `RESET_FAULT` command.

**Latency**: Detection in <2.5 ms (one 400/500 Hz control loop cycle).

---

## Data Flow

```
ROS2 safety_node
  ↓ HB:0,seq\n (≥50 Hz via USB-CDC)
HeartbeatReceiver → HeartbeatMonitor → SafetyTask
                                          ↓ EN gate (motor enable/disable)
                                       HeartbeatTransmitter → SB:fault,en,alive\n (10 Hz)
  ↑ (USB-CDC back to ROS2)
```

All heartbeat processing runs inside the 400/500 Hz FreeRTOS control loop on core 0. It never blocks.

---

## ROS2 Nodes

| Node | Role | Key Topics |
|------|------|-----------|
| `usb_bridge_node` | Owns serial port; HB TX at 50 Hz, telemetry RX, command TX | pub: `/sixeyes/firmware_status`, `/sixeyes/joint_states` |
| `safety_node` | Parses SB: packets, publishes safety flag | pub: `/sixeyes/is_safe` (Bool) |
| `joint_state_node` | Converts `/sixeyes/joint_states` (deg) → `/joint_states` (rad) | — |
| `camera_node` | USB cam → image topic | pub: `/camera/image_raw` |
| `vla_inference_node` | VLA inference stub (not yet implemented) | — |

Launch:
```bash
ros2 launch sixeyes_bringup teleop.launch.py port:=/dev/ttyACM0 heartbeat_hz:=50
ros2 launch sixeyes_bringup vla.launch.py port:=/dev/ttyACM0
```

---

## Quick Test (Manual)

```bash
# 1. Open serial monitor
picocom /dev/ttyACM0 -b 115200   # Linux
# PuTTY COM3, 115200              # Windows

# 2. Verify startup messages appear (all modules init, waiting for heartbeat)

# 3. Send a heartbeat
echo "HB:0,0" > /dev/ttyACM0

# 4. Expected: "EN pin HIGH - motors enabled", then SB:0,1,1 at 10 Hz

# 5. Stop sending for 500ms — expected: motors disable, SB:1,0,0
```

For a real ROS2 heartbeat bridge (not a standalone script), run `usb_bridge_node` from `ros2_ws/src/usb_bridge_node/` — it owns the serial port and handles heartbeat TX / status RX as an actual ROS2 node. See the Node architecture section below.

---

## Timing Guarantees

| Component | Frequency | Blocking? |
|-----------|-----------|-----------|
| Heartbeat RX parse | per-packet | No |
| SafetyTask HB check | 400/500 Hz | No |
| Heartbeat TX (SB:) | 10 Hz | No |
| Motor disable latency | <2.5 ms | — |

---

## Configuration (board_config.h)

```cpp
SAFETY_HEARTBEAT_TIMEOUT_MS = 500   // ROS2 heartbeat timeout
```

Build flag override:
```ini
build_flags = -DSAFETY_HEARTBEAT_TIMEOUT_MS=500
```

---

## Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Port not found | Driver missing | Install CH340 driver; Linux: `sudo usermod -a -G dialout $USER` |
| No output | Baud mismatch | Verify 115200 both sides |
| Motors don't enable | SafetyTask not init | Check `SafetyTask::init(&Serial)` called with correct UART |
| Timeout triggers immediately | Junk UART data / EMI | Check USB cable quality; send HB at ≥50 Hz |
| `Dropped > 0` | USB CDC buffer overflow | Reduce telemetry rate; check for serial bottleneck |

---

## Error Scenarios

| Scenario | Timeline | Action |
|----------|----------|--------|
| Heartbeat lost | t=0 last HB; t=500ms timeout | Motors disable, `SB:1,0,0` latched |
| Firmware fault (e.g. stall) | Immediate on detection | Motors disable, `SB:1,0,1` |
| Standalone (no ROS2 UART) | Internal HB monitor only | Motors use 500ms internal timeout; no SB: packets sent |
