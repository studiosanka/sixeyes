# SixEyes Follower PCB Design Package — Beta Rev2 (ESP32-S3-WROOM-1-N16R8)

Design intent for this revision:
- ESP32-S3-WROOM-1-N16R8 module soldered directly to PCB via castellated pads (no devkit, no socket headers). **N16R8 variant is mandatory** — 16 MB Flash, 8 MB Octal PSRAM.
- SMD passives (0402/0603/0805). Single-sided SMT (top layer only).
- 60×60 mm 4-layer PCB.
- Board function: 4-axis stepper control + 3-axis servo PWM + inter-board UART + MicroSD logging + USB-CDC ROS2 bridge + CH340K flash port.
- Dual USB-C: native OTG (J_USB1, GPIO19/20) for ROS2 bridge and telemetry; CH340K UART bridge (J_USB2, GPIO43/44) for firmware flash and debug.
- 24 V DC input via XT30 (J_PWR). Three downstream rails: +24V_MOTOR (direct to TMC2209), +6.6V_SERVO (TPS54540B buck), +3.3V (LMR14030S → MIC5219-3.3 LDO).
- Leader UART link to Node1 at 921,600 baud on GPIO41/42 (UART1, GPIO matrix).

> The N16R8 octal-PSRAM variant permanently commits GPIO26/33/34/35/36/37 to internal silicon. These must never appear on any net, pad, pull resistor, or via on the PCB. See section 1 for the full forbidden list.

> Firmware must use the Beta Rev2 GPIO map. Update `board_config.h` and `tmc2209_config.h` before flashing. See section 1 for the locked pinout.

---

## 1) Locked Pinout (Firmware Target)

### Forbidden GPIO Lines — N16R8 Octal PSRAM/Flash

| GPIO  | Internal Function              |
|:------|:-------------------------------|
| GPIO26 | Internal octal flash clock    |
| GPIO33 | Flash D4                      |
| GPIO34 | Flash D5                      |
| GPIO35 | Octal PSRAM D6 / WP           |
| GPIO36 | Octal PSRAM D7 / HOLD         |
| GPIO37 | Octal PSRAM CLK               |

### Stepper Motor Outputs (4 channels — TMC2209)

| Joint        | STEP GPIO | DIR GPIO | PDN_UART GPIO | DIAG GPIO |
|:-------------|:----------|:---------|:--------------|:----------|
| J1 — Base    | GPIO5     | GPIO6    | GPIO7         | GPIO1     |
| J2 — Shldr A | GPIO8     | GPIO9    | GPIO14        | GPIO2     |
| J3 — Shldr B | GPIO15    | GPIO16   | GPIO17        | GPIO3     |
| J4 — Elbow   | GPIO18    | GPIO21   | GPIO47        | GPIO48    |

**Global driver enable:** GPIO4 → `DRV_EN_ALL` (active LOW, shared to all 4 TMC2209 EN pins).

PDN_UART conditioning: 1 kΩ series resistor + 4.7 kΩ pull-up to +3.3V per channel.

DIAG conditioning: 47 kΩ pull-up to +3.3V per channel. Open-drain — pull-up is mandatory.

### Servo Outputs (3 channels — LEDC / MCPWM)

Servos powered from dedicated +6.6V_SERVO rail.

| Servo         | GPIO   | Net Name         | Connector |
|:--------------|:-------|:-----------------|:----------|
| Wrist Pitch   | GPIO38 | SRV_WRIST_PITCH  | J_SV1     |
| Wrist Roll    | GPIO39 | SRV_WRIST_ROLL   | J_SV2     |
| Gripper       | GPIO40 | SRV_GRIPPER      | J_SV3     |

### Inter-Board UART — Leader Link (Node0 ↔ Node1)

UART1, remapped via GPIO matrix. UART0 (GPIO43/44) is reserved for CH340K flash/debug.

| Signal                    | GPIO   | Net Name       | Direction                        |
|:--------------------------|:-------|:---------------|:---------------------------------|
| TX → Node1 RX             | GPIO41 | LDR_UART_TX    | Node0 TX → J_LDR pin 3 → Node1  |
| RX ← Node1 TX             | GPIO42 | LDR_UART_RX    | Node0 RX ← J_LDR pin 2 ← Node1  |

Baud: 921,600, 8N1. Magic-number scan + CRC-CCITT (poly `0x1021`) framing. Packets carry absolute joint targets for all 6 joints.

### Native USB OTG Interface (J_USB1)

| Signal | GPIO   | Net Name |
|:-------|:-------|:---------|
| USB D− | GPIO19 | USB_DM   |
| USB D+ | GPIO20 | USB_DP   |

Route as 90 Ω differential pair on L1 only. Used for ROS2 USB-CDC bridge and telemetry streaming.

### CH340K UART Bridge Interface (J_USB2)

| Signal      | GPIO   | Net Name    |
|:------------|:-------|:------------|
| UART0 TX    | GPIO43 | UART0_TXD   |
| UART0 RX    | GPIO44 | UART0_RXD   |

Connects to CH340K RXD/TXD pins. DTR/RTS auto-reset for PlatformIO upload.

### MicroSD — Hardware SPI

| Signal  | GPIO   | Net Name | Pull-up      |
|:--------|:-------|:---------|:-------------|
| SD CS   | GPIO10 | SD_CS    | 10 kΩ to +3.3V |
| SD MOSI | GPIO11 | SD_MOSI  | 10 kΩ to +3.3V |
| SD SCK  | GPIO12 | SD_SCK   | —            |
| SD MISO | GPIO13 | SD_MISO  | 10 kΩ to +3.3V |

