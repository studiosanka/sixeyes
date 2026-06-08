# SixEyes Leader PCB Design Package — Beta (ESP32-C3-MINI-1)

Design intent for this revision:
- ESP32-C3-MINI-1 module soldered directly to PCB via castellated pads (no devkit, no socket headers).
- SMD passives (0402/0603).
- Board function is analog capture + UART link only. No motor stage.
- USB-C for power and flashing. The ESP32-C3 has a built-in USB Serial/JTAG peripheral on GPIO18/19 — no USB-UART bridge chip (CH340, CP2102) needed.
- Boot and reset buttons for manual download-mode entry.

> Firmware must be updated from the alpha GPIO map. See section 1 for the new locked pinout.

---

## 1) Locked Pinout (Firmware Target)

### Potentiometer Inputs (6 channels)

All six channels are on ADC1. ADC2 is unused.

| Joint Input | ESP32-C3 GPIO | ADC Channel | Net Name     |
|:------------|:--------------|:------------|:-------------|
| Pot 1       | GPIO0         | ADC1_CH0    | POT1_WIPER   |
| Pot 2       | GPIO1         | ADC1_CH1    | POT2_WIPER   |
| Pot 3       | GPIO2         | ADC1_CH2    | POT3_WIPER   |
| Pot 4       | GPIO3         | ADC1_CH3    | POT4_WIPER   |
| Pot 5       | GPIO4         | ADC1_CH4    | POT5_WIPER   |
| Pot 6       | GPIO5         | ADC1_CH5    | POT6_WIPER   |

### Inter-Board UART (Leader ↔ Follower)

UART1, remapped via GPIO matrix. UART0 (GPIO20/21) is reserved for flash/debug.

| Signal                  | ESP32-C3 GPIO | Net Name           |
|:------------------------|:--------------|:-------------------|
| TX → follower RX        | GPIO7         | UART_FOLLOWER_RX   |
| RX ← follower TX        | GPIO6         | UART_FOLLOWER_TX   |

### USB Flashing Interface

Native USB Serial/JTAG — no bridge chip required.

| Signal | ESP32-C3 GPIO | Net Name |
|:-------|:--------------|:---------|
| USB D− | GPIO18        | USB_DM   |
| USB D+ | GPIO19        | USB_DP   |

Connect USB-C D−/D+ directly to GPIO18/GPIO19. The C3's built-in peripheral handles enumeration and flashing.

### Strapping Pin Awareness

| GPIO  | Strapping Function               | Action                                              |
|:------|:---------------------------------|:----------------------------------------------------|
| GPIO8 | Boot mode (HIGH = normal boot)   | Pull to +3V3_LOGIC via 10 kΩ                        |
| GPIO9 | Download mode (LOW = USB DL)     | Pull to +3V3_LOGIC via 10 kΩ; BOOT button shorts to GND |

For manual download mode: hold BOOT (GPIO9 low) while pressing RESET (EN low then released).

### Power Nets

| Net          | Notes                                                     |
|:-------------|:----------------------------------------------------------|
| +5V_USB      | From USB-C VBUS, input to LDO                             |
| +3V3_LOGIC   | LDO output, powers C3-MINI-1 and all pots                |
| USB_DM       | USB D−, GPIO18, routes to USB-C D− pin                   |
| USB_DP       | USB D+, GPIO19, routes to USB-C D+ pin                   |
| GND          | Common ground shared with follower                        |

---

## 2) Net Naming Standard (Use Exactly)

- `POT1_WIPER`, `POT2_WIPER`, `POT3_WIPER`, `POT4_WIPER`, `POT5_WIPER`, `POT6_WIPER`
- `POT_BUS_3V3`, `POT_BUS_GND`
- `UART_FOLLOWER_RX`, `UART_FOLLOWER_TX`
- `USB_DM`, `USB_DP`
- `+5V_USB`, `+3V3_LOGIC`, `GND`

Optional debug nets:
- `TP_POT1` … `TP_POT6`
- `TP_UART_TX`, `TP_UART_RX`
- `TP_3V3`, `TP_5V`

---

## 3) Full Netlist by Connector/Block

### 3.1 ESP32-C3-MINI-1 Castellated Pads to Nets

