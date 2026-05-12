# SixEyes Leader PCB Design Package (ESP32-C6 SuperMini)

This document is a practical, copy-into-CAD checklist for the leader teleoperation PCB.

Design intent for this revision:
- Single-layer routing for prototype bring-up.
- Through-hole parts only for this prototype spin.
- ESP32-C6 SuperMini connected as a pluggable devboard via header pins.
- Board function is analog capture + UART link (no motor power stage on leader board).

---

## 1) Locked Pinout (Firmware-Verified)

Authoritative mapping is based on current leader firmware.

### Potentiometer Inputs (6 channels)

| Joint Input | ESP32-C6 GPIO | Net Name |
|:--|:--|:--|
| Pot 1 | GPIO1 | POT1_WIPER |
| Pot 2 | GPIO2 | POT2_WIPER |
| Pot 3 | GPIO3 | POT3_WIPER |
| Pot 4 | GPIO4 | POT4_WIPER |
| Pot 5 | GPIO5 | POT5_WIPER |
| Pot 6 | GPIO6 | POT6_WIPER |

### Inter-Board UART (Leader -> Follower)

| Signal | Leader GPIO | Net Name |
|:--|:--|:--|
| TX (to follower RX) | GPIO17 | UART_FOLLOWER_RX |
| RX (from follower TX) | GPIO18 | UART_FOLLOWER_TX |

### Power Nets

| Net | Notes |
|:--|:--|
| +3V3_LOGIC | Main logic rail for ESP32-C6 and pots |
| GND | Common ground (must be shared with follower/laptop side) |

---

## 2) Net Naming Standard (Use Exactly)

Use these exact net names in schematic and PCB:

- POT1_WIPER, POT2_WIPER, POT3_WIPER, POT4_WIPER, POT5_WIPER, POT6_WIPER
- POT_BUS_3V3, POT_BUS_GND
- UART_FOLLOWER_RX, UART_FOLLOWER_TX
- +3V3_LOGIC, GND

Optional debug nets:
- TP_POT1, TP_POT2, TP_POT3, TP_POT4, TP_POT5, TP_POT6
- TP_UART_TX, TP_UART_RX

---

## 3) Full Netlist by Connector/Block

## 3.1 ESP32-C6 SuperMini Header to Nets

| MCU GPIO | Net |
|:--|:--|
| 1 | POT1_WIPER |
| 2 | POT2_WIPER |
| 3 | POT3_WIPER |
| 4 | POT4_WIPER |
| 5 | POT5_WIPER |
| 6 | POT6_WIPER |
| 17 | UART_FOLLOWER_RX |
| 18 | UART_FOLLOWER_TX |

Power/header pins:
- 3V3 -> +3V3_LOGIC
- GND -> GND

## 3.2 Potentiometer Connectors (3-pin JST each)

Use 6 identical connectors (J_POT1..J_POT6), each 3 pins:
- Pin 1: POT_BUS_3V3
- Pin 2: POTx_WIPER
- Pin 3: POT_BUS_GND

Mapping:
- J_POT1 pin 2 -> POT1_WIPER
- J_POT2 pin 2 -> POT2_WIPER
- J_POT3 pin 2 -> POT3_WIPER
- J_POT4 pin 2 -> POT4_WIPER
- J_POT5 pin 2 -> POT5_WIPER
- J_POT6 pin 2 -> POT6_WIPER

## 3.3 Leader/Follower UART Connector

4-pin recommended header (H_UART1):
- Pin 1: GND
- Pin 2: UART_FOLLOWER_RX (leader TX, to follower RX)
- Pin 3: UART_FOLLOWER_TX (leader RX, from follower TX)
- Pin 4: +3V3_LOGIC (optional, only if power-sharing is required)

## 3.4 Power Input

Leader board can be powered by either:
- USB (via ESP32-C6 SuperMini onboard USB), or
- External +3V3_LOGIC input header.

If external 3.3V is provided:
- H_PWR1 pin 1: +3V3_LOGIC
- H_PWR1 pin 2: GND

Important:
- Do not apply 5V directly to +3V3_LOGIC net.
- Keep one common ground between leader, follower, and host interfaces.

---

## 4) Footprints and Package Choices

