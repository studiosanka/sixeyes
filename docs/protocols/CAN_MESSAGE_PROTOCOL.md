# CAN Message Protocol — v1 Joint Bus

**Status**: Current — authoritative protocol for v1 Universal Joint PCB hardware (`docs/hardware/v1/v1_PCB_Design_Reference.md`).
**Supersedes**: `docs/protocols/legacy/JSON_MESSAGE_PROTOCOL.md` (single-MCU USB-CDC protocol, Alpha/Beta only).
**Transport**: TWAI (CAN 2.0A, 11-bit standard ID), 1 Mbps, linear daisy chain Base → Shoulder L → Shoulder R → Elbow, 120 Ω termination at Base and Elbow only.

This is a design document, not yet implemented in firmware. It defines node addressing, message framing, and the distributed safety/heartbeat model needed before `firmware/v1/joint_node/` CAN and safety modules can be written.

**Design decisions locked (2026-08-22)**: stall detection strategy, control loop frequency, checksum policy, and bus-off recovery behavior are all resolved below — see §4.3, §5, and §6.

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
| 2 | `stall_detected` (0/1 — set by encoder-vs-commanded-position divergence exceeding threshold during normal operation; see §4.3a) |
| 3–6 | `encoder_position` (int32, latest SPI encoder reading, raw counts) |

### 4.3a Stall Detection Strategy

**Decision**: StallGuard (TMC2209 sensorless stall detection, DIAG pin) is used for **homing only** — sensorless zero-finding at boot/home request, same role it played in legacy firmware. It is not used for runtime fault detection.

**Runtime stall/fault detection** is encoder-vs-command divergence: each node compares its SPI encoder position against the last commanded `MOTOR_TARGET` and flags `stall_detected` in `NODE_STATUS` when divergence exceeds a threshold (threshold value TBD during bench tuning — not a protocol-level concern). This is strictly more informative than StallGuard alone, since it also catches slow drift and mechanical slip that never crosses StallGuard's stall threshold, not just hard stalls.

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

**Control loop frequency — decided: 250 Hz** (down from legacy Alpha=500 Hz / Beta=400 Hz precedent). Two independent constraints drove this down rather than carrying the legacy number forward:

- **CAN bus bandwidth**: steady-state per tick, worst case, Base sends 4× `MOTOR_TARGET` and the nodes reply with 4× `ENCODER_TELEMETRY` = 8 frames/tick, each ~130 µs worst-case on the wire → ~1.06 ms bus-busy time per tick. At 500 Hz (2.0 ms tick) that's ~53% bus utilization from motor+telemetry traffic alone; at 400 Hz (2.5 ms tick), ~43%. Both eat into the margin needed for arbitration/retry headroom under burst conditions (fault reports, heartbeat, E-stop all competing for the same wire). At 250 Hz (4.0 ms tick), utilization drops to ~27%, comfortably under the conventional ~30–40% ceiling for safety-critical CAN loading.
- **MCU headroom**: legacy ran ESP32-S3 (dual-core, 240 MHz). v1's ESP32-C6-MINI-1 is single-core RISC-V at 160 MHz, and each node now does more per-core work than before — CAN RX/TX, SPI encoder polling, TMC2209 UART, and (Elbow) servo PWM — with no second core to offload any of it.

250 Hz is a starting point, not a hard ceiling — revisit upward once bench profiling on real ESP32-C6 hardware shows spare CPU cycles and bus headroom.

**E-stop propagation latency budget — decoupled from the control loop:**

Rather than gating motor disable on the next control-loop tick (which would make the safety-latency figure worse every time the control rate is tuned down), `E-STOP` receipt is handled directly in the CAN RX callback/ISR — motor disable happens as part of interrupt handling, not the next scheduled control-loop iteration. This keeps the safety guarantee independent of whatever control rate is chosen.

At 1 Mbps, a worst-case CAN 2.0A frame (with bit-stuffing) is ~130 µs on the wire. Because `0x000` is the lowest possible ID, it wins arbitration against any in-flight frame from any node — it either transmits immediately or, at worst, waits out one currently-transmitting frame (≤130 µs) before winning the next arbitration round.

| Stage | Budget |
|---|---|
| Frame transmission + arbitration wait (worst case) | ≤130 µs |
| Receiving node's CAN RX ISR → motor disable (interrupt-driven, not tied to control loop tick) | ≤~500 µs (ISR + immediate GPIO/PWM disable, TBD exact figure during firmware implementation) |
| **Total** | **≤~630 µs target**, comfortably inside the legacy <2.5 ms guarantee with margin to spare |

**Boot state**: identical to legacy — motors disabled by default on every node at power-up; a node will not enable motors until it has seen at least one valid `BUS_HEARTBEAT` and has no outstanding fault bits requiring `RESET_FAULT`-equivalent (`MOTOR_TARGET flags bit1`).

**Termination note**: 120 Ω termination only at Base and Elbow (physical bus ends) per the v1 hardware doc — Shoulder L/R must NOT populate termination resistors, or bus signal integrity degrades and frame errors (including safety-critical E-STOP frames) become more likely under load.

## 6. Resolved Design Decisions

All four items previously open here are now decided. Recorded with rationale so the reasoning survives past this doc's drafting session.

### 6.1 Stall detection
See §4.3a. StallGuard = homing only. Runtime stall/fault = encoder-vs-command divergence.

### 6.2 Control loop frequency
See §5. Decided: 250 Hz, decoupled E-stop handling (ISR-driven, not tied to control tick). Revisit upward after bench profiling.

### 6.3 Checksum policy — no application-layer checksum

**Decision**: rely on CAN's built-in 15-bit CRC (plus stuff-bit violation detection) for transport-layer integrity. No redundant application-layer checksum is added on top.

**Reasoning**: CAN's CRC-15 already gives very low undetected-error probability for wire-level corruption — this is standard, unsupplemented practice in the vast majority of non-certified CAN systems. What it does not catch — stale/replayed data, or an endpoint applying a logically-wrong-but-CRC-valid frame — is already covered by the `seq` fields on `MOTOR_TARGET`/`SERVO_TARGET`. `E-STOP` deliberately carries no `seq`: accepting a "stale" E-stop is the fail-safe direction, so replay protection there would be actively counterproductive.

**⚠️ Certification note — revisit if this ever changes**: this reasoning holds for a hobbyist/open-source, non-certified project. If SixEyes ever needs to meet a formal functional-safety standard (IEC 61508, ISO 13849, or similar) for this bus, that typically mandates *diverse* redundancy — i.e., an application-layer check that is independent of the transport-layer CRC, not just "more CRC." Revisit this decision explicitly before any certification effort; do not assume the current CAN-CRC-only approach is sufficient for a certified deployment.

### 6.4 Bus-off recovery — manual only, no auto-recovery

**Decision**: TWAI bus-off (after 256 consecutive TX errors) is treated as a hard fault. A node in bus-off disables its own motors immediately and locally (it can no longer send/receive `BUS_HEARTBEAT`/`E-STOP` regardless), and does **not** attempt automatic bus-off recovery. Recovery requires a manual reset/power-cycle of that node.

**Reasoning**: auto-recovery risks a node flapping in and out of a still-faulty bus, repeatedly re-entering an error state. Manual-only recovery is consistent with the existing `RESET_FAULT`-required-after-fault philosophy carried over from legacy firmware. It's also redundant-safe by design: independent of what the bus-off'd node does, Base's `NODE_STATUS` liveness timeout (§5, item 2) will notice that node has gone silent and fire a bus-wide `E-STOP` anyway, so the whole arm goes safe regardless of the bus-off'd node's own behavior.