| Module Pin | Net                           |
|:-----------|:------------------------------|
| GPIO0      | POT1_WIPER                    |
| GPIO1      | POT2_WIPER                    |
| GPIO2      | POT3_WIPER                    |
| GPIO3      | POT4_WIPER                    |
| GPIO4      | POT5_WIPER                    |
| GPIO5      | POT6_WIPER                    |
| GPIO6      | UART_FOLLOWER_TX              |
| GPIO7      | UART_FOLLOWER_RX              |
| GPIO8      | +3V3_LOGIC via R_BOOT2 (10 kΩ) |
| GPIO9      | +3V3_LOGIC via R_BOOT1 (10 kΩ); BOOT button to GND |
| GPIO18     | USB_DM                        |
| GPIO19     | USB_DP                        |
| 3V3        | +3V3_LOGIC                    |
| GND        | GND                           |
| EN         | +3V3_LOGIC via R_EN (10 kΩ); RESET button to GND |

### 3.2 Potentiometer Connectors — JST XH 3-pin (×6)

Designators J_POT1 … J_POT6. Pin order matches JST XH B3B-XH-A:

| Pin | Net          |
|:----|:-------------|
| 1   | POT_BUS_3V3  |
| 2   | POTx_WIPER   |
| 3   | POT_BUS_GND  |

Mapping:
- J_POT1 pin 2 → POT1_WIPER
- J_POT2 pin 2 → POT2_WIPER
- J_POT3 pin 2 → POT3_WIPER
- J_POT4 pin 2 → POT4_WIPER
- J_POT5 pin 2 → POT5_WIPER
- J_POT6 pin 2 → POT6_WIPER

### 3.3 Leader/Follower UART Connector — JST XH 4-pin

Designator J_UART1. Pin order must match follower Node0 UART header exactly:

| Pin | Net                | Direction          |
|:----|:-------------------|:-------------------|
| 1   | GND                | Common             |
| 2   | UART_FOLLOWER_RX   | Leader TX → Follower RX |
| 3   | UART_FOLLOWER_TX   | Follower TX → Leader RX |
| 4   | +3V3_LOGIC         | Optional power share |

### 3.4 USB-C Connector (Power + Flashing)

Designator J_USB1:

| Pin     | Net       | Notes                                          |
|:--------|:----------|:-----------------------------------------------|
| VBUS    | +5V_USB   | To LDO VIN                                     |
| GND     | GND       |                                                |
| D−      | USB_DM    | To GPIO18                                      |
| D+      | USB_DP    | To GPIO19                                      |
| CC1     | GND via R10 (5.1 kΩ) | USB-C sink pull-down                |
| CC2     | GND via R11 (5.1 kΩ) | USB-C sink pull-down                |

Keep USB_DM and USB_DP traces short, equal-length, and away from ADC signals. Target impedance: 90 Ω differential.

### 3.5 LDO — 3.3 V Regulator

Use AMS1117-3.3 (SOT-223) or equivalent.

| LDO Pin | Net         |
|:--------|:------------|
| VIN     | +5V_USB     |
| VOUT    | +3V3_LOGIC  |
| GND     | GND         |

---

## 4) Bill of Materials (SMD)

| RefDes  | Qty | Value      | Package   | Voltage / Rating | Net Placement                                      |
|:--------|:---:|:-----------|:----------|:-----------------|:---------------------------------------------------|
| U1      | 1   | ESP32-C3-MINI-1 | Module, castellated | 3.3 V | Main MCU                                    |
| U2      | 1   | AMS1117-3.3 | SOT-223  | 1 A, 5 V input   | LDO, +5V_USB → +3V3_LOGIC                         |
| C1      | 1   | 10 µF      | 0603      | 10 V             | LDO input cap (+5V_USB to GND)                    |
| C2      | 1   | 100 nF     | 0402      | 10 V             | LDO input bypass                                   |
| C3      | 1   | 22 µF      | 0805      | 10 V             | LDO output bulk cap (+3V3_LOGIC to GND)            |
| C4      | 1   | 100 nF     | 0402      | 10 V             | LDO output bypass                                  |
| C5      | 1   | 4.7 µF     | 0603      | 10 V             | +3V3_LOGIC bulk near C3-MINI-1 VCC pad            |
| C6      | 1   | 100 nF     | 0402      | 10 V             | +3V3_LOGIC bypass near C3-MINI-1 VCC pad          |
| C7–C12  | 6   | 100 nF     | 0402      | 10 V             | One per POTx_WIPER to GND, near C3-MINI-1 GPIO pads |
| R1–R6   | 6   | 1 kΩ       | 0402      | 1/16 W           | Series on each POTx_WIPER between connector and ADC pad |
| R7–R8   | 2   | 220 Ω      | 0402      | 1/16 W           | Series on UART_FOLLOWER_RX and UART_FOLLOWER_TX (DNI if link is short and clean) |
| R9      | 1   | 10 kΩ      | 0402      | 1/16 W           | EN pull-up to +3V3_LOGIC (R_EN)                    |
| R10–R11 | 2   | 5.1 kΩ     | 0402      | 1/16 W           | USB-C CC1 and CC2 to GND                           |
| R12     | 1   | 10 kΩ      | 0402      | 1/16 W           | GPIO9 pull-up to +3V3_LOGIC (R_BOOT1)              |
| R13     | 1   | 10 kΩ      | 0402      | 1/16 W           | GPIO8 pull-up to +3V3_LOGIC (R_BOOT2)              |
| S1      | 1   | BOOT button  | SMD tactile | —             | GPIO9 to GND — hold during reset to enter download mode |
| S2      | 1   | RESET button | SMD tactile | —             | EN to GND — resets the C3                          |