Use common KiCad library footprints where possible.

Prototype policy for this revision:
- Through-hole parts only.
- No SMD passives in this build.

## 4.1 Recommended Footprint Map

| Ref Type | Suggested Footprint |
|:--|:--|
| ESP32-C6 SuperMini socket headers | PinSocket_1x12_P2.54mm_Vertical x2 (adjust pin count to your exact SuperMini variant) |
| Pot connectors (x6) | JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical |
| UART link | PinHeader_1x04_P2.54mm_Vertical |
| Optional external 3.3V power header | PinHeader_1x02_P2.54mm_Vertical |
| Test points | TestPoint_Pad_D1.5mm |
| Mount holes | MountingHole_3.2mm_M3 |
| Through-hole resistors | R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal |
| Through-hole electrolytics | CP_Radial_D5.0mm_P2.00mm, CP_Radial_D6.3mm_P2.50mm |
| Through-hole ceramic caps | C_Disc_D5.0mm_W2.5mm_P5.00mm |

## 4.1.1 Devboard Header Footprints (Explicit)

ESP32-C6 SuperMini is a pluggable devboard:
- PCB footprint: PinSocket_1x12_P2.54mm_Vertical x2 (or your measured count).
- Devboard mating part: PinHeader_1x12_P2.54mm_Vertical x2 soldered on module.

If your module is not 1x12 per side:
- Keep 2.54 mm pitch and replace only pin count to match exact board.

Mechanical/orientation notes:
- Add clear silkscreen marker for pin 1.
- Keep at least 2.0 mm courtyard around socket rows.
- Lock one orientation only (no mirrored insertion).

## 4.2 Discrete Passives BOM (Through-Hole)

| RefDes | Qty | Value | Type | Voltage/Power | Net Placement |
|:--|:--:|:--|:--|:--|:--|
| C1 | 1 | 47 uF | Electrolytic radial | 10V min | +3V3_LOGIC to GND near ESP32 header |
| C2 | 1 | 100 nF | Ceramic disc THT | 25V min | +3V3_LOGIC to GND near ESP32 header |
| C3-C8 | 6 | 100 nF | Ceramic disc THT | 25V min | One from each POTx_WIPER to GND near ESP32 ADC pins |
| R1-R6 | 6 | 1 k | Axial resistor | 1/4W | Series from each pot wiper connector pin to POTx_WIPER net |
| R7-R8 | 2 | 220 ohm | Axial resistor | 1/4W | Series on UART_FOLLOWER_RX and UART_FOLLOWER_TX (optional EMI damping) |
| C9 | 1 | 10 uF | Electrolytic radial | 10V min | +3V3_LOGIC to GND near pot connector cluster |

Notes:
- R7-R8 can be DNI if UART link is short and clean.
- If ADC response is too slow, reduce C3-C8 from 100 nF to 10 nF.

## 4.3 Discrete Passives Wiring Guide

1. 3.3V rail stabilization:
- Place C1 and C2 across +3V3_LOGIC and GND near ESP32 power pins.
- Place C9 across +3V3_LOGIC and GND near pot connector fan-out.

2. Per-channel ADC conditioning:
- Route each pot wiper through series resistor R1..R6 into POTx_WIPER nets.
- Place C3..C8 from each POTx_WIPER net to GND close to ESP32 side.

3. UART conditioning:
- Place R7 in series with UART_FOLLOWER_RX near ESP32 side.
- Place R8 in series with UART_FOLLOWER_TX near ESP32 side.

---

## 5) Trace Width, Clearance, and Via Rules

For low-current leader board signals (1 oz copper):

## 5.1 Net Class Rules (Recommended)

| Net Class | Width | Clearance | Via Drill / Dia | Applies To |
|:--|:--|:--|:--|:--|
| ADC_SIGNAL | 0.25 mm (10 mil) | 0.20 mm (8 mil) | 0.30 / 0.60 mm | POTx_WIPER |
| UART_SIGNAL | 0.25 mm (10 mil) | 0.20 mm (8 mil) | 0.30 / 0.60 mm | UART_FOLLOWER_RX/TX |
| PWR_3V3 | 0.50 mm (20 mil) | 0.20 mm (8 mil) | 0.30 / 0.60 mm | +3V3_LOGIC |
| GND_RETURN | 0.80 mm (31 mil) where traced, otherwise pour | 0.20 mm (8 mil) | 0.30 / 0.60 mm | Ground returns and links |