FAT32, 32 kB clusters, 1 MB pre-allocated circular buffer, flush every 8 writes.

### Strapping Pin Awareness

| GPIO   | Strapping Function                  | Action                                        |
|:-------|:------------------------------------|:----------------------------------------------|
| GPIO0  | Boot mode (HIGH = normal boot)      | Pull to +3.3V via 10 kΩ; BOOT button to GND  |
| GPIO46 | Must be LOW at reset                | Pull to GND via 10 kΩ (MCU_STRAP_LOW)        |

### Power Nets

| Net            | Notes                                                                 |
|:---------------|:----------------------------------------------------------------------|
| +24V_MOTOR     | From XT30 J_PWR, direct to TMC2209 VM pins (fused)                   |
| +6.6V_SERVO    | TPS54540B buck output; powers J_SV1–J_SV3                            |
| +5.0V          | LMR14030S buck output; powers J_CAM1, J_CAM2, J_LDR_PWR, MIC5219 LDO input |
| +3.3V          | MIC5219-3.3 LDO output; powers ESP32-S3 core, TMC2209 VIO, pull-up networks |
| GND            | Common ground; solid pour L2                                         |

---

## 2) Net Naming Standard (Use Exactly)

**Motor nets:**
- `J1_BASE_STEP`, `J1_BASE_DIR`, `J1_BASE_PDN`, `J1_BASE_DIAG`
- `J2_SHLDA_STEP`, `J2_SHLDA_DIR`, `J2_SHLDA_PDN`, `J2_SHLDA_DIAG`
- `J3_SHLDB_STEP`, `J3_SHLDB_DIR`, `J3_SHLDB_PDN`, `J3_SHLDB_DIAG`
- `J4_ELBOW_STEP`, `J4_ELBOW_DIR`, `J4_ELBOW_PDN`, `J4_ELBOW_DIAG`
- `DRV_EN_ALL`

**Motor phase nets (stepper coils):**
- `J1_A1`, `J1_A2`, `J1_B1`, `J1_B2`
- `J2_A1`, `J2_A2`, `J2_B1`, `J2_B2`
- `J3_A1`, `J3_A2`, `J3_B1`, `J3_B2`
- `J4_A1`, `J4_A2`, `J4_B1`, `J4_B2`

**Servo nets:**
- `SRV_WRIST_PITCH`, `SRV_WRIST_ROLL`, `SRV_GRIPPER`

**Leader UART nets:**
- `LDR_UART_TX`, `LDR_UART_RX`

**USB nets:**
- `USB_DM`, `USB_DP`
- `UART0_TXD`, `UART0_RXD`

**SD nets:**
- `SD_CS`, `SD_MOSI`, `SD_SCK`, `SD_MISO`

**Boot/strap nets:**
- `MCU_BOOT`, `MCU_STRAP_LOW`

**Power nets:**
- `+24V_MOTOR`, `+6.6V_SERVO`, `+5.0V`, `+3.3V`, `GND`

Optional test point nets:
- `TP_3V3`, `TP_5V`, `TP_24V`
- `TP_J1_STEP` … `TP_J4_STEP`
- `TP_LDR_TX`, `TP_LDR_RX`

---

## 3) Full Netlist by Connector/Block

### 3.1 ESP32-S3-WROOM-1-N16R8 Castellated Pads to Nets

| Module Pin | Net                            | Notes                          |
|:-----------|:-------------------------------|:-------------------------------|
| GPIO0      | MCU_BOOT                       | 10 kΩ pull-up; BOOT button to GND |
| GPIO1      | J1_BASE_DIAG                   | 47 kΩ pull-up to +3.3V        |
| GPIO2      | J2_SHLDA_DIAG                  | 47 kΩ pull-up to +3.3V        |
| GPIO3      | J3_SHLDB_DIAG                  | 47 kΩ pull-up to +3.3V        |
| GPIO4      | DRV_EN_ALL                     | Active LOW, all 4 TMC2209 EN   |
| GPIO5      | J1_BASE_STEP                   | → TMC2209-1 STEP               |
| GPIO6      | J1_BASE_DIR                    | → TMC2209-1 DIR                |
| GPIO7      | J1_BASE_PDN                    | 1 kΩ series + 4.7 kΩ pull-up  |
| GPIO8      | J2_SHLDA_STEP                  | → TMC2209-2 STEP               |
| GPIO9      | J2_SHLDA_DIR                   | → TMC2209-2 DIR                |
| GPIO10     | SD_CS                          | 10 kΩ pull-up to +3.3V        |
| GPIO11     | SD_MOSI                        | 10 kΩ pull-up to +3.3V        |
| GPIO12     | SD_SCK                         |                                |
| GPIO13     | SD_MISO                        | 10 kΩ pull-up to +3.3V        |
| GPIO14     | J2_SHLDA_PDN                   | 1 kΩ series + 4.7 kΩ pull-up  |
| GPIO15     | J3_SHLDB_STEP                  | → TMC2209-3 STEP               |
| GPIO16     | J3_SHLDB_DIR                   | → TMC2209-3 DIR                |
| GPIO17     | J3_SHLDB_PDN                   | 1 kΩ series + 4.7 kΩ pull-up  |
| GPIO18     | J4_ELBOW_STEP                  | → TMC2209-4 STEP               |
| GPIO19     | USB_DM                         | 90 Ω diff pair → J_USB1 D−     |
| GPIO20     | USB_DP                         | 90 Ω diff pair → J_USB1 D+     |
| GPIO21     | J4_ELBOW_DIR                   | → TMC2209-4 DIR                |
| GPIO26     | **FORBIDDEN — do not connect** | Internal octal flash clock     |
| GPIO33     | **FORBIDDEN — do not connect** | Flash D4                       |
| GPIO34     | **FORBIDDEN — do not connect** | Flash D5                       |
| GPIO35     | **FORBIDDEN — do not connect** | Octal PSRAM D6 / WP            |
| GPIO36     | **FORBIDDEN — do not connect** | Octal PSRAM D7 / HOLD          |
| GPIO37     | **FORBIDDEN — do not connect** | Octal PSRAM CLK                |
| GPIO38     | SRV_WRIST_PITCH                | LEDC/MCPWM → J_SV1 signal     |
| GPIO39     | SRV_WRIST_ROLL                 | LEDC/MCPWM → J_SV2 signal     |
| GPIO40     | SRV_GRIPPER                    | LEDC/MCPWM → J_SV3 signal     |
| GPIO41     | LDR_UART_TX                    | → J_LDR pin 3 → Node1 RX      |
| GPIO42     | LDR_UART_RX                    | ← J_LDR pin 2 ← Node1 TX      |
| GPIO43     | UART0_TXD                      | → CH340K RXD                   |
| GPIO44     | UART0_RXD                      | ← CH340K TXD                   |
| GPIO46     | MCU_STRAP_LOW                  | 10 kΩ pull-down to GND        |
| GPIO47     | J4_ELBOW_PDN                   | 1 kΩ series + 4.7 kΩ pull-up  |
| GPIO48     | J4_ELBOW_DIAG                  | 47 kΩ pull-up to +3.3V        |
| 3V3        | +3.3V                          |                                |
| GND        | GND                            |                                |

