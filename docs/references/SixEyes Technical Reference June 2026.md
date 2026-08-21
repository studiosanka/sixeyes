# SixEyes – Technical Reference — June 2026

**Beta Rev2 / NodeMesh Edition**

> **Status: LEGACY.** Beta is superseded by the v1 Universal Joint PCB architecture — see `docs/hardware/v1/v1_PCB_Design_Reference.md` for the current authoritative hardware spec. This document is kept for Alpha/Beta reference only.

> This document reflects Beta Rev2 hardware.
> For Alpha hardware details see `docs/hardware/legacy/alpha/`.
> For PCB design detail see `docs/references/SixEyes_Node0.pdf`.
> For firmware config see `firmware/legacy/beta/follower_esp32/src/modules/config/`.

---

## Table of Contents

1. [Project Purpose & Scope](#1-project-purpose--scope)
2. [High-Level System Architecture](#2-high-level-system-architecture)
3. [Hardware Architecture](#3-hardware-architecture)
4. [Power Architecture](#4-power-architecture)
5. [Forbidden GPIO Lines](#5-forbidden-gpio-lines-n16r8-octal-psramflash)
6. [Full GPIO Quick Reference](#6-full-gpio-quick-reference-beta-rev2)
7. [Connector Reference](#7-connector-reference)
8. [Embedded Firmware Architecture](#8-embedded-firmware-architecture)
9. [Communication Protocols](#9-communication-protocols)
10. [ROS2 Integration](#10-ros2-integration-laptop)
11. [Safety Architecture](#11-safety-architecture)
12. [Mechanical Design](#12-mechanical-design)
13. [Mechanical Design Constraints](#13-mechanical-design-constraints)
14. [Cost and Sustainability Goals](#14-cost-and-sustainability-goals)
15. [System Summary](#15-system-summary)
16. [Summary Statement](#16-summary-statement)

---

## 1. Project Purpose & Scope

SixEyes is a low-cost, open-source, research robotic arm platform designed for Vision–Language–Action (VLA) experimentation.

The system focuses on:

- Collecting high-quality multimodal datasets
- Enabling real-time VLA policy execution
- Using accessible, reusable hardware
- Maintaining deterministic low-level control

The system architecture intentionally separates **embedded control (real-time)** from **AI inference (high compute)**. Embedded hardware focuses strictly on sensing, motor control, and safety. All perception, planning, and machine learning runs on an external laptop.

Two hardware generations exist:

| Generation | Description |
|---|---|
| **Alpha** | Through-hole DevKit carrier, single-layer protoboard, external TMC2209 modules |
| **Beta Rev2** *(this document)* | 60×60 mm 4-layer custom PCB, integrated MCU/drivers/bucks, ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB Octal PSRAM) |

---

## 2. High-Level System Architecture

### 2.1 Distributed Control Philosophy

| Layer | Device | Responsibilities |
|---|---|---|
| Low-Level Control | Node0 — ESP32-S3-WROOM-1-N16R8 (Beta PCB) | 400 Hz stepper timing (4 axes), servo PWM (3 channels), StallGuard2 homing, safety supervision |
| Communication | USB-CDC serial link (Native OTG via J_USB1) | Deterministic command transfer, telemetry streaming to ROS2 |
| High-Level Intelligence | Laptop | Vision processing, VLA policy inference, dataset logging, motion planning |
| Orchestration | ROS2 (Laptop) | Node communication, dataset synchronization, experiment tooling |

### 2.2 NodeMesh Topology

In NodeMesh mode the system spans four MCUs:

| Node | Hardware | Role |
|---|---|---|
| **Node0** | ESP32-S3-WROOM-1-N16R8 (Beta PCB) | Real-time orchestrator. Runs 400 Hz control loop, aggregates joint states and vision features, logs experience packets to MicroSD. |
| **Node1** | ESP32-C3-MINI-1 (integrated beta leader PCB, wired to Node0) | Leader/teleoperation input. Reads 6× potentiometers at 100 Hz, sends absolute joint targets at 921,600 baud. |
| **Node2** | ESP32-CAM (global perspective, wireless) | Global camera node. Sends 128-byte brightness histogram feature vectors via ESP-NOW channel 6. |
| **Node3** | ESP32-CAM (wrist/eye-in-hand, wireless) | Wrist camera node. Same protocol as Node2. |

Node0 discards camera frames older than 300 ms before merging into the IK/experience packet.

---

## 3. Hardware Architecture

### 3.1 Leader Arm (Teleoperation Controller)

The leader arm captures human motion input using potentiometers representing target joint positions.

**MCU:** ESP32-C3-MINI-1 (Node1, integrated beta leader PCB — castellation-mounted, no devkit)

**ADC Domain:** ADC1 only (GPIO0–5). ADC2 unused.

**Leader Pinout (ESP32-C3 Node1):**

| Input | GPIO | ADC Channel |
|---|---|---|
| Pot 1 | `GPIO0` | ADC1_CH0 |
| Pot 2 | `GPIO1` | ADC1_CH1 |
| Pot 3 | `GPIO2` | ADC1_CH2 |
| Pot 4 | `GPIO3` | ADC1_CH3 |
| Pot 5 | `GPIO4` | ADC1_CH4 |
| Pot 6 | `GPIO5` | ADC1_CH5 |

**UART Link to Node0 (UART1, GPIO matrix):**

| Signal | Node1 GPIO | Path |
|---|---|---|
| Node1 TX | `GPIO7` | J_UART1 Pin 2 → Node0 `GPIO42` |
| Node1 RX | `GPIO6` | J_UART1 Pin 3 ← Node0 `GPIO41` |

Connector: JST XH 4-pin, pin order [GND | TX | RX | 3V3]. Matches follower J_LDR pinout.

Baud: 921,600, 8N1. Magic-number scan + CRC-CCITT (poly `0x1021`) framing. Packets carry absolute joint targets for all 6 joints.

**Power:** Node1 beta leader PCB is self-powered via onboard USB-C + AMS1117-3.3 LDO. Pin 4 of J_UART1 carries +3V3_LOGIC optionally for power-sharing if USB-C is not used. J_LDR_PWR on follower PCB may be left unused when leader USB-C is populated.

> **Firmware note:** Beta leader GPIO map differs from alpha. GPIO0–5 for ADC, GPIO6/7 for UART1 (not GPIO0,1,2,3,4,6 and GPIO17/18 as on alpha ESP32-C6 SuperMini). Leader firmware environment must be updated before flashing to beta leader PCB.

---

### 3.2 Follower Arm (Actuated Robot Arm)

**Degrees of Freedom:**

| Joint | Actuator | Channel |
|---|---|---|
| Base | 1× NEMA17 | J1 |
| Shoulder A | 1× NEMA17 (torque split) | J2 |
| Shoulder B | 1× NEMA17 (torque split) | J3 |
| Elbow | 1× NEMA17 | J4 |
| Wrist Roll | 1× MG996R servo | J_SV2 |
| Wrist Pitch | 1× MG996R servo | J_SV1 |
| Gripper | 1× MG996R servo | J_SV3 |

4 stepper channels · 3 servo channels · 6 DOF

**IK Parameters (Node0 firmware):**

| Parameter | Value |
|---|---|
| L1 — upper arm | 225 mm |
| L2 — forearm | 200 mm |
| Model | 2-link planar IK |

See [§12.2](#122-link-length-ratios-agilex-piper-reference) for derivation.

---

### 3.3 Stepper Motor Drivers (Beta Rev2)

**Drivers:** 4× TMC2209-LA (QFN-28, direct IC on Beta PCB, 2×2 grid)

**Configuration:**

- PDN_UART mode enabled (single-wire UART per driver)
- StallGuard2 enabled (DIAG pins routed)
- CoolStep adaptive current scaling available
- SpreadCycle / StealthChop2 configurable per driver
- VIO = 3.3 V · VM = 24 V

**Global enable:** `GPIO4` → `DRV_EN_ALL` (active LOW, shared to all 4 drivers)

**PDN_UART wiring (per driver):** 1 kΩ series + 4.7 kΩ pull-up to 3.3 V

**DIAG wiring (per driver):** 47 kΩ pull-up to 3.3 V — open-drain, pull-up is mandatory

**Beta Follower GPIO — TMC2209 Pin Assignment:**

| Joint | STEP | DIR | PDN_UART | DIAG |
|---|---|---|---|---|
| J1 – Base | `GPIO5` | `GPIO6` | `GPIO7` | `GPIO1` |
| J2 – Shoulder A | `GPIO8` | `GPIO9` | `GPIO14` | `GPIO2` |
| J3 – Shoulder B | `GPIO15` | `GPIO16` | `GPIO17` | `GPIO3` |
| J4 – Elbow | `GPIO18` | `GPIO21` | `GPIO47` | `GPIO48` |

PDN_UART: 1 kΩ series + 4.7 kΩ pull-up. DIAG: 47 kΩ pull-up (StallGuard2 / fault).

---

### 3.4 Servo Outputs

Servos operate on the dedicated 6.6 V rail.

| Servo | GPIO | Connector |
|---|---|---|
| Wrist Roll | `GPIO39` | J_SV2 |
| Wrist Pitch | `GPIO38` | J_SV1 |
| Gripper | `GPIO40` | J_SV3 |

PWM generation via ESP32-S3 LEDC / MCPWM peripheral.

Connector type: 2.54 mm 3-pin header per channel (Signal / 6.6 V / GND)

**Servo current budget:** 5.0 A continuous · 7.0 A stall peak (3 servos total)

---

### 3.5 Leader–Follower Communication

| Parameter | Value |
|---|---|
| Node0 RX (← Node1 TX) | `GPIO42` |
| Node0 TX (→ Node1 RX) | `GPIO41` |
| Baud rate | 921,600, 8N1 |
| Framing | Magic-number scan + CRC-CCITT (poly `0x1021`) |
| Receive mode | Non-blocking async on Node0 |
| Connector | J_LDR — 2.54 mm 4-pin, South Edge (TX, RX, GND, spare) |

Benefits: deterministic latency · works without laptop · enables teleoperation fallback mode.

---

### 3.6 Camera Architecture (NodeMesh)

Camera nodes are **not** wired to Beta PCB data lines. Power only is supplied via `J_CAM1` and `J_CAM2`.

```
Node2 (Global Cam)  ──ESP-NOW ch.6──▶  Node0
Node3 (Wrist Cam)   ──ESP-NOW ch.6──▶  Node0
```

- **Data payload:** 128-byte brightness histogram feature vector in `ExperiencePacket` (magic `0x4E4D5650`)
- **Freshness gate:** frames older than 300 ms discarded before IK merge

| Connector | Type | Signal | Edge |
|---|---|---|---|
| J_CAM1 | JST XH 2.5mm 2-pin | Node2 5 V / GND | South |
| J_CAM2 | JST XH 2.5mm 2-pin | Node3 5 V / GND | South |

10 µF ceramic bypass cap at each camera header (absorbs ESP-NOW TX burst ~200 mA).

In standalone SixEyes mode (without NodeMesh), cameras may connect directly to the laptop via USB.

---

### 3.7 MicroSD Logging

Hardware SPI · FAT32 · 32 kB clusters · 1 MB pre-allocated circular buffer · ~300 B per experience packet · flush every 8 writes.

| Signal | GPIO | Pull-up |
|---|---|---|
| SD_CS | `GPIO10` | 10 kΩ |
| SD_MOSI | `GPIO11` | 10 kΩ |
| SD_SCK | `GPIO12` | — |
| SD_MISO | `GPIO13` | 10 kΩ |

Connector: Hirose DM3D-SF MicroSD push-pull (interior zone).

---

### 3.8 Dual USB-C Interface

| Port | Connector | GPIOs | Purpose |
|---|---|---|---|
| **Port 1** | J_USB1 (West Edge) | `GPIO19` (D−) / `GPIO20` (D+) | Native USB-OTG — ROS2 USB-CDC bridge, telemetry, VLA dataset logging. 90 Ω diff pair, L1 only. |
| **Port 2** | J_USB2 (West Edge) | `GPIO43` (TXD) / `GPIO44` (RXD) | CH340K UART bridge — firmware flash, DTR/RTS auto-reset for PlatformIO. |

---

## 4. Power Architecture

### 4.1 Main Power Input

**24 V DC** via XT30 right-angle connector (`J_PWR`, West Edge). Recommended supply ≥ 10 A.

Directly powers:
- 4× TMC2209 VM pins (direct, fused)
- TPS54540B 6.6 V buck input
- LMR14030S 5.0 V buck input

---

### 4.2 6.6 V Rail (Servo Power)

| Parameter | Value |
|---|---|
| Regulator | TPS54540B — 5.5 A synchronous buck, HSOP-8-EP |
| Output voltage | 6.6 V (R_top = 102 kΩ, R_bot = 14 kΩ, V_ref = 0.8 V) |
| Loads | 3× MG996R / MG995 servos |
| Continuous / peak | 5.0 A / 7.0 A |
| Inductor | Shielded, L = 4.7 µH, I_sat > 9.0 A, ≥ 10×10 mm (e.g. Bourns SRR1260) |
| Output cap | ≥ 1000 µF low-ESR SMD electrolytic + 10 µF ceramic at servo headers |

---

### 4.3 5.0 V Rail (Logic & Peripheral Domain)

**Regulator:** LMR14030S — 3 A asynchronous buck, HSOP-8-EP

**Load budget (worst-case simultaneous):**

| Load | Typical | Peak |
|---|---|---|
| Node2 ESP32-CAM (global cam, ESP-NOW TX) | 200 mA | 400 mA |
| Node3 ESP32-CAM (wrist cam, ESP-NOW TX) | 200 mA | 400 mA |
| Node1 beta leader PCB (self-powered via USB-C; optional 3V3 share via J_UART1 pin 4) | 0 mA typical | 0 mA |
| MIC5219-3.3 LDO input (ESP32-S3 + TMC VIO) | 400 mA | 620 mA |
| **Total 5.0 V rail** | **900 mA** | **1670 mA** |

LMR14030S 3 A rating provides >1.3× headroom over worst-case peak.
Inductor: 4.7 µH, I_sat > 3.5 A. Output: ≥ 100 µF SMD electrolytic + ceramic bank.

---

### 4.4 3.3 V Rail (Logic Domain)

**Regulator:** MIC5219-3.3YM5 — 1 A LDO, SOT-23-5, powered from 5.0 V rail.

| Load | Peak |
|---|---|
| ESP32-S3-WROOM-1 core + RF | 500 mA |
| 4× TMC2209 VIO (15 mA each) | 60 mA |
| Pull-up resistor networks | 20 mA |
| **Total** | **580 mA** |

MIC5219-3.3 1 A rating provides ~420 mA headroom.

> **Thermal note:** worst-case P_D = (5.0 − 3.3) V × 0.58 A ≈ 1.0 W. Ensure adequate copper spreading under SOT-23-5 pad.

Input cap: ≥ 10 µF ceramic (0805). Output cap: 22 µF ceramic (0805) + 100 nF X7R (0603).

---

### 4.5 Power Rail Summary

```
24 V DC (J_PWR)
 ├─▶ 4× TMC2209 VM (direct, fused)
 ├─▶ TPS54540B 6.6 V buck
 │     └─▶ J_SV1 / J_SV2 / J_SV3 (servo channels)
 └─▶ LMR14030S 5.0 V buck
       ├─▶ J_LDR_PWR (Node1 leader board)
       ├─▶ J_CAM1 (Node2 camera)
       ├─▶ J_CAM2 (Node3 camera)
       └─▶ MIC5219-3.3 LDO
             └─▶ ESP32-S3 core logic + 4× TMC2209 VIO
```

---

### 4.6 24 V Motor Rail Filtering (per TMC2209)

All caps placed within 2 mm of IC package edge.

| Component | Value | Notes |
|---|---|---|
| Bulk cap | 100 µF / 35 V low-ESR SMD electrolytic | Panasonic FR or Nichicon UWX, case D. **No tantalum** — motor back-EMF causes catastrophic failure. |
| HF bypass | 100 nF X7R 50 V (0603) | Trace to VM pin ≤ 1 mm. |

---

### 4.7 Grounding Strategy

4-layer stackup:

| Layer | Type | Function |
|---|---|---|
| L1 | Signal / Component | Component pads, USB diff pairs, buck switching polygons |
| L2 | Solid GND plane | Primary return path, RF shield, TMC2209 thermal via landing |
| L3 | Split power plane | Isolated copper polygons: +24V_MOTOR, +6.6V_SERVO, +5.0V, +3.3V |
| L4 | Signal / Thermal | Trapped signal escape, exposed copper under TMC2209 via farms |

Rules:
- Servo return currents must not share trace segments with MCU signal GND — both terminate at L2 via separate vias.
- Buck switching node polygons on L1 must not be crossed by signal traces.
- GND stitching vias L1→L2 every ~5 mm at board perimeter.
- GND stitching vias along full length of USB differential pair routes.
- GND stitching vias adjacent to all buck switching polygons.

---

## 5. Forbidden GPIO Lines (N16R8 Octal PSRAM/Flash)

> **⚠ These GPIOs are consumed by internal silicon. They must never be used in firmware or routed on the PCB.**

| GPIO | Internal Function |
|---|---|
| `GPIO26` | Internal octal flash clock |
| `GPIO33` | Flash D4 |
| `GPIO34` | Flash D5 |
| `GPIO35` | Octal PSRAM D6 / WP |
| `GPIO36` | Octal PSRAM D7 / HOLD |
| `GPIO37` | Octal PSRAM CLK |

---

## 6. Full GPIO Quick Reference (Beta Rev2)

| GPIO | Function | Notes |
|---|---|---|
| `0` | MCU_BOOT | 10 kΩ pull-up; tactile switch to GND |
| `1` | J1 Base DIAG | StallGuard2 / fault, 47 kΩ pull-up |
| `2` | J2 ShldA DIAG | StallGuard2 / fault, 47 kΩ pull-up |
| `3` | J3 ShldB DIAG | StallGuard2 / fault, 47 kΩ pull-up |
| `4` | DRV_EN_ALL | Active LOW, all 4 TMC2209 |
| `5` | J1 Base STEP | |
| `6` | J1 Base DIR | |
| `7` | J1 Base PDN_UART | 1 kΩ series + 4.7 kΩ pull-up |
| `8` | J2 ShldA STEP | |
| `9` | J2 ShldA DIR | |
| `10` | SD_CS | 10 kΩ pull-up |
| `11` | SD_MOSI | 10 kΩ pull-up |
| `12` | SD_SCK | |
| `13` | SD_MISO | 10 kΩ pull-up |
| `14` | J2 ShldA PDN_UART | 1 kΩ series + 4.7 kΩ pull-up |
| `15` | J3 ShldB STEP | |
| `16` | J3 ShldB DIR | |
| `17` | J3 ShldB PDN_UART | 1 kΩ series + 4.7 kΩ pull-up |
| `18` | J4 Elbow STEP | |
| `19` | USB_D− | 90 Ω diff pair → J_USB1 |
| `20` | USB_D+ | 90 Ω diff pair → J_USB1 |
| `21` | J4 Elbow DIR | |
| `26` | **FORBIDDEN** | Internal octal flash clock |
| `33` | **FORBIDDEN** | Flash D4 |
| `34` | **FORBIDDEN** | Flash D5 |
| `35` | **FORBIDDEN** | Octal PSRAM D6/WP |
| `36` | **FORBIDDEN** | Octal PSRAM D7/HOLD |
| `37` | **FORBIDDEN** | Octal PSRAM CLK |
| `38` | Servo Wrist Pitch PWM | → J_SV1 |
| `39` | Servo Wrist Roll PWM | → J_SV2 |
| `40` | Servo Gripper PWM | → J_SV3 |
| `41` | Leader UART TX | 921,600 baud → Node1 |
| `42` | Leader UART RX | 921,600 baud ← Node1 |
| `43` | UART0_TXD | → CH340K RXD, J_USB2 |
| `44` | UART0_RXD | ← CH340K TXD, J_USB2 |
| `46` | MCU_STRAP_LOW | 10 kΩ pull-down to GND (required by strapping spec) |
| `47` | J4 Elbow PDN_UART | 1 kΩ series + 4.7 kΩ pull-up |
| `48` | J4 Elbow DIAG | StallGuard2 / fault, 47 kΩ pull-up |

---

## 7. Connector Reference

| Ref | Type | Signal / Net | Edge | Rail |
|---|---|---|---|---|
| J_PWR | XT30 RA 2-pin | 24 V DC in / GND | West | 24 V |
| J_USB1 | USB-C receptacle | Native USB-OTG (D− / D+) | West | — |
| J_USB2 | USB-C receptacle | CH340K UART debug / flash | West | — |
| J_M1 | JST XH 2.5mm 4-pin | J1 Base stepper phases | South / East | 24 V |
| J_M2 | JST XH 2.5mm 4-pin | J2 Shoulder A phases | South / East | 24 V |
| J_M3 | JST XH 2.5mm 4-pin | J3 Shoulder B phases | South / East | 24 V |
| J_M4 | JST XH 2.5mm 4-pin | J4 Elbow phases | South / East | 24 V |
| J_SV1 | 2.54 mm 3-pin | Wrist Pitch PWM / 6.6 V / GND | South | 6.6 V |
| J_SV2 | 2.54 mm 3-pin | Wrist Roll PWM / 6.6 V / GND | South | 6.6 V |
| J_SV3 | 2.54 mm 3-pin | Gripper PWM / 6.6 V / GND | South | 6.6 V |
| J_CAM1 | JST XH 2.5mm 2-pin | Node2 5 V / GND | South | 5 V |
| J_CAM2 | JST XH 2.5mm 2-pin | Node3 5 V / GND | South | 5 V |
| J_LDR | 2.54 mm 4-pin | Node1 UART TX, RX, GND, spare | South | — |
| J_LDR_PWR | JST XH 2.5mm 2-pin | Node1 5 V / GND power | South | 5 V |
| J_SD | MicroSD push-pull | SPI: CS / MOSI / SCK / MISO | Interior | 3.3 V |

---

## 8. Embedded Firmware Architecture

**Control loop rates:**

| Loop | Rate |
|---|---|
| Node0 motor control | 400 Hz |
| Node1 leader ADC sampling | 100 Hz |
| VLA policy (laptop) | 5–10 Hz |

**Firmware environments (PlatformIO):**

| Environment | Path | Target |
|---|---|---|
| `beta_follower` | `firmware/legacy/beta/follower_esp32/` | Node0, Beta PCB |
| `alpha_leader` | `firmware/legacy/alpha/leader_esp32/` | Node1, shared with Alpha |
| `alpha_follower` | `firmware/legacy/alpha/follower_esp32/` | Alpha only |

```bash
cd firmware/legacy/beta/follower_esp32 && pio run -e beta_follower
```

**Operating modes** (compile-time `-DOPERATION_MODE`):

| Mode | Value | Behaviour |
|---|---|---|
| VLA Inference | `1` | ROS2 AI planner sends `MOTOR_TARGET` commands; ESP32 executes trajectory profiling |
| Teleoperation | `2` | Node1 streams joint angles → Node0 mirrors in real time; laptop optional |

**Data streams:**

| Link | Content |
|---|---|
| Node0 → Laptop (USB-CDC, J_USB1) | Joint positions, timestamp, motor state, fault bitmask, telemetry |
| Node1 → Node0 (UART 921,600 baud) | Absolute joint targets (all 6 joints) |
| Node2 / Node3 → Node0 (ESP-NOW ch. 6) | 128-byte histogram feature vectors |

**Key firmware config files:**

- `firmware/legacy/beta/follower_esp32/src/modules/config/board_config.h` — Beta GPIO pin assignments
- `firmware/legacy/beta/follower_esp32/src/modules/drivers/tmc2209/tmc2209_config.h` — Beta motor driver pins, DIAG, PDN_UART

---

## 9. Communication Protocols

### 9.1 Safety Heartbeat (ASCII, hardware-agnostic)

| Direction | Format | Rate |
|---|---|---|
| ROS2 → ESP32 | `HB:<source_id>,<seq>\n` | ≥ 50 Hz |
| ESP32 → ROS2 | `SB:<fault_bitmask>,<motors_en>,<ros2_alive>\n` | 10 Hz |

Timeout: >500 ms → motors disabled, latched until `RESET_FAULT`.

---

### 9.2 JSON Command Protocol (VLA Inference Mode)

Line-delimited, `\n` framed, max 512 bytes per message.

| Command | Direction | Purpose |
|---|---|---|
| `MOTOR_TARGET` | ROS2 → ESP32 | Stepper position targets |
| `SERVO_TARGET` | ROS2 → ESP32 | Servo angle targets |
| `ENABLE_MOTORS` | ROS2 → ESP32 | Arm motors |
| `RESET_FAULT` | ROS2 → ESP32 | Clear latched fault |
| `HOME_ZERO` | ROS2 → ESP32 | Zero all axes |
| `HOME_STALLGUARD` | ROS2 → ESP32 | Sensorless homing via StallGuard2 |
| `TUNE_PID` | ROS2 → ESP32 | Adjust control loop gains |

---

### 9.3 Teleoperation UART Protocol

| Message | Direction | Content |
|---|---|---|
| `JOINT_STATE` | Node1 → Node0 | Leader joint angles; follower mirrors |
| `TELEMETRY_STATE` | Node0 → host | Current joint positions + status |

Packet framing: magic-number scan + CRC-CCITT (poly `0x1021`).

---

## 10. ROS2 Integration (Laptop)

**Primary nodes:**

| Node | Role |
|---|---|
| `usb_bridge_node` | Owns serial port; HB TX at 50 Hz; telemetry RX; command TX |
| `safety_node` | Parses `SB:` → publishes `/sixeyes/is_safe` (Bool) |
| `joint_state_node` | Converts `/sixeyes/joint_states` (deg) → `/joint_states` (rad) |
| `camera_node` | USB cam → `/camera/image_raw` |
| `vla_inference_node` | Stub (not yet implemented) |

**Data alignment strategy (dataset recording):**

1. Image received.
2. Find nearest `joint_state` timestamp.
3. Next `joint_state` = action label.

**Minimal rosbag dataset schema:**

```
sensor_msgs/Image
sensor_msgs/JointState
custom ActionMsg
timestamp
```

---

## 11. Safety Architecture

**Principles:**

- Motors disabled by default at boot.
- Motion only enabled after heartbeat is established.
- `RESET_FAULT` required to re-enable after any fault.
- Heartbeat must be active before `ENABLE_MOTORS` is accepted.

**Heartbeat timeout:** 500 ms

**Safety control:** `DRV_EN_ALL` (`GPIO4`) controlled by safety supervisor task. Active LOW — pulled LOW by ESP32 to disable all drivers simultaneously.

If laptop communication stops:
- All drivers disabled via `DRV_EN_ALL`.
- Motion halts immediately.
- State latched until `RESET_FAULT` received.

**DIAG pin monitoring:** `GPIO1` / `GPIO2` / `GPIO3` / `GPIO48` are TMC2209 StallGuard2 / fault flags. Open-drain with 47 kΩ pull-ups on PCB. Firmware reads these to detect stalls and log fault events.

**Beta PCB safety rules (never violate):**

- Forbidden GPIOs (26/33/34/35/36/37) must never appear in firmware.
- DIAG pull-ups (47 kΩ) are mandatory — open-drain without pull-up reads indeterminate logic level.
- No tantalum capacitors on the 24 V motor rail.

---

## 12. Mechanical Design

### 12.1 Performance Targets

| Parameter | Value |
|---|---|
| Payload | 500 g at full reach |
| Reach | 500 mm (base rotation axis → TCP, arm horizontal) |
| Gear ratio | 25:1 cycloidal (all stepper joints) |
| Structure | Fully 3D-printed, no ball bearings |

---

### 12.2 Link Length Ratios (AgileX Piper Reference)

**Design reference — AgileX Piper 6-DOF:**

| Parameter | Value |
|---|---|
| Upper arm (a3) | 285 mm |
| Forearm (d4) | 251 mm |
| Wrist-to-TCP (d6) | 91 mm |
| Total reach | 627 mm |
| Payload | 1500 g |

**Scale factor for SixEyes:** 500 mm / 627 mm = **0.798**

| Segment | Piper | SixEyes | Fraction of reach |
|---|---|---|---|
| Upper arm | 285 mm | 225 mm | 45.0% |
| Forearm | 251 mm | 200 mm | 40.0% |
| Wrist assembly | 91 mm | 75 mm | 15.0% |
| **Total** | **627 mm** | **500 mm** | **100%** |

**Link ratios:**
- Upper arm : Forearm : Wrist = 225 : 200 : 75 ≈ **3.00 : 2.67 : 1.00**
- Upper arm : Forearm = **1.125 : 1**

The slightly longer upper arm follows Piper's proportions and improves dexterity in the mid-workspace without increasing elbow torque demand at full extension.

**IK constants (Node0 firmware):** L1 = 225 mm · L2 = 200 mm

---

### 12.3 Wrist Assembly Breakdown (75 mm total)

Kinematic chain from elbow outward:

| Segment | Length |
|---|---|
| Elbow flange → Wrist Roll pivot | 23 mm |
| Wrist Roll pivot → Wrist Pitch pivot | 27 mm |
| Wrist Pitch pivot → TCP | 25 mm |
| **Total wrist assembly** | **75 mm** |

**Joint order (base → TCP):**

| Joint | Type | Actuator |
|---|---|---|
| J1 — Base | Yaw | Stepper + cycloidal 25:1 |
| J2 — Shoulder | Pitch | Stepper + cycloidal 25:1 (torque split with J3) |
| J3 — Shoulder B | Pitch | Stepper + cycloidal 25:1 (torque split partner) |
| J4 — Elbow | Pitch | Stepper + cycloidal 25:1 |
| J5 — Wrist Roll | Roll | MG996R servo — **further from gripper** |
| J6 — Wrist Pitch | Pitch | MG996R servo — **closer to gripper** |
| J7 — Gripper | — | MG996R servo |

---

### 12.4 Wrist Joint Ordering Rationale

Wrist Pitch (J6) is placed **closer to the gripper** than Wrist Roll (J5) to maximise lifting capacity within MG996R torque limits.

**MG996R specifications:**

| Parameter | Value |
|---|---|
| Stall torque | 13 kg·cm = 1.27 N·m (at 6.6 V) |
| Operating torque | ~0.90 N·m (continuous safe) |

**With Pitch closer to gripper (this design):**

| Parameter | Value |
|---|---|
| Load on pitch | gripper assembly (~100 g) + payload (500 g) = **600 g** |
| Moment arm | 25 mm (pitch pivot → TCP) |
| Required τ | 0.6 × 9.81 × 0.025 = **0.147 N·m** |
| Stall margin | 1.27 / 0.147 = **8.6×** |
| Operating margin | 0.90 / 0.147 = **6.1×** |

**If ordering were reversed (Pitch further from gripper, ~50 mm from TCP):**

| Parameter | Value |
|---|---|
| Load on pitch | roll assembly (~80 g) + gripper (100 g) + payload (500 g) = **680 g** |
| Moment arm | 50 mm |
| Required τ | 0.68 × 9.81 × 0.050 = **0.334 N·m** |
| Stall margin | 1.27 / 0.334 = **3.8×** |
| Operating margin | 0.90 / 0.334 = **2.7×** ⚠ dangerously low at high duty cycle |

The current ordering (Roll → Pitch → Gripper) reduces the pitch torque demand by **2.3×**, providing safe margin for the MG996R under 500 g payload at maximum reach.

Wrist Roll (J5) rotates about the forearm's long axis; when the arm is extended horizontally for lifting tasks, gravitational loading on the roll joint is pose-dependent and substantially lower than on the pitch joint.

---

### 12.5 Per-Joint Torque Budget (500 g at 500 mm, arm horizontal)

Estimated 3D-printed link masses: upper arm ~200 g · forearm ~150 g · wrist assembly ~80 g · gripper ~100 g.

| Joint | Type | Worst-case τ | Available τ | Margin |
|---|---|---|---|---|
| J1 Base | Cycloidal | 2.7 N·m ¹ | ~10.0 N·m | 3.7× |
| J2+J3 Shoulder | Dual cycloidal | 3.2 N·m | ~20.0 N·m | 6.3× |
| J4 Elbow | Cycloidal | 0.7 N·m | ~10.0 N·m | 14.3× |
| J5 Wrist Roll | MG996R | ~0.22 N·m | 1.27 N·m | 5.8× |
| J6 Wrist Pitch | MG996R | 0.147 N·m | 1.27 N·m | 8.6× |

¹ J1 base torque is inertial/dynamic (arm rotating); gravitational load on base is zero when arm is symmetric. Figure shown is worst-case deceleration at 400 Hz control rate.

> Available cycloidal torque estimates assume NEMA17 holding torque ~0.4 N·m × 25:1. Actual value depends on TMC2209 RMS current setting and temperature derating.

---

### 12.6 Shoulder Joint

Two NEMA17 motors (J2 and J3) drive the shoulder with a shared cycloidal gearbox.

**Reasons for dual-motor shoulder:**
- Shoulder bears the highest sustained gravitational torque in the kinematic chain (full arm + payload at max reach).
- Torque split across two cycloidal drives halves shaft stress on each motor and improves reliability.

Gear reduction: 25:1 cycloidal per motor → effectively ~20 N·m at the shoulder joint.

---

## 13. Mechanical Design Constraints

- Cycloidal gearboxes are fully 3D-printed.
- Software compensates for backlash.
- StallGuard2 provides sensorless homing — no limit switches required.
- Wiring routed internally where possible.
- Node3 (wrist camera) mounting must avoid occlusion of the gripper workspace.
- Beta PCB antenna (North Edge): ≥ 2 mm overhang past board edge; 15×10 mm copper keep-out zone under antenna across all 4 layers.

---

## 14. Cost and Sustainability Goals

Design philosophy: **reuse hardware wherever possible.**

| Component class | Typical source |
|---|---|
| NEMA17 motors | Salvaged from old 3D printers |
| MG996R / MG995 servos | Generic RC hobby supply |
| ESP32-CAM modules | Generic |
| Structural parts | Entirely 3D-printed |

**Target BOM:** ~$250 USD *(excluding prints and laptop)*

---

## 15. System Summary

**SixEyes Beta Rev2 Platform:**

- 6 DOF robotic arm
- 4× TMC2209-LA stepper drivers (QFN-28, direct on PCB)
- 3× MG996R class servos (Wrist Roll / Wrist Pitch / Gripper)
- Dual shoulder motors (J2 + J3)
- Node0: ESP32-S3-WROOM-1-N16R8 on 60×60 mm 4-layer PCB
- 400 Hz deterministic control loop
- TPS54540B 6.6 V buck (servo power)
- LMR14030S 5.0 V buck (logic / peripheral power)
- MIC5219-3.3 1 A LDO (3.3 V core logic)
- Dual USB-C: Native OTG (J_USB1) + CH340K UART (J_USB2)
- MicroSD experience packet logging (FAT32, circular buffer)
- NodeMesh: Node1 leader, Node2/Node3 ESP-NOW camera nodes
- ROS2 AI control stack on laptop
- StallGuard2 sensorless homing (DIAG on `GPIO1`/`GPIO2`/`GPIO3`/`GPIO48`)

---

## 16. Summary Statement

SixEyes is a reproducible, low-cost research platform for Vision–Language–Action robotics.

The embedded system focuses on **deterministic control**, **hardware safety**, and **real-time actuation**. All intelligence is handled externally by a laptop running ROS2 and VLA models. This separation keeps the hardware simple, robust, and accessible to researchers.

---

*For Alpha hardware see `docs/hardware/legacy/alpha/`. For PCB design detail see `docs/references/SixEyes_Node0.pdf`. For firmware config see `firmware/legacy/beta/follower_esp32/src/modules/config/`.*
