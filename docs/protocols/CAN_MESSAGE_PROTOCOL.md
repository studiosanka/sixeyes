# CAN Message Protocol — v1 Joint Bus

**Status**: Current — authoritative protocol for v1 Universal Joint PCB hardware (`docs/hardware/v1/v1_PCB_Design_Reference.md`).
**Supersedes**: `docs/protocols/legacy/JSON_MESSAGE_PROTOCOL.md` (single-MCU USB-CDC protocol, Alpha/Beta only).
**Transport**: TWAI (CAN 2.0A, 11-bit standard ID), 1 Mbps, linear daisy chain Base → Shoulder L → Shoulder R → Elbow, 120 Ω termination at Base and Elbow only.

This is a design document, not yet implemented in firmware. It defines node addressing, message framing, and the distributed safety/heartbeat model needed before `firmware/v1/joint_node/` CAN and safety modules can be written.

---

## 1. Why This Replaces the JSON Protocol

The legacy protocol assumed one laptop-facing MCU (the Beta follower) directly owning every stepper and servo over local GPIO, with a single point-to-point USB-CDC link carrying ASCII heartbeat (`HB:`/`SB:`) and JSON commands. v1 has no single controller: four ESP32-C6-MINI-1 nodes (Base, Shoulder L, Shoulder R, Elbow) each own their own stepper (and Elbow additionally owns 3 open-loop servos), sharing one CAN bus. Only Base has a USB-C link to the laptop/ROS2 side.

Consequences for the protocol:
- Laptop commands must be addressed to a specific node, not broadcast implicitly to "the follower."
- Heartbeat/safety can no longer rely on one MCU's local watchdog — loss of the *bus* heartbeat, or loss of any *individual node*, must both be detectable and must both result in that node's motors disabling.
- Base is the only bridge to ROS2 — it either relays raw CAN frames over USB (transparent bridge) or terminates the JSON-over-USB link and re-encodes to CAN (translating bridge). This doc assumes **Base translates**: ROS2 still speaks a JSON/ASCII protocol to Base over USB (reusing `usb_bridge_node` mostly as-is), and Base fans commands out to CAN. This keeps ROS2-side code changes minimal — only `usb_bridge_node`'s downstream framing changes, not its message vocabulary.

## 2. Node Addressing

| Node | ID | Role |
|---|---|---|
| Base | 1 | Bus master role for heartbeat origination + USB bridge; also owns Stepper 1 |
| Shoulder L | 2 | Stepper 2A |
| Shoulder R | 3 | Stepper 2B (inverse-direction logic, see v1 hardware doc) |
| Elbow | 4 | Stepper 3 + 3× open-loop servo |

Node ID is a 3-bit field (values 1–4 used, 0 and 5–7 reserved) set per-board at flash time via a build flag (`-DJOINT_NODE_ID=N`), matching the "one firmware image, four roles" plan from the legacy-move discussion.

## 3. CAN ID Allocation (11-bit standard ID)

Priority on a CAN bus is arbitration-order, lowest numeric ID wins. Layout groups by descending priority:

| ID range | Class | Priority |
|---|---|---|
| `0x000` | E-STOP (bus-wide) | Highest — always wins arbitration |
| `0x010`–`0x017` | BUS_HEARTBEAT (Base → all) | High |
| `0x020`–`0x027` | NODE_STATUS (node → Base) | High |
| `0x100`–`0x10F` | MOTOR_TARGET (Base → node, per-node sub-ID) | Medium |
| `0x110`–`0x11F` | SERVO_TARGET (Base → Elbow only) | Medium |
| `0x200`–`0x20F` | ENCODER_TELEMETRY (node → Base, per-node sub-ID) | Low |
| `0x300`–`0x30F` | FAULT_REPORT (node → Base, per-node sub-ID) | Low |

Per-node sub-ID = class base + node ID (e.g. `MOTOR_TARGET` to Shoulder L = `0x100 + 2 = 0x102`).

## 4. Message Formats

All multi-byte fields little-endian. CAN 2.0A data payload is max 8 bytes; larger values are split across the field layout below, not multi-frame — every message here fits in one frame by design.

### 4.1 E-STOP — `0x000`, 1 byte, any sender
| Byte | Field |
|---|---|
| 0 | `reason` (0=laptop/ROS2 request, 1=heartbeat timeout, 2=node-detected fault, 3=manual button) |

Every node — including the one that raised it — treats *receipt* of this frame as an immediate, unconditional motor disable, regardless of source. No acknowledgement required; the frame itself is the action. Any node may transmit it (a stepper-detected stall on Elbow disables the whole arm, not just Elbow), which is why it is bus-wide rather than addressed.

### 4.2 BUS_HEARTBEAT — `0x010`, Base → all, ≥50 Hz
| Byte | Field |
|---|---|
| 0–3 | `sequence` (uint32, monotonic) |
| 4 | `ros2_alive` (0/1 — mirrors legacy `SB:` semantics: is the laptop-side heartbeat itself current) |