### 3.2 Stepper Motor Connectors — JST XH 2.5mm 4-pin (×4)

Designators J_M1 … J_M4. Pin order: A1 / A2 / B1 / B2 (matches standard NEMA17 coil pairs).

| Pin | Net          |
|:----|:-------------|
| 1   | Jx_A1        |
| 2   | Jx_A2        |
| 3   | Jx_B1        |
| 4   | Jx_B2        |

- J_M1: J1 Base stepper coil phases
- J_M2: J2 Shoulder A stepper coil phases
- J_M3: J3 Shoulder B stepper coil phases
- J_M4: J4 Elbow stepper coil phases

Motor phases connect to TMC2209 OA1/OA2/OB1/OB2 output pins. Route as wide traces (0.80 mm minimum) direct to board edge.

### 3.3 Servo Connectors — 2.54 mm 3-pin (×3)

Signal / +6.6V_SERVO / GND per channel.

| Connector | Signal Net       | GPIO   |
|:----------|:-----------------|:-------|
| J_SV1     | SRV_WRIST_PITCH  | GPIO38 |
| J_SV2     | SRV_WRIST_ROLL   | GPIO39 |
| J_SV3     | SRV_GRIPPER      | GPIO40 |

Place 10 µF ceramic bypass cap at each header between +6.6V_SERVO and GND. Servo GND return must terminate at L2 GND plane via a dedicated via — do not share a trace segment with MCU signal GND.

### 3.4 Leader UART Connector — 2.54 mm 4-pin

Designator J_LDR. Pin order must match Node1 (leader) J_UART1 header exactly for straight-through cable.

| Pin | Net            | Direction                          |
|:----|:---------------|:-----------------------------------|
| 1   | GND            | Common                             |
| 2   | LDR_UART_RX    | Node1 TX → Node0 RX (GPIO42)       |
| 3   | LDR_UART_TX    | Node0 TX (GPIO41) → Node1 RX       |
| 4   | +3.3V          | Optional power share to Node1      |

### 3.5 Leader Power Header — JST XH 2.5mm 2-pin

Designator J_LDR_PWR. Supplies +5.0V and GND to Node1 leader board when Node1 USB-C is not populated. Leave unpopulated when Node1 is self-powered via USB-C.

| Pin | Net     |
|:----|:--------|
| 1   | +5.0V   |
| 2   | GND     |

Place 10 µF ceramic bypass cap at the header pins.

### 3.6 Camera Power Headers — JST XH 2.5mm 2-pin (×2)

Data is exchanged wirelessly via ESP-NOW channel 6. These headers carry power only.

| Connector | Net     | Node        |
|:----------|:--------|:------------|
| J_CAM1    | +5.0V   | Node2 — global camera |
| J_CAM2    | +5.0V   | Node3 — wrist camera  |

Both connectors: pin 1 = +5.0V, pin 2 = GND. Place 10 µF ceramic bypass cap at each header to absorb ESP-NOW TX current burst (~200 mA step).

### 3.7 USB-C Connectors — J_USB1 and J_USB2

**J_USB1 (Native USB OTG — West Edge):**

| Pin   | Net      | Notes                                    |
|:------|:---------|:-----------------------------------------|
| VBUS  | +5.0V    | Optional input (see note)                |
| GND   | GND      |                                          |
| D−    | USB_DM   | To GPIO19, 90 Ω diff pair               |
| D+    | USB_DP   | To GPIO20, 90 Ω diff pair               |
| CC1   | GND via R_CC1 (5.1 kΩ) | USB-C sink pull-down       |
| CC2   | GND via R_CC2 (5.1 kΩ) | USB-C sink pull-down       |

Keep USB_DM/USB_DP traces short, equal-length, L1 only, away from switching nodes and motor phase traces. Target impedance: 90 Ω differential.

**J_USB2 (CH340K UART Bridge — West Edge):**

