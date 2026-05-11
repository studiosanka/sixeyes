# Dual-Controller Pinout & Wiring Matrix

Consolidated pinout/GPIO reference for `leader_esp32`, `follower_esp32`, 4× TMC2209, servos, inter-board UART, and power rails.

Primary source: `docs/references/SixEyes Technical Reference 2.txt`.

---

## 1) Leader ESP32-C6 SuperMini (Teleoperation Controller)

### Analog Inputs (Potentiometers)

| Joint Input | ESP32-C6 GPIO |
|:------------|:--------------|
| Pot 1 | GPIO1 |
| Pot 2 | GPIO2 |
| Pot 3 | GPIO3 |
| Pot 4 | GPIO4 |
| Pot 5 | GPIO5 |
| Pot 6 | GPIO6 |

### UART to Follower (Dedicated Inter-Board Link)

| Signal | Leader GPIO | Connects To (Follower) |
|:-------|:------------|:-----------------------|
| TX | GPIO17 | RX on GPIO38 |
| RX | GPIO18 | TX on GPIO39 |

### Power (Leader)

| Rail | Notes |
|:-----|:------|
| 3.3V | From follower PCB or local LDO (standalone mode) |
| GND | Common ground with follower required |

---

## 2) Follower ESP32-S3 (Actuated Arm Controller)

### TMC2209 Stepper Driver Channels (x4)

| Channel | Joint | STEP | DIR | EN | PDN_UART |
|:--------|:------|:-----|:----|:---|:---------|
| J1 | Base | GPIO4 | GPIO5 | GPIO6 | GPIO7 |
| J2 | Shoulder A | GPIO8 | GPIO9 | GPIO10 | GPIO11 |
| J3 | Shoulder B | GPIO12 | GPIO13 | GPIO14 | GPIO15 |
| J4 | Elbow | GPIO16 | GPIO17 | GPIO18 | GPIO21 |

Notes:
- EN control is safety-gated in firmware.
- PDN_UART lines use series resistor + pull-up per hardware reference.

### Servo Outputs (x3)

| Function | Follower GPIO | Power Rail |
|:---------|:--------------|:-----------|
| Wrist Pitch | GPIO35 | 6.6V |
| Wrist Yaw | GPIO36 | 6.6V |
| Gripper | GPIO37 | 6.6V |

### UART to Leader (Dedicated Inter-Board Link)

| Signal | Follower GPIO | Connects To (Leader) |
|:-------|:--------------|:---------------------|
| RX | GPIO38 | TX on GPIO17 |
| TX | GPIO39 | RX on GPIO18 |

### USB to Laptop

| Interface | Notes |
|:----------|:------|
| Native USB-CDC | Follower↔Laptop command/telemetry path |

---

## 3) Power Rails & Distribution

| Rail | Source | Primary Loads |
|:-----|:-------|:--------------|
| 24V | Main input (≥10A recommended) | TMC2209 VM, onboard buck converters |
| 6.6V | XL4016 buck (on PCB) | 3× servos |
| 3.3V | MP1584 buck (on PCB) | ESP32 logic, TMC2209 VIO, digital logic |

Grounding:
- Use common ground star point near power entry.
- Keep servo high-current return paths away from MCU signal ground.

---

## 4) Camera Architecture (Revised)

Camera is not routed through follower PCB.

| Path | Connection |
|:-----|:-----------|
| Camera stream | Camera → USB cable → Laptop |

Embedded boards focus on motor/sensor/safety/telemetry; laptop handles camera + VLA stack.

---

## 5) Firmware Mapping Status (Mar 2026)

| Item | Status | Firmware Reference |
|:-----|:-------|:-------------------|
| Leader ADC pins GPIO1..GPIO6 | ✅ Aligned | `sixeyes/firmware/leader_esp32/src/main.cpp` (ESP32-C6 SuperMini) |
| Follower TMC2209 pin map | ✅ Aligned | `sixeyes/firmware/follower_esp32/src/modules/drivers/tmc2209/tmc2209_config.h` |
| Follower UART pins GPIO38/39 | ✅ Aligned | `sixeyes/firmware/follower_esp32/src/modules/config/board_config.h`, `.../uart_leader.cpp` |
| Servo outputs GPIO35/36/37 | ✅ Aligned | `sixeyes/firmware/follower_esp32/src/modules/servo_control/servo_manager.h` |