## 5.2 Single-Layer + Jumper Strategy

- Route +3V3 and GND trunks first.
- Route six ADC channels away from UART traces where possible.
- Use wire jumpers only for unavoidable crossings.
- Stitch isolated ground-pour islands with labeled GND jumpers.

---

## 6) Spacing and Placement Rules (Practical)

- Keep ADC routing short from connectors to ESP32 pins.
- Keep pot bus traces (POT_BUS_3V3 and POT_BUS_GND) as clean shared rails.
- Place UART header near one board edge for cable access.
- Separate analog wiper traces from noisy USB/UART area when possible.

---

## 7) ERC/DRC Checklist Before Fabrication

Schematic/ERC:
- POT1..POT6 wipers map exactly to GPIO1..GPIO6.
- UART pins map exactly: TX GPIO17, RX GPIO18.
- All pot connectors use 3-pin order: 3V3, WIPER, GND.
- Through-hole package footprints are used for all passives.

PCB/DRC:
- No unrouted nets.
- No isolated copper islands left unintentionally.
- Ground continuity verified end-to-end.

Bring-up:
- Power with USB first and verify stable +3V3_LOGIC.
- Sweep each pot and confirm JOINT_STATE channel changes correctly.
- Verify UART link to follower at 115200.

---

## 8) NodeMesh/Teleop Notes for Leader Board

- This board is teleoperation input only (no motor driver stage).
- Keep board modular: ADC capture + UART transport.
- Maintain firmware-compatible GPIO mapping in section 1.

---

## 9) Schematic Symbols (What To Use)

Use connector-style symbols so net names remain firmware-readable.

Recommended symbol naming:
- U1: ESP32_C6_SUPERMINI_2x12
- J_POT1..J_POT6: JST3_POT
- H_UART1: HDR_1x04_UART
- H_PWR1: HDR_1x02_3V3

### 9.1 ESP32-C6 SuperMini Symbol (U1)

Create symbol ESP32_C6_SUPERMINI_2x12 with at least these pins exposed:

| Symbol Pin Name | MCU GPIO | Net Label |
|:--|:--|:--|
| GPIO1 | 1 | POT1_WIPER |
| GPIO2 | 2 | POT2_WIPER |
| GPIO3 | 3 | POT3_WIPER |
| GPIO4 | 4 | POT4_WIPER |
| GPIO5 | 5 | POT5_WIPER |
| GPIO6 | 6 | POT6_WIPER |
| GPIO17 | 17 | UART_FOLLOWER_RX |
| GPIO18 | 18 | UART_FOLLOWER_TX |
| 3V3 | - | +3V3_LOGIC |
| GND | - | GND |

### 9.2 Pot Connector Symbol (J_POTx)

Create symbol JST3_POT:

| Pin | Net |
|:--|:--|
| 1 | POT_BUS_3V3 |
| 2 | POTx_WIPER |
| 3 | POT_BUS_GND |

### 9.3 UART Header Symbol (H_UART1)

Create symbol HDR_1x04_UART:

| Pin | Net |
|:--|:--|
| 1 | GND |
| 2 | UART_FOLLOWER_RX |
| 3 | UART_FOLLOWER_TX |
| 4 | +3V3_LOGIC (optional) |

### 9.4 Power Header Symbol (H_PWR1)

Create symbol HDR_1x02_3V3:

| Pin | Net |
|:--|:--|
| 1 | +3V3_LOGIC |
| 2 | GND |

### 9.5 Power Symbols and Net Labels

Use global labels:
- +3V3_LOGIC
- GND
- POT_BUS_3V3
- POT_BUS_GND

---

## 10) Quick Copy Block for CAD Project Notes

Pin map summary:
- POT ADC: GPIO1,2,3,4,5,6
- UART: TX GPIO17, RX GPIO18

Prototype constraints:
- Single-layer priority.
- Through-hole only passives and connectors.
- ESP32-C6 SuperMini mounted via header sockets.