---

## 5) Passive Wiring Guide

**LDO supply chain:**
- Place C1 and C2 on the +5V_USB input side of U2, as close to VIN as possible.
- Place C3 and C4 on the +3V3_LOGIC output of U2.
- Route +3V3_LOGIC to C5 and C6 before distributing to U1 VCC and the pot bus.

**Per-channel ADC RC filter (cutoff ~1.6 kHz, sufficient for 100 Hz sampling):**
- R1 in series from J_POT1 pin 2 → POT1_WIPER node.
- C7 from POT1_WIPER node to GND, placed at the C3-MINI-1 GPIO0 pad.
- Repeat for R2/C8 … R6/C12 on channels 2–6.

**UART conditioning:**
- R7 in series on UART_FOLLOWER_RX trace, placed near U1.
- R8 in series on UART_FOLLOWER_TX trace, placed near U1.
- Mark R7 and R8 DNI if cable length from leader to follower is under 30 cm.

**EN pull-up:**
- R9 from EN pad of U1 to +3V3_LOGIC.

---

## 6) Footprints

| Ref Type                  | Footprint                                          |
|:--------------------------|:---------------------------------------------------|
| U1 ESP32-C3-MINI-1        | `RF_Module:ESP32-C3-MINI-1` (KiCad official)       |
| U2 AMS1117-3.3            | `Package_TO_SOT_SMD:SOT-223-3_TabPin2`             |
| J_POT1–J_POT6 (JST XH 3-pin) | `Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical` |
| J_UART1 (JST XH 4-pin)    | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical` |
| J_USB1 USB-C              | `Connector_USB:USB_C_Receptacle_GCT_USB4135` (or equivalent with D+/D− exposed)  |
| S1 BOOT, S2 RESET         | `Button_Switch_SMD:SW_SPST_B3U-1000P` or equivalent 3×2 mm SMD tactile           |
| C (0402)                  | `Capacitor_SMD:C_0402_1005Metric`                  |
| C (0603)                  | `Capacitor_SMD:C_0603_1608Metric`                  |
| C (0805)                  | `Capacitor_SMD:C_0805_2012Metric`                  |
| R (0402)                  | `Resistor_SMD:R_0402_1005Metric`                   |
| Test points               | `TestPoint:TestPoint_Pad_D1.5mm`                   |
| Mount holes               | `MountingHole:MountingHole_3.2mm_M3`               |

---

## 7) Trace Width and Net Class Rules

2-layer board assumed, 1 oz copper.

| Net Class    | Width              | Clearance    | Via Drill / Pad | Applies To                    |
|:-------------|:-------------------|:-------------|:----------------|:------------------------------|
| ADC_SIGNAL   | 0.15 mm            | 0.15 mm      | 0.30 / 0.60 mm  | POTx_WIPER                    |
| UART_SIGNAL  | 0.15 mm            | 0.15 mm      | 0.30 / 0.60 mm  | UART_FOLLOWER_RX/TX           |
| USB_DIFF     | 0.20 mm, 90 Ω diff | 0.20 mm      | 0.30 / 0.60 mm  | USB_DM / USB_DP (route as differential pair, equal length) |
| PWR_3V3      | 0.50 mm            | 0.20 mm      | 0.30 / 0.60 mm  | +3V3_LOGIC                    |
| PWR_5V       | 0.50 mm            | 0.20 mm      | 0.30 / 0.60 mm  | +5V_USB                       |
| GND_POUR     | Pour               | 0.20 mm      | 0.30 / 0.60 mm  | GND plane (both layers)       |

Use a solid GND pour on both layers and stitch with vias every ~5 mm.

---

## 8) Placement and Routing Guidelines

- Place U1 (C3-MINI-1) with antenna edge facing away from all connectors and power traces. Keep a copper-free zone at least 3 mm beyond the antenna end of the module on all layers.
- Place J_USB1 near the board edge. Route USB_DM/USB_DP as a differential pair directly from J_USB1 to GPIO18/19 with minimal stubs. Keep away from ADC traces.
- Place S1 (BOOT) and S2 (RESET) near the U1 GPIO9 and EN pads, accessible from the board edge.
- Place LDO (U2) between J_USB1 and U1. Keep the +5V trace short.
- Cluster J_POT1–J_POT6 along one board edge for clean cable exit. Fan R1–R6 and C7–C12 between the connectors and U1 GPIO pads.
- Place J_UART1 on the opposite edge from the pot connectors, near U1 GPIO6/7 pads.
- Route ADC traces (POTx_WIPER) away from USB and UART traces to avoid noise injection.

---

## 9) ERC/DRC Checklist Before Fabrication

Schematic/ERC:
- GPIO0–5 connected to POT1–6 wipers through R1–R6 (ADC RC filter complete).
- GPIO6 → UART_FOLLOWER_TX, GPIO7 → UART_FOLLOWER_RX.
- GPIO18 → USB_DM, GPIO19 → USB_DP, both connecting to J_USB1 D−/D+ pins.
- GPIO8 pulled up to +3V3_LOGIC via R13. GPIO9 pulled up via R12 with S1 to GND.
- EN pulled up to +3V3_LOGIC via R9 with S2 to GND.
- USB-C CC1 and CC2 each have 5.1 kΩ to GND (R10, R11).
- LDO VIN bypassed (C1, C2), VOUT bypassed (C3, C4).
- All JST connector pin 1 oriented consistently (mark in silkscreen).

PCB/DRC:
- No unrouted nets.
- GND pours on both layers, stitched with vias.
- Antenna keep-out zone respected on all layers (no copper under PCB antenna area of U1).
- USB_DM / USB_DP routed as differential pair, equal length, no stubs.
- All SMD pads have paste layer assigned.
- Courtyard clearances respected around U1 module.

Bring-up sequence:
1. Apply USB power. Measure +5V_USB and +3V3_LOGIC before populating U1.
2. Solder U1. Hold BOOT (S1), press RESET (S2), release RESET, release BOOT. Board should enumerate as a USB Serial device.
3. Flash minimal firmware via `esptool.py` or the Arduino IDE over USB.
4. Sweep each pot and confirm ADC readings on GPIO0–5.
5. Verify UART1 link to follower at 921,600 baud on GPIO6/7.

---

## 10) Schematic Symbols

| Ref             | Symbol Name             | Notes                                                              |
|:----------------|:------------------------|:-------------------------------------------------------------------|
| U1              | `ESP32-C3-MINI-1`       | Use Espressif KiCad library; expose GPIO0–9, GPIO18, GPIO19, EN, 3V3, GND |
| U2              | `AMS1117-3.3`           | SOT-223, VIN/VOUT/GND                                             |
| J_POT1–J_POT6   | `JST3_POT`              | 3-pin: POT_BUS_3V3 / POTx_WIPER / POT_BUS_GND                    |
| J_UART1         | `JST4_UART`             | 4-pin: GND / UART_FOLLOWER_RX / UART_FOLLOWER_TX / +3V3_LOGIC     |
| J_USB1          | `USB_C_Receptacle`      | Expose VBUS, GND, D−, D+, CC1, CC2                                |
| S1              | `SW_Push`               | BOOT — GPIO9 to GND                                               |
| S2              | `SW_Push`               | RESET — EN to GND                                                 |

Global power labels: `+5V_USB`, `+3V3_LOGIC`, `GND`

---

## 11) Quick Copy Block for CAD Notes

```
MCU: ESP32-C3-MINI-1 (castellated, 3.3 V, integrated 4 MB flash + PCB antenna)
ADC: GPIO0–5 → POT1–POT6 (ADC1 only, no ADC2)
UART1: GPIO7 TX (→ follower RX) / GPIO6 RX (← follower TX)
USB flashing: native USB Serial/JTAG — GPIO18 (D−) / GPIO19 (D+) → USB-C D−/D+, no bridge chip
Boot mode: hold S1 (GPIO9 low) + press/release S2 (EN low) → release S1
RC filter: 1 kΩ series + 100 nF to GND per ADC channel (~1.6 kHz cutoff)
Power: USB-C VBUS → AMS1117-3.3 → +3V3_LOGIC
Pot connectors: JST XH 3-pin, order = [3V3 | WIPER | GND]
UART connector: JST XH 4-pin, order = [GND | LEADER_TX | LEADER_RX | 3V3]
```