| Pin   | Net           | Notes                              |
|:------|:--------------|:-----------------------------------|
| VBUS  | +5.0V         |                                    |
| GND   | GND           |                                    |
| D−    | CH340K USB D− | Internal to CH340K                 |
| D+    | CH340K USB D+ | Internal to CH340K                 |
| CC1   | GND via 5.1 kΩ | USB-C sink pull-down              |
| CC2   | GND via 5.1 kΩ | USB-C sink pull-down              |

### 3.8 Power Input — XT30 Connector

Designator J_PWR (West Edge):

| Pin | Net         |
|:----|:------------|
| 1   | +24V_MOTOR  |
| 2   | GND         |

Add bulk electrolytic cap and TVS diode immediately inboard of J_PWR to absorb inrush and clamping transients.

### 3.9 MicroSD Card Slot

Designator J_SD (Interior zone). Hirose DM3D-SF push-pull:

| Signal   | Net      | Pull-up          |
|:---------|:---------|:-----------------|
| CS       | SD_CS    | 10 kΩ to +3.3V  |
| MOSI     | SD_MOSI  | 10 kΩ to +3.3V  |
| SCK      | SD_SCK   | —                |
| MISO     | SD_MISO  | 10 kΩ to +3.3V  |
| VCC      | +3.3V    | 100 nF bypass at slot |
| GND      | GND      |                  |

### 3.10 Power Regulators

**TPS54540B — 6.6 V Buck (U6):**

| Pin   | Net            |
|:------|:---------------|
| VIN   | +24V_MOTOR     |
| SW    | To inductor L6 |
| VOUT  | +6.6V_SERVO    |
| GND   | GND            |
| RT/CLK | Freq-set resistor to GND |
| EN    | +24V_MOTOR via R_EN6 (pull-up, or tie to VIN) |
| SS/TR | Soft-start cap to GND |
| BOOT  | Bootstrap cap to SW  |

Feedback: R_FB_TOP = 102 kΩ, R_FB_BOT = 14 kΩ (1% tolerance, 0603). V_out = 0.8 V × (1 + 102k/14k) ≈ 6.63 V.

**LMR14030S — 5.0 V Buck (U7):**

| Pin   | Net       |
|:------|:----------|
| VIN   | +24V_MOTOR |
| SW    | To inductor L7 |
| VOUT  | +5.0V      |
| GND   | GND        |

**MIC5219-3.3YM5 — 3.3 V LDO (U8):**

| Pin  | Net      |
|:-----|:---------|
| VIN  | +5.0V    |
| VOUT | +3.3V    |
| GND  | GND      |
| EN   | +5.0V (tie HIGH for always-on) |

---

## 4) Bill of Materials (SMD)

