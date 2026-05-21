# Dual-Controller Pinout & Wiring Matrix

Consolidated pinout/GPIO reference for `leader_esp32`, `follower_esp32`, 4× TMC2209, servos, inter-board UART, and power rails.

Primary source: `docs/references/SixEyes Technical Reference 2.txt`.

---

## 1) Leader ESP32-C6 SuperMini (Teleoperation Controller)

### Analog Inputs (Potentiometers)

| Joint Input | ESP32-C6 GPIO |
|:------------|:--------------|
| Pot 1 | GPIO0 |
| Pot 2 | GPIO1 |
| Pot 3 | GPIO2 |
| Pot 4 | GPIO3 |
| Pot 5 | GPIO4 |
| Pot 6 | GPIO6 |

### UART to Follower (Dedicated Inter-Board Link)

| Signal | Leader GPIO | Connects To (Follower) |
|:-------|:------------|:-----------------------|
| TX | GPIO17 | RX on GPIO18 |
| RX | GPIO18 | TX on GPIO17 |

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
| J1 | Base | GPIO12 | GPIO11 | GPIO14 (shared EN_ALL) | GPIO13 |
| J2 | Shoulder A | GPIO9 | GPIO8 | GPIO14 (shared EN_ALL) | GPIO10 |
| J3 | Shoulder B | GPIO15 | GPIO7 | GPIO14 (shared EN_ALL) | GPIO16 |
| J4 | Elbow | GPIO5 | GPIO4 | GPIO14 (shared EN_ALL) | GPIO6 |

Notes:
- EN control is safety-gated in firmware.
- PDN_UART lines use series resistor + pull-up per hardware reference.

### Servo Outputs (x3)

| Function | Follower GPIO | Power Rail |
|:---------|:--------------|:-----------|
| Wrist Pitch | GPIO40 | 6.6V |
| Wrist Yaw | GPIO41 | 6.6V |
| Gripper | GPIO42 | 6.6V |

### SD Card SPI + Detect

| Signal | Follower GPIO |
|:-------|:--------------|
| SD_MOSI | GPIO35 |
| SD_SCK | GPIO36 |
| SD_MISO | GPIO37 |
| SD_CS | GPIO38 |
| SD_CD | GPIO39 |

### UART to Leader (Dedicated Inter-Board Link)

| Signal | Follower GPIO | Connects To (Leader) |
|:-------|:--------------|:---------------------|
| RX | GPIO18 | TX on GPIO17 |
| TX | GPIO17 | RX on GPIO18 |

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
| Leader ADC pins GPIO0,1,2,3,4,6 | ✅ Aligned | `sixeyes/firmware/leader_esp32/src/main.cpp` (ESP32-C6 SuperMini) |
| Follower TMC2209 pin map | ✅ Aligned | `sixeyes/firmware/follower_esp32/src/modules/drivers/tmc2209/tmc2209_config.h` |
| Follower UART pins GPIO18/17 | ✅ Aligned | `sixeyes/firmware/follower_esp32/src/modules/config/board_config.h`, `.../uart_leader.cpp` |
| Servo outputs GPIO40/41/42 | ✅ Aligned | `sixeyes/firmware/follower_esp32/src/modules/servo_control/servo_manager.h` |