### 4.3 NODE_STATUS — `0x020 + node_id`, node → Base, 10 Hz
| Byte | Field |
|---|---|
| 0 | `fault_bitmask` |
| 1 | `motors_enabled` (0/1) |
| 2 | `stall_detected` (0/1, StallGuard-equivalent if retained on v1 — TBD pending encoder-only decision) |
| 3–6 | `encoder_position` (int32, latest SPI encoder reading, raw counts) |

### 4.4 MOTOR_TARGET — `0x100 + node_id`, Base → node
| Byte | Field |
|---|---|
| 0–3 | `target_position` (float32, degrees) |
| 4–5 | `seq` (uint16) |
| 6 | `flags` (bit0 = home request, bit1 = reset fault) |

### 4.5 SERVO_TARGET — `0x110 + 4` (Elbow only)
| Byte | Field |
|---|---|
| 0 | `servo_index` (0–2) |
| 1–4 | `target_angle` (float32, degrees) |
| 5–6 | `seq` (uint16) |

### 4.6 ENCODER_TELEMETRY — `0x200 + node_id`, node → Base, control-loop rate
| Byte | Field |
|---|---|
| 0–3 | `position` (int32, raw SPI encoder counts) |
| 4–5 | `velocity_estimate` (int16, counts/tick) |
| 6–7 | `seq` (uint16) |

### 4.7 FAULT_REPORT — `0x300 + node_id`, node → Base, event-driven
| Byte | Field |
|---|---|
| 0 | `fault_code` |
| 1–4 | `timestamp_ms` (uint32, node-local millis) |

## 5. Safety Model — Heartbeat and E-Stop Over CAN

This is the actual novel risk in v1: safety used to be one MCU watching one USB link; now it's four independently-clocked MCUs sharing one bus, any of which can fail, be unplugged, or lose bus sync without taking the others with it.

**Two independent timeout domains, both required:**

1. **Bus heartbeat timeout (existing model, extended)**: every node runs a local watchdog against `BUS_HEARTBEAT` (`0x010`). If Base's heartbeat is not seen within 500 ms, *that node* disables its own motors and stops driving CAN, regardless of what other nodes are doing. This preserves the legacy guarantee — no single point of laptop/ROS2 disconnection can leave motors live — but now it's evaluated per-node instead of per-MCU-owns-everything, so a bus fault only visible to one node still safes that node.

2. **Node liveness timeout (new)**: Base tracks `NODE_STATUS` (`0x020+id`) from each of the other 3 nodes. If any node's status goes silent for >500 ms, Base immediately transmits `E-STOP` (reason=1). This covers the case the old architecture didn't have to consider: a single joint node crashing, losing power, or falling off the bus while the other three (and the bus heartbeat) are still fine — without this, a dead Shoulder node would just silently stop responding to `MOTOR_TARGET` with no bus-wide reaction.

**E-stop propagation latency budget:**

At 1 Mbps, a worst-case CAN 2.0A frame (with bit-stuffing) is ~130 µs on the wire. Because `0x000` is the lowest possible ID, it wins arbitration against any in-flight frame from any node — it either transmits immediately or, at worst, waits out one currently-transmitting frame (≤130 µs) before winning the next arbitration round. Target end-to-end budget, mirroring the legacy <2.5 ms figure:

| Stage | Budget |
|---|---|
| Frame transmission + arbitration wait (worst case) | ≤130 µs |
| Receiving node's ISR → motor disable | ≤1 control-loop tick (v1 target: 400 Hz → 2.5 ms, TBD once v1 control loop rate is fixed) |
| **Total** | **≤2.63 ms**, keeping parity with the legacy <2.5 ms guarantee within rounding |

**Boot state**: identical to legacy — motors disabled by default on every node at power-up; a node will not enable motors until it has seen at least one valid `BUS_HEARTBEAT` and has no outstanding fault bits requiring `RESET_FAULT`-equivalent (`MOTOR_TARGET flags bit1`).

**Termination note**: 120 Ω termination only at Base and Elbow (physical bus ends) per the v1 hardware doc — Shoulder L/R must NOT populate termination resistors, or bus signal integrity degrades and frame errors (including safety-critical E-STOP frames) become more likely under load.

## 6. Open Questions (blocking firmware implementation)

- Does v1 retain StallGuard-equivalent sensorless stall detection now that SPI position encoders exist, or is stall detection fully replaced by encoder-vs-command divergence? `NODE_STATUS.stall_detected` above is marked TBD pending this decision.
- Final v1 control loop frequency (Alpha=500 Hz, Beta=400 Hz — v1's is not yet fixed in the hardware doc) directly sets the second half of the E-stop latency budget in §5.
- CRC/checksum: CAN already provides a 15-bit CRC per frame at the transport layer. This doc does not add an application-layer checksum on top — confirm that's sufficient given the safety-critical nature of `E-STOP` and `MOTOR_TARGET`, or whether a redundant application-level check is wanted given how safety-critical this bus is.
- Bus-off recovery behavior: TWAI peripherals auto-enter bus-off state after excessive errors. This doc does not yet define whether a node in bus-off should default motors-disabled-and-stay-off (safe but requires manual recovery) or attempt auto-recovery — needs a decision before implementation.