| RefDes    | Qty | Value / Part               | Package     | Rating / Notes                                      |
|:----------|:---:|:---------------------------|:------------|:----------------------------------------------------|
| U1        | 1   | ESP32-S3-WROOM-1-N16R8     | Module, castellated | 3.3 V, 16 MB Flash, 8 MB Octal PSRAM, PCB antenna |
| U2–U5     | 4   | TMC2209-LA                 | QFN-28      | 2×2 grid, stepper motor drivers                     |
| U6        | 1   | TPS54540B                  | HSOP-8-EP   | 6.6 V synchronous buck, 5.5 A                       |
| U7        | 1   | LMR14030S                  | HSOP-8-EP   | 5.0 V asynchronous buck, 3 A                        |
| U8        | 1   | MIC5219-3.3YM5             | SOT-23-5    | 3.3 V LDO, 1 A, powered from +5.0V                 |
| U9        | 1   | CH340K                     | ESSOP-10    | USB-UART bridge, J_USB2 flash port                  |
| L6        | 1   | 4.7 µH shielded inductor   | ≥10×10 mm   | I_sat > 9.0 A (e.g. Bourns SRR1260) — 6.6V buck    |
| L7        | 1   | 4.7 µH shielded inductor   | ≥6×6 mm     | I_sat > 3.5 A — 5.0V buck                          |
| C_VM1–C_VM4 | 4 | 100 µF / 35 V electrolytic | Case D      | Low-ESR (Panasonic FR / Nichicon UWX). **No tantalum.** One per TMC2209 VM |
| C_VMH1–C_VMH4 | 4 | 100 nF X7R 50 V          | 0603        | HF bypass at each TMC2209 VM pin (≤1 mm trace)      |
| C_VIO1–C_VIO4 | 4 | 1.0 µF X7R               | 0603        | TMC2209 VIO filter                                  |
| C_CP1–C_CP4 | 4 | 100 nF X7R 50 V           | 0603        | TMC2209 charge pump (VCP–CPI)                       |
| C_VR1–C_VR4 | 4 | 4.7 µF X5R               | 0603        | TMC2209 internal VOUT regulator                     |
| C_SVO_B   | 2   | 470 µF / 16 V electrolytic | Case E      | 6.6V buck output bulk (2× in parallel = 940 µF)    |
| C_SVO_BP  | 1   | 10 µF ceramic             | 0805        | 6.6V buck output HF bypass                         |
| C_SVO_H1–C_SVO_H3 | 3 | 10 µF ceramic      | 0805        | One per servo header J_SV1–J_SV3                    |
| C_6IN     | 1   | 10 µF ceramic             | 0805        | 6.6V buck input bypass                              |
| C_6INB    | 1   | 100 nF X7R                | 0402        | 6.6V buck input HF bypass                          |
| C_5OUT    | 1   | 100 µF / 16 V electrolytic | Case C      | 5.0V buck output bulk                               |
| C_5OUTb   | 1   | 10 µF ceramic             | 0805        | 5.0V buck output bypass                             |
| C_5IN     | 1   | 10 µF ceramic             | 0805        | 5.0V buck input bypass                              |
| C_LDO_IN  | 1   | 10 µF ceramic             | 0805        | MIC5219-3.3 input cap (from +5.0V)                 |
| C_LDO_OUT | 1   | 22 µF ceramic             | 0805        | MIC5219-3.3 output bulk                             |
| C_LDO_OUTb | 1  | 100 nF X7R                | 0603        | MIC5219-3.3 output bypass                           |
| C_U1_3V3  | 1   | 10 µF ceramic             | 0805        | ESP32-S3 module 3V3 supply bulk                     |
| C_U1_3V3b | 1   | 100 nF X7R                | 0402        | ESP32-S3 module 3V3 supply bypass                   |
| C_CH340_1 | 1   | 100 nF X7R 10 V           | 0402        | CH340K VCC decoupling                               |
| C_CH340_2 | 1   | 100 nF X7R 10 V           | 0402        | CH340K V3 (internal 3.3V) pin decoupling            |
| C_CH340_3 | 1   | 100 nF X7R                | 0402        | CH340K XI USB signal conditioning                   |
| C_CAM1    | 1   | 10 µF ceramic             | 0805        | J_CAM1 5V bypass (Node2 ESP-NOW burst)              |
| C_CAM2    | 1   | 10 µF ceramic             | 0805        | J_CAM2 5V bypass (Node3 ESP-NOW burst)              |
| C_LDR_PWR | 1   | 10 µF ceramic             | 0805        | J_LDR_PWR 5V bypass                                |
| C_SD      | 1   | 100 nF X7R                | 0402        | MicroSD VCC pin bypass                              |
| R_PDN1–R_PDN4 | 4 | 1 kΩ                    | 0402        | Series on each PDN_UART line                        |
| R_PDN_PU1–R_PDN_PU4 | 4 | 4.7 kΩ             | 0402        | Pull-up on each PDN_UART node to +3.3V             |
| R_DIAG1–R_DIAG4 | 4 | 47 kΩ                 | 0402        | Pull-up on each DIAG to +3.3V (open-drain mandatory)|
| R_FB_TOP  | 1   | 102 kΩ 1%               | 0603        | TPS54540B feedback divider top                      |
| R_FB_BOT  | 1   | 14 kΩ 1%                | 0603        | TPS54540B feedback divider bottom                   |
| R_SD1–R_SD3 | 3 | 10 kΩ                   | 0402        | SD_CS, SD_MOSI, SD_MISO pull-ups to +3.3V          |
| R_BOOT    | 1   | 10 kΩ                   | 0402        | GPIO0 pull-up to +3.3V (MCU_BOOT)                  |
| R_STRAP46 | 1   | 10 kΩ                   | 0402        | GPIO46 pull-down to GND (MCU_STRAP_LOW)             |
| R_EN_ALL  | 1   | 10 kΩ                   | 0402        | DRV_EN_ALL pull-up to +3.3V (active-LOW safety)     |
| R_CC1–R_CC4 | 4 | 5.1 kΩ                  | 0402        | USB-C CC1/CC2 to GND, 2 per port × 2 ports         |
| S1        | 1   | BOOT button (SMD tactile) | 3×2 mm     | GPIO0 to GND — hold during reset to enter download mode |

---

## 5) Passive Wiring Guide

**24 V motor rail (per TMC2209, ×4):**
- Place C_VMx (100 µF electrolytic) and C_VMHx (100 nF ceramic) within 2 mm of each TMC2209 VM pin. Electrolytic only — no tantalum.
- C_VIOx (1.0 µF) from TMC2209 VIO pin to GND.
- C_CPx (100 nF) between TMC2209 VCP and CPI pins (charge pump).
- C_VRx (4.7 µF) from TMC2209 VOUT pin to GND.

**PDN_UART conditioning (per driver, ×4):**
- R_PDNx (1 kΩ) in series between GPIO pad and the TMC2209 PDN_UART pin.
- R_PDN_PUx (4.7 kΩ) from the TMC2209-side node to +3.3V.
- This forms a single-wire UART half-duplex bus per driver. Each driver is individually addressed.

**DIAG pull-ups (per driver, ×4):**
- R_DIAGx (47 kΩ) from TMC2209 DIAG pin to +3.3V. DIAG is open-drain; no pull-up = indeterminate logic level. Mandatory.

**Global driver enable:**
- R_EN_ALL (10 kΩ) from DRV_EN_ALL net to +3.3V. Ensures drivers are disabled at power-up before firmware asserts the pin low.

**6.6 V servo rail supply chain:**
- Place C_6IN and C_6INB on the +24V_MOTOR input side of U6, close to VIN.
- Place C_SVO_B (2× 470 µF) and C_SVO_BP (10 µF) on the +6.6V_SERVO output of U6.
- Place C_SVO_H1–C_SVO_H3 (10 µF) at each servo header J_SV1–J_SV3 between +6.6V_SERVO and GND.
- R_FB_TOP/R_FB_BOT form the voltage-divider feedback to TPS54540B VSENSE pin. Use 1% tolerance resistors.

**5.0 V logic rail supply chain:**
- Place C_5IN on the +24V_MOTOR input side of U7.
- Place C_5OUT and C_5OUTb on the +5.0V output of U7.
- Distribute +5.0V to J_CAM1, J_CAM2, J_LDR_PWR headers, and MIC5219-3.3 VIN.

**3.3 V logic supply chain:**
- C_LDO_IN on MIC5219-3.3 input (+5.0V side).
- C_LDO_OUT and C_LDO_OUTb on MIC5219-3.3 output (+3.3V side).
- Place C_U1_3V3 and C_U1_3V3b in-route to the ESP32-S3 module 3V3 pad (power via → cap → module pad, not a stub branch).

**Boot/strap:**
- R_BOOT (10 kΩ) from GPIO0 to +3.3V. S1 pulls GPIO0 to GND for manual boot-mode override.
- R_STRAP46 (10 kΩ) from GPIO46 to GND. This is required by ESP32-S3 strapping specification.

**USB-C CC pull-downs:**
- R_CC1/R_CC2 (5.1 kΩ) for J_USB1 CC1/CC2 to GND.
- R_CC3/R_CC4 (5.1 kΩ) for J_USB2 CC1/CC2 to GND.

**MicroSD pull-ups:**
- R_SD1 (10 kΩ) on SD_CS to +3.3V.
- R_SD2 (10 kΩ) on SD_MOSI to +3.3V.
- R_SD3 (10 kΩ) on SD_MISO to +3.3V.

---

## 6) Footprints

| Ref Type                         | Footprint                                                     |
|:---------------------------------|:--------------------------------------------------------------|
| U1 ESP32-S3-WROOM-1-N16R8        | `RF_Module:ESP32-S3-WROOM-1` (KiCad official)                |
| U2–U5 TMC2209-LA                 | `Package_DFN_QFN:QFN-28-1EP_5x5mm_P0.5mm`                   |
| U6 TPS54540B                     | `Package_SO:HSOP-8-1EP_3.89x4.89mm_P1.27mm`                  |
| U7 LMR14030S                     | `Package_SO:HSOP-8-1EP_3.89x4.89mm_P1.27mm`                  |
| U8 MIC5219-3.3YM5                | `Package_TO_SOT_SMD:SOT-23-5`                                 |
| U9 CH340K                        | `Package_SO:ESSOP-10_3.9x4.9mm_P1.0mm`                       |
| J_M1–J_M4 (JST XH 4-pin)        | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical`        |
| J_SV1–J_SV3 (2.54 mm 3-pin)     | `Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical`  |
| J_CAM1, J_CAM2 (JST XH 2-pin)   | `Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical`        |
| J_LDR (2.54 mm 4-pin)           | `Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical`  |
| J_LDR_PWR (JST XH 2-pin)        | `Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical`        |
| J_USB1, J_USB2 USB-C             | `Connector_USB:USB_C_Receptacle_GCT_USB4135` (or equivalent with D+/D− exposed) |
| J_PWR XT30                       | `Connector_Amass:AMASS_XT30PW-M_1x02_P5.0mm_Horizontal` (RA) |
| J_SD MicroSD                     | `Connector_Card:MicroSD_Hirose_DM3D-SF`                       |
| S1 BOOT                          | `Button_Switch_SMD:SW_SPST_B3U-1000P` or equivalent 3×2 mm SMD tactile |
| C (0402)                         | `Capacitor_SMD:C_0402_1005Metric`                             |
| C (0603)                         | `Capacitor_SMD:C_0603_1608Metric`                             |
| C (0805)                         | `Capacitor_SMD:C_0805_2012Metric`                             |
| C electrolytic case D            | `Capacitor_SMD:CP_Elec_8x10`                                  |
| R (0402)                         | `Resistor_SMD:R_0402_1005Metric`                              |
| R (0603)                         | `Resistor_SMD:R_0603_1608Metric`                              |
| L6, L7 shielded inductors        | Per manufacturer datasheet (Bourns SRR1260 or equivalent)     |
| Test points                      | `TestPoint:TestPoint_Pad_D1.5mm`                              |
| Mount holes                      | `MountingHole:MountingHole_3.2mm_M3`                          |

---

## 7) Trace Width and Net Class Rules

4-layer board, 1 oz copper per layer.

| Net Class        | Width              | Clearance    | Via Drill / Pad | Applies To                              |
|:-----------------|:-------------------|:-------------|:----------------|:----------------------------------------|
| PWR_24V          | 1.20 mm            | 0.40 mm      | 0.40 / 0.70 mm  | +24V_MOTOR                              |
| PWR_SERVO        | 1.50 mm            | 0.30 mm      | 0.40 / 0.70 mm  | +6.6V_SERVO, J1_A1…J4_B2 motor phases  |
| PWR_LOGIC        | 0.50 mm            | 0.20 mm      | 0.30 / 0.60 mm  | +5.0V, +3.3V                            |
| MOTOR_PHASE      | 0.80 mm            | 0.35 mm      | 0.40 / 0.70 mm  | J1_A1/A2/B1/B2 … J4_A1/A2/B1/B2        |
| STEP_DIR_SIGNAL  | 0.15 mm            | 0.15 mm      | 0.30 / 0.60 mm  | Jx_BASE_STEP, DIR, PDN, DIAG, servo PWM |
| LEADER_UART      | 0.15 mm            | 0.15 mm      | 0.30 / 0.60 mm  | LDR_UART_TX, LDR_UART_RX               |
| USB_DIFF         | 0.25 mm, 90 Ω diff | 0.20 mm      | 0.25 / 0.50 mm  | USB_DM / USB_DP (L1 only, equal length) |
| SPI_SD           | 0.15 mm            | 0.15 mm      | 0.30 / 0.60 mm  | SD_CS, SD_MOSI, SD_SCK, SD_MISO         |
| GND_POUR         | Pour               | 0.20 mm      | 0.30 / 0.60 mm  | Solid GND plane L2; pour L1/L4         |

Use a solid GND pour on L2 (primary plane). Pour GND on L1 and L4 around all components and stitch with vias every ~5 mm at board perimeter and along USB differential pair routes.

4-layer stackup: L1 (signal/component) — prepreg — L2 (solid GND) — core — L3 (split power plane: +24V_MOTOR / +6.6V_SERVO / +5.0V / +3.3V) — prepreg — L4 (signal escape / thermal pour under TMC2209).

Order with controlled impedance: 90 Ω differential for USB pairs on L1.

---

## 8) Placement and Routing Guidelines

- Place U1 (ESP32-S3-WROOM-1-N16R8) with the PCB antenna edge on the North board edge, overhanging by ≥ 2 mm. Maintain a copper-free keep-out zone 15×10 mm under the antenna across all 4 layers — no connectors, vias, pours, or mounting hardware in this zone.
- Place the TMC2209 driver matrix (U2–U5) in a 2×2 grid in the East-Centre zone. Thermal via farms (3×3 grid, 0.3 mm drill, ≤1 mm pitch) through each QFN-28 exposed pad, connecting L1→L2→L4. Expose copper on L4 below the farm area (no solder mask) to act as a passive thermal radiator.
- Place J_PWR (XT30) on the West Edge. Cluster U6, U7, U8 and all bulk electrolytic caps immediately inboard of J_PWR to minimise high-current 24 V loop area.
- Place J_USB1 and J_USB2 on the West Edge adjacent to J_PWR. Route USB_DM/USB_DP as a differential pair exclusively on L1 from connectors to GPIO19/20 — no layer changes, no stubs. Keep away from TMC2209 switching nodes and motor phase traces.
- Cluster J_M1–J_M4 and J_SV1–J_SV3 along the South and East edges. Motor phase traces route outward directly — wide (0.80 mm), short, no routing under TMC2209 switching zones.
- Place J_CAM1, J_CAM2, J_LDR, and J_LDR_PWR on the South Edge.
- Place J_SD (MicroSD) in the interior zone, away from the buck switching node polygons.
- Buck SW node polygons (TPS54540B and LMR14030S) must be wide, short copper fills on L1. No signal trace on L1 or L4 may pass beneath a switching node polygon. Affected signals: ESP-NOW receive paths (LDR_UART_RX, SPI MISO).
- Servo GND return currents must not share any trace segment with MCU signal GND. Both must terminate separately at the L2 GND plane via dedicated vias.
- Place S1 (BOOT) near U1 GPIO0 pad, accessible from the board edge.
- Mark JST connector pin 1 orientation consistently in silkscreen.

---

## 9) ERC/DRC Checklist Before Fabrication

**Schematic/ERC:**
- GPIO0 → MCU_BOOT with 10 kΩ pull-up to +3.3V; S1 pulls to GND.
- GPIO46 → MCU_STRAP_LOW with 10 kΩ pull-down to GND.
- GPIO1/2/3/48 → DIAG nets, each with 47 kΩ pull-up to +3.3V.
- GPIO4 → DRV_EN_ALL with 10 kΩ pull-up to +3.3V; active-LOW to all 4 TMC2209 EN pins.
- GPIO5/6/7, GPIO8/9/14, GPIO15/16/17, GPIO18/21/47 → STEP/DIR/PDN nets for each TMC2209.
- Each PDN net: 1 kΩ series + 4.7 kΩ pull-up to +3.3V.
- GPIO10–13 → SD_CS/MOSI/SCK/MISO with pull-ups on CS, MOSI, MISO.
- GPIO19/20 → USB_DM/USB_DP → J_USB1 D−/D+.
- GPIO38/39/40 → SRV_WRIST_PITCH/SRV_WRIST_ROLL/SRV_GRIPPER → J_SV1/SV2/SV3 signal pins.
- GPIO41/42 → LDR_UART_TX/LDR_UART_RX → J_LDR pins 3/2.
- GPIO43/44 → UART0_TXD/UART0_RXD → CH340K RXD/TXD.
- Forbidden GPIOs 26/33/34/35/36/37: no nets, no pads, no pull resistors assigned.
- USB-C CC1/CC2: 5.1 kΩ to GND on both J_USB1 and J_USB2.
- TPS54540B feedback divider: R_FB_TOP = 102 kΩ, R_FB_BOT = 14 kΩ.
- MIC5219-3.3 EN pin tied to +5.0V.
- All TMC2209 VM bypass caps (100 µF electrolytic + 100 nF ceramic) present.

**PCB/DRC:**
- No unrouted nets.
- GND pour on L2 solid and unbroken. GND pour on L1/L4, stitched with vias every ~5 mm at perimeter.
- Antenna keep-out zone respected on all 4 layers (no copper under U1 PCB antenna area).
- USB_DM / USB_DP routed as differential pair on L1 only, equal length, no stubs, away from switching polygons.
- TMC2209 thermal via farms (3×3 per driver) present; L4 copper pour under drivers with solder mask removed.
- Motor phase traces ≥ 0.80 mm.
- No signal trace passes beneath TPS54540B or LMR14030S SW switching polygons.
- All SMD pads have paste layer assigned.
- Courtyard clearances respected around U1 module.
- JST connector pin 1 marked in silkscreen, orientation consistent across all headers.

**Bring-up sequence:**
1. Apply 24 V to J_PWR with all ICs populated except U1. Measure +24V_MOTOR, +6.6V_SERVO, +5.0V, and +3.3V before soldering U1.
2. Solder U1. Connect J_USB2 (CH340K port). PlatformIO should auto-reset via DTR/RTS and enumerate the CH340K as a COM port.
3. Flash minimal firmware via PlatformIO (`pio run -e beta_follower --target upload`). Alternatively, hold S1 (GPIO0 low), apply power, release S1 to enter manual download mode via J_USB1 native USB.
4. Connect J_USB1. Verify the board enumerates as a USB-CDC device (ROS2 bridge port).
5. Enable motors via firmware command. Verify DRV_EN_ALL (GPIO4) goes LOW and all TMC2209 EN pins respond.
6. Test each stepper axis with a step/dir pulse sequence. Confirm DIAG flags (GPIO1/2/3/48) are HIGH (no fault).
7. Verify servo PWM on GPIO38/39/40 with an oscilloscope before connecting MG996R servos to +6.6V_SERVO rail.
8. Connect Node1 leader board to J_LDR. Verify UART1 link at 921,600 baud; confirm joint target packets arrive on GPIO42.
9. Connect Node2/Node3 ESP32-CAM modules to J_CAM1/J_CAM2. Verify 5V supply and ESP-NOW packet reception on Node0.

---

## 10) Schematic Symbols

| Ref          | Symbol Name                 | Notes                                                              |
|:-------------|:----------------------------|:-------------------------------------------------------------------|
| U1           | `ESP32-S3-WROOM-1`          | Use Espressif KiCad library; expose GPIO0–21, GPIO38–48, 3V3, GND |
| U2–U5        | `TMC2209`                   | Expose STEP, DIR, PDN_UART, DIAG, VM, VIO, OA1/OA2/OB1/OB2, EN, VCP, CPI, VOUT |
| U6           | `TPS54540B`                 | VIN, SW, VOUT, GND, RT/CLK, EN, SS/TR, BOOT                       |
| U7           | `LMR14030S`                 | VIN, SW, VOUT, GND                                                 |
| U8           | `MIC5219-3.3YM5`            | VIN, VOUT, GND, EN — SOT-23-5                                      |
| U9           | `CH340K`                    | VCC, V3, D+, D−, TXD, RXD, DTR, RTS, XI, GND                     |
| J_M1–J_M4   | `JST4_MOTOR`                | 4-pin: A1 / A2 / B1 / B2                                          |
| J_SV1–J_SV3 | `SERVO_HDR`                 | 3-pin: Signal / +6.6V_SERVO / GND                                  |
| J_LDR        | `HDR4_UART`                 | 4-pin: GND / LDR_UART_RX / LDR_UART_TX / +3.3V                   |
| J_LDR_PWR    | `JST2_PWR`                  | 2-pin: +5.0V / GND                                                 |
| J_CAM1, J_CAM2 | `JST2_PWR`               | 2-pin: +5.0V / GND                                                 |
| J_USB1, J_USB2 | `USB_C_Receptacle`        | Expose VBUS, GND, D−, D+, CC1, CC2                                |
| J_PWR        | `XT30_2Pin`                 | +24V_MOTOR / GND                                                   |
| J_SD         | `MicroSD_Card_Kit`          | SPI: CS / MOSI / SCK / MISO / VCC / GND                           |
| S1           | `SW_Push`                   | BOOT — GPIO0 to GND                                               |

Global power labels: `+24V_MOTOR`, `+6.6V_SERVO`, `+5.0V`, `+3.3V`, `GND`

---

## 11) Quick Copy Block for CAD Notes

```
MCU: ESP32-S3-WROOM-1-N16R8 (castellated, 3.3 V core, 16 MB Flash, 8 MB Octal PSRAM, PCB antenna)
FORBIDDEN GPIO: 26, 33, 34, 35, 36, 37 — internal N16R8 silicon, no PCB connections
Stepper drivers: 4× TMC2209-LA (QFN-28, 2×2 grid)
  J1 Base:     STEP=GPIO5  DIR=GPIO6  PDN=GPIO7  DIAG=GPIO1
  J2 Shldr A:  STEP=GPIO8  DIR=GPIO9  PDN=GPIO14 DIAG=GPIO2
  J3 Shldr B:  STEP=GPIO15 DIR=GPIO16 PDN=GPIO17 DIAG=GPIO3
  J4 Elbow:    STEP=GPIO18 DIR=GPIO21 PDN=GPIO47 DIAG=GPIO48
  Global EN:   GPIO4 (active LOW, 10 kΩ pull-up)
  PDN cond:    1 kΩ series + 4.7 kΩ pull-up per driver
  DIAG cond:   47 kΩ pull-up per driver (open-drain, mandatory)
Servos (LEDC/MCPWM): GPIO38 Wrist Pitch → J_SV1 | GPIO39 Wrist Roll → J_SV2 | GPIO40 Gripper → J_SV3
MicroSD (SPI): CS=GPIO10 MOSI=GPIO11 SCK=GPIO12 MISO=GPIO13 (10 kΩ pull-ups on CS/MOSI/MISO)
Leader UART1 (921,600 baud): GPIO41 TX → Node1 / GPIO42 RX ← Node1 | J_LDR pin order [GND | RX | TX | 3V3]
USB OTG (J_USB1): GPIO19 D− / GPIO20 D+ — 90 Ω diff pair, L1 only
CH340K flash (J_USB2): GPIO43 TXD / GPIO44 RXD — DTR/RTS auto-reset
Boot: S1 pulls GPIO0 LOW for download mode; GPIO46 pulled LOW via 10 kΩ (strapping)
Power: 24 V → TPS54540B (6.6 V servo) / LMR14030S (5.0 V logic) / MIC5219-3.3 (3.3 V core)
Motor connectors: JST XH 2.5mm 4-pin, order = [A1 | A2 | B1 | B2]
Servo connectors: 2.54mm 3-pin, order = [Signal | 6.6V | GND]
Leader UART: 2.54mm 4-pin, order = [GND | LDR_RX | LDR_TX | 3V3]
Camera headers: JST XH 2.5mm 2-pin, power only (ESP-NOW data wireless)
```
