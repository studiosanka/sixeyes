# SixEyes Follower PCB Design Package (NodeMesh-Aware)

Migration note: active NodeMesh firmware and NodeMesh-specific architecture docs are maintained in [https://github.com/SixEyes-Open-Source/nodemesh](https://github.com/SixEyes-Open-Source/nodemesh).

This document is a practical, copy-into-CAD checklist for the first follower PCB prototype.

Design intent for this revision:
- Single-layer routing for prototype bring-up.
- Use jumper wires for unavoidable crossovers and to stitch ground islands/pours.
- Keep compatibility with current follower firmware and NodeMesh direction.
- Through-hole parts only for this prototype spin.

---

## 1) Locked Pinout (Firmware-Verified)

Authoritative mapping is based on current follower firmware and NodeMesh pin docs.

### Stepper Drivers (TMC2209 x4)

| Channel | Joint | STEP | DIR | EN | PDN_UART |
|:--|:--|:--|:--|:--|:--|
| J1 | Base | GPIO12 | GPIO11 | GPIO14 (shared EN_ALL) | GPIO13 |
| J2 | Shoulder A | GPIO9 | GPIO8 | GPIO14 (shared EN_ALL) | GPIO10 |
| J3 | Shoulder B | GPIO15 | GPIO7 | GPIO14 (shared EN_ALL) | GPIO16 |
| J4 | Elbow | GPIO5 | GPIO4 | GPIO14 (shared EN_ALL) | GPIO6 |

### Servo PWM Outputs

| Function | GPIO | Net Name |
|:--|:--|:--|
| Wrist Pitch | GPIO40 | SERVO_WRIST_PITCH |
| Wrist Yaw | GPIO41 | SERVO_WRIST_YAW |
| Gripper | GPIO42 | SERVO_GRIPPER |

### Inter-Board UART (Leader <-> Follower)

| Signal | Follower GPIO | Net Name |
|:--|:--|:--|
| RX (from leader TX) | GPIO18 | UART_LEADER_RX |
| TX (to leader RX) | GPIO17 | UART_LEADER_TX |

### Power Nets

| Net | Notes |
|:--|:--|
| +24V_MOTOR | TMC VMOT rail input |
| +6V6_SERVO | Servo rail from buck converter |
| +3V3_LOGIC | ESP32-S3 logic, TMC VIO, pull-ups |
| GND | Common ground |

### Reserved/Free GPIO in this revision

- Leave unconnected due to board constraints: GPIO3, GPIO46, GPIO19, GPIO20, GPIO45, GPIO0, GPIO44, GPIO43
- Reserved/free for future use: GPIO21, GPIO47, GPIO1, GPIO2
- Onboard RGB LED: GPIO48

---

## 2) Net Naming Standard (Use Exactly)

Use these exact net names in schematic and PCB for easy firmware cross-check:

- STEP_J1, DIR_J1, PDN_J1
- STEP_J2, DIR_J2, PDN_J2
- STEP_J3, DIR_J3, PDN_J3
- STEP_J4, DIR_J4, PDN_J4
- EN_ALL
- SERVO_WRIST_PITCH, SERVO_WRIST_YAW, SERVO_GRIPPER
- UART_LEADER_RX, UART_LEADER_TX
- SD_SCK, SD_MISO, SD_MOSI, SD_CS, SD_CD
- +24V_MOTOR, +6V6_SERVO, +3V3_LOGIC, GND

Optional debug nets (recommended):
- TP_EN_ALL
- TP_PDN_J1, TP_PDN_J2, TP_PDN_J3, TP_PDN_J4
- TP_UART_RX, TP_UART_TX

---

## 3) Full Netlist by Connector/Block

## 3.1 ESP32-S3 DevKitC-1 Header to Nets

| MCU GPIO | Net |
|:--|:--|
| 14 | EN_ALL |
| 13 | PDN_J1 |
| 12 | STEP_J1 |
| 11 | DIR_J1 |
| 10 | PDN_J2 |
| 9 | STEP_J2 |
| 8 | DIR_J2 |
| 18 | UART_LEADER_RX |
| 17 | UART_LEADER_TX |
| 16 | PDN_J3 |
| 15 | STEP_J3 |
| 7 | DIR_J3 |
| 6 | PDN_J4 |
| 5 | STEP_J4 |
| 4 | DIR_J4 |
| 35 | SD_MOSI |
| 36 | SD_SCK |
| 37 | SD_MISO |
| 38 | SD_CS |
| 39 | SD_CD |
| 40 | SERVO_WRIST_PITCH |
| 41 | SERVO_WRIST_YAW |
| 42 | SERVO_GRIPPER |

Power/header pins:
- 3V3 -> +3V3_LOGIC
- GND -> GND

## 3.2 TMC2209 Drivers (x4)

Per driver block Jx (stepstick-style 2x08 module):
- STEP pin -> STEP_Jx
- DIR pin -> DIR_Jx
- EN pin -> EN_ALL
- PDN pin -> PDN_Jx
- VIO / VDD -> +3V3_LOGIC
- VMOT / VM -> +24V_MOTOR
- GND pins -> GND
- Motor outputs A1/A2/B1/B2 -> matching motor connector Jx_MOTOR

Reference socket orientation used in this project:
- Logic row: EN, MS1, MS2, PDN, CLK, STEP, DIR
- Power/motor row: VM, GND, A2, A1, B1, B2, VDD, GND
- MS1, MS2, and CLK are reserved for future use unless explicitly wired.

Recommended support passives per driver:
- 100 nF through-hole decoupler at VIO-to-GND.
- 4.7 k pull-up on each PDN_Jx to +3V3_LOGIC.
- Optional 0R (through-hole axial) series link on each PDN_Jx for debug isolation.
- Bulk capacitor near VMOT entry area (see passives section).

## 3.3 Stepper Motor Outputs (6-pin JST, 4 used)

Use one 6-pin JST connector per NEMA17 stepper channel for 6-wire bipolar motors, leaving center taps open.

Per motor connector Jx_MOTOR (x4):
- Pin 1: A+ (TMC A1 / OUT1 / 1A)
- Pin 2: NC (center tap, leave open)
- Pin 3: A- (TMC A2 / OUT2 / 2A)
- Pin 4: B+ (TMC B1 / OUT3 / 1B)
- Pin 5: NC (center tap, leave open)
- Pin 6: B- (TMC B2 / OUT4 / 2B)

Channel mapping:
- J1_MOTOR -> TMC2209 J1 A1/A2/B1/B2 to connector pins 1/3/4/6
- J2_MOTOR -> TMC2209 J2 A1/A2/B1/B2 to connector pins 1/3/4/6
- J3_MOTOR -> TMC2209 J3 A1/A2/B1/B2 to connector pins 1/3/4/6
- J4_MOTOR -> TMC2209 J4 A1/A2/B1/B2 to connector pins 1/3/4/6

## 3.4 Servo Connectors (3-pin JST each)

For each servo connector:
- Pin 1: GND
- Pin 2: +6V6_SERVO
- Pin 3: Signal

Signal map:
- Servo 1 signal -> SERVO_WRIST_PITCH
- Servo 2 signal -> SERVO_WRIST_YAW
- Servo 3 signal -> SERVO_GRIPPER

## 3.5 Leader/Follower UART Connector

4-pin recommended header:
- Pin 1: GND
- Pin 2: UART_LEADER_RX (leader TX)
- Pin 3: UART_LEADER_TX (leader RX)
- Pin 4: +3V3_LOGIC (optional, only if power-sharing is required)

## 3.6 Power Input and Rails

Power terminals in this revision (5x 2-pin terminal blocks):
- J_PWR1 (PWR-IN-24V): external 24V input -> +24V_MOTOR, GND
- J_PWR2 (24V-OUT-BUCK3V3-IN): +24V_MOTOR, GND output to 3.3V buck input
- J_PWR3 (24V-OUT-BUCK6V6-IN): +24V_MOTOR, GND output to 6.6V buck input
- J_PWR4 (BUCK3V3-OUT-IN): external buck 3.3V output input -> +3V3_LOGIC, GND
- J_PWR5 (BUCK6V6-OUT-IN): external buck 6.6V output input -> +6V6_SERVO, GND

This architecture intentionally keeps buck converters off-board for prototype flexibility.

Ground strategy for single-layer prototype:
- One primary ground pour on bottom.
- If pour is segmented by traces, stitch segments with dedicated wire jumpers (labeled GND_JMP1, GND_JMP2, ...).

## 3.7 SD Card Reader Board (SPI) Pinout

Use a through-hole 6-pin socket (female) for direct SD module plug-in, no flying jumpers.

ESP32-S3 to SD net mapping:
- GPIO36 -> SD_SCK
- GPIO37 -> SD_MISO
- GPIO35 -> SD_MOSI
- GPIO38 -> SD_CS
- GPIO39 -> SD_CD (card detect, optional input)

Recommended SD socket pinout (H_SD1, 1x06):
- Pin 1: +3V3_LOGIC (to SD VCC)
- Pin 2: GND
- Pin 3: SD_SCK
- Pin 4: SD_MISO
- Pin 5: SD_MOSI
- Pin 6: SD_CS

Card detect wiring:
- Route SD_CD to a dedicated test pad or optional 1-pin header (for modules exposing CD switch).

Wire-up to common SD module labels:
- SD_SCK -> CLK
- SD_MISO -> DO / MISO
- SD_MOSI -> DI / MOSI
- SD_CS -> CS / SS

Important:
- Use 3.3V SD modules for direct ESP32-S3 logic compatibility.
- Keep SD SPI traces short and away from motor phase traces.

---

## 4) Footprints and Package Choices

Use common KiCad library footprints where possible to avoid custom risk.

Prototype policy for this revision:
- Through-hole parts only.
- No SMD passives in this build.

## 4.1 Recommended Footprint Map

| Ref Type | Suggested Footprint |
|:--|:--|
| ESP32-S3 DevKitC-1 socket headers | PinSocket_1x19_P2.54mm_Vertical x2 (adjust pin count to module) |
| TMC2209 carrier sockets | PinSocket_2x08_P2.54mm_Vertical per driver (through-hole female socket) |
| Servo connectors | JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical |
| Motor outputs (6-pin, 4 active) | JST_XH_B6B-XH-A_1x06_P2.50mm_Vertical (or same-series equivalent matching your harness pitch) |
| Power terminals (J_PWR1..J_PWR5) | TerminalBlock_1x02_P5.08mm |
| UART link | PinHeader_1x04_P2.54mm_Vertical |
| SD module direct socket | PinSocket_1x06_P2.54mm_Vertical |
| Test points | TestPoint_Pad_D1.5mm |
| Mount holes | MountingHole_3.2mm_M3 |
| Through-hole resistors | R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal |
| Through-hole electrolytics | CP_Radial_D8.0mm_P3.50mm, CP_Radial_D10.0mm_P5.00mm (based on value/voltage) |
| Through-hole film/ceramic caps | C_Disc_D5.0mm_W2.5mm_P5.00mm or C_Rect_L7.2mm_W2.5mm_P5.00mm |

If your exact connector stock differs, keep net names and pin order unchanged and only swap footprint package.

## 4.1.1 Devboard Header Footprints (Explicit)

All controller/driver modules are treated as pluggable devboards and must connect through 2.54 mm through-hole headers.

ESP32-S3 devboard (on main PCB):
- PCB footprint: PinSocket_1x19_P2.54mm_Vertical x2 (female socket on PCB).
- Devboard mating part: PinHeader_1x19_P2.54mm_Vertical x2 (male pins soldered on ESP32 devboard).

TMC2209 devboards (each driver on main PCB):
- PCB footprint: PinSocket_2x08_P2.54mm_Vertical per driver.
- Devboard mating part: PinHeader_2x08_P2.54mm_Vertical on each TMC board.

Alternative if you want lower stack height:
- Use PinHeader_* on PCB and PinSocket_* on module side, but keep 2.54 mm pitch and same pin count.

Mechanical/orientation notes:
- Add clear silkscreen marker for pin 1 on every devboard socket row.
- Keep at least 2.0 mm courtyard clearance around sockets for insertion/removal.
- Lock one orientation only (no mirrored socket rows).
- Place TMC sockets so EN/STEP/DIR/PDN side is consistent across J1..J4.

## 4.2 Discrete Passives BOM (Through-Hole)

These are the minimum recommended passives for stable first-spin bring-up.

| RefDes | Qty | Value | Type | Voltage/Power | Net Placement |
|:--|:--:|:--|:--|:--|:--|
| C1 | 1 | 470 uF | Electrolytic radial | 35V min | +24V_MOTOR to GND at power entry |
| C2 | 1 | 1 uF | Film/ceramic THT | 50V min | +24V_MOTOR to GND near TMC bank |
| C3 | 1 | 100 nF | Ceramic disc THT | 50V min | +24V_MOTOR to GND near TMC bank |
| C4 | 1 | 1000 uF | Electrolytic radial | 16V min | +6V6_SERVO to GND at servo power fan-out |
| C5 | 1 | 100 nF | Ceramic disc THT | 25V min | +6V6_SERVO to GND near servo connector cluster |
| C6 | 1 | 220 uF | Electrolytic radial | 10V min | +3V3_LOGIC to GND near ESP32 socket |
| C7-C10 | 4 | 100 nF | Ceramic disc THT | 25V min | Each TMC VIO pin to nearest GND |
| C11 | 1 | 100 nF | Ceramic disc THT | 25V min | +3V3_LOGIC to GND near UART header |
| C12 | 1 | 10 uF | Electrolytic radial | 10V min | +3V3_LOGIC to GND near SD header |
| C13 | 1 | 100 nF | Ceramic disc THT | 25V min | +3V3_LOGIC to GND near SD header |
| R1-R4 | 4 | 4.7 k | Axial resistor | 1/4W | PDN_J1..PDN_J4 pull-up to +3V3_LOGIC |
| R5-R8 | 4 | 0R link | Axial resistor | 1/4W | Series on PDN_J1..PDN_J4 (optional debug isolation) |
| R9-R11 | 3 | 100 k | Axial resistor | 1/4W | Servo signal pulldown to GND (one per SERVO_* net) |
| R12-R13 | 2 | 220 ohm | Axial resistor | 1/4W | Series on UART_LEADER_RX and UART_LEADER_TX (optional EMI damping) |
| R14-R17 | 4 | 33 ohm | Axial resistor | 1/4W | Series on SD_SCK/SD_MISO/SD_MOSI/SD_CS (optional signal damping) |

Notes:
- R5-R8 can be DNI if you prefer direct PDN routing.
- R12-R13 can be DNI if UART link is short and clean.
- Keep all capacitor leads short, especially C7-C10 around TMC VIO pins.

## 4.3 Discrete Passives Wiring Guide

Follow this order for clean bring-up.

1. 24V input filtering:
- Wire C1 directly across +24V_MOTOR and GND at the board input terminal.
- Place C2 and C3 in parallel across +24V_MOTOR and GND near the first/center TMC driver socket.

2. 6.6V servo rail stabilization:
- Wire C4 across +6V6_SERVO and GND at the servo rail branch point.
- Add C5 near the servo connector cluster between +6V6_SERVO and GND.

3. 3.3V logic rail stabilization:
- Place C6 across +3V3_LOGIC and GND near ESP32 power entry pins.
- Place C11 across +3V3_LOGIC and GND near UART header/logic edge.
- Place C12 and C13 across +3V3_LOGIC and GND near the SD header.

4. Per-driver logic decoupling:
- Place C7, C8, C9, C10 each from TMC VIO to nearest GND on J1..J4 respectively.

5. PDN conditioning:
- Route PDN_J1..PDN_J4 from ESP32 to corresponding drivers.
- Add R1-R4 as pull-ups from each PDN_Jx net to +3V3_LOGIC.
- Insert R5-R8 in series on each PDN_Jx trace (ESP32 side -> resistor -> driver side).

6. Servo signal default-safe state:
- Add R9-R11 from SERVO_WRIST_PITCH, SERVO_WRIST_YAW, SERVO_GRIPPER to GND.

7. UART damping:
- Place R12 in series on UART_LEADER_RX and R13 in series on UART_LEADER_TX close to ESP32 header side.

8. SD SPI conditioning:
- Route SD_SCK, SD_MISO, SD_MOSI, SD_CS from ESP32 pins to H_SD1.
- Optionally place R14-R17 in series on each SD line near the ESP32 side.

## 4.4 Optional Custom Footprint Code (KiCad .kicad_mod Template)

Use this only when you need a custom jumper pad footprint for GND stitching wires.

```scheme
(footprint "WireJumper_1x02_P2.54mm" (version 20221018) (generator "manual")
  (layer "F.Cu")
  (descr "2-pad wire jumper footprint for single-layer bridges")
  (attr through_hole)
  (fp_text reference "JP?" (at 0 -2.2) (layer "F.SilkS")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  (fp_text value "WIRE_JUMPER" (at 0 2.2) (layer "F.Fab")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  (fp_line (start -2 -1.3) (end 2 -1.3) (layer "F.SilkS") (width 0.12))
  (fp_line (start -2 1.3) (end 2 1.3) (layer "F.SilkS") (width 0.12))
  (pad "1" thru_hole circle (at -1.27 0) (size 1.8 1.8) (drill 0.9)
    (layers "*.Cu" "*.Mask")
  )
  (pad "2" thru_hole circle (at 1.27 0) (size 1.8 1.8) (drill 0.9)
    (layers "*.Cu" "*.Mask")
  )
)
```

Place these as:
- Signal jumpers for unavoidable single-layer crossings.
- Ground jumpers between disconnected ground-pour islands.

## 4.5 Schematic Symbols (What To Use)

Use hierarchical, connector-style symbols for devboards so pin names match firmware nets directly.

Recommended symbol naming in your schematic library:
- U1: ESP32_S3_DEVBOARD_2x19
- U2-U5: TMC2209_DEVBOARD_2x08
- J1_MOTOR..J4_MOTOR: JST6_STEPPER_4USED
- J_SERVO1..J_SERVO3: JST3_SERVO
- H_UART1: HDR_1x04_UART
- H_SD1: HDR_1x06_SD_SPI

### 4.5.1 ESP32-S3 Devboard Symbol (U1)

Create one multi-pin symbol named ESP32_S3_DEVBOARD_2x19 with at least the used pins below exposed and labeled exactly:

| Symbol Pin Name | MCU GPIO | Net Label to Wire |
|:--|:--|:--|
| GPIO14 | 14 | EN_ALL |
| GPIO13 | 13 | PDN_J1 |
| GPIO12 | 12 | STEP_J1 |
| GPIO11 | 11 | DIR_J1 |
| GPIO10 | 10 | PDN_J2 |
| GPIO9 | 9 | STEP_J2 |
| GPIO8 | 8 | DIR_J2 |
| GPIO18 | 18 | UART_LEADER_RX |
| GPIO17 | 17 | UART_LEADER_TX |
| GPIO16 | 16 | PDN_J3 |
| GPIO15 | 15 | STEP_J3 |
| GPIO7 | 7 | DIR_J3 |
| GPIO6 | 6 | PDN_J4 |
| GPIO5 | 5 | STEP_J4 |
| GPIO4 | 4 | DIR_J4 |
| GPIO35 | 35 | SD_MOSI |
| GPIO36 | 36 | SD_SCK |
| GPIO37 | 37 | SD_MISO |
| GPIO38 | 38 | SD_CS |
| GPIO39 | 39 | SD_CD |
| GPIO40 | 40 | SERVO_WRIST_PITCH |
| GPIO41 | 41 | SERVO_WRIST_YAW |
| GPIO42 | 42 | SERVO_GRIPPER |
| 3V3 | - | +3V3_LOGIC |
| GND | - | GND |

Symbol rules:
- Mark GPIO pins as bidirectional/passive (project preference), power pins as power input.
- Keep pin names as GPIOxx text, not custom aliases.

### 4.5.2 TMC2209 Devboard Symbol (U2-U5)

Create symbol TMC2209_DEVBOARD_2x08 with these functional pins:

| Symbol Pin Name | Net (Per Channel x) |
|:--|:--|
| STEP | STEP_Jx |
| DIR | DIR_Jx |
| EN | EN_ALL |
| PDN_UART | PDN_Jx |
| VIO | +3V3_LOGIC |
| VMOT | +24V_MOTOR |
| GND | GND |
| A1 | Jx_MOTOR_PIN3 (1A) |
| A2 | Jx_MOTOR_PIN2 (2A) |
| B1 | Jx_MOTOR_PIN1 (1B) |
| B2 | Jx_MOTOR_PIN4 (2B) |

Instantiate four copies:
- U2 (J1), U3 (J2), U4 (J3), U5 (J4)

### 4.5.3 JST-6 Stepper Connector Symbol (Jx_MOTOR)

Create symbol JST6_STEPPER_4USED (single-row 6-pin connector):

| Connector Pin | Net |
|:--|:--|
| 1 | Jx_MOTOR_PIN1 / A+ |
| 2 | NC (center tap open) |
| 3 | Jx_MOTOR_PIN2 / A- |
| 4 | Jx_MOTOR_PIN3 / B+ |
| 5 | NC (center tap open) |
| 6 | Jx_MOTOR_PIN4 / B- |

In KiCad, set pins 5 and 6 as passive and mark explicitly as NC with no-connect flags.

### 4.5.4 JST-3 Servo Connector Symbol (J_SERVOx)

Create symbol JST3_SERVO:

| Connector Pin | Net |
|:--|:--|
| 1 | GND |
| 2 | +6V6_SERVO |
| 3 | SERVO_* (per channel) |

Use three instances with signal nets:
- J_SERVO1 -> SERVO_WRIST_PITCH
- J_SERVO2 -> SERVO_WRIST_YAW
- J_SERVO3 -> SERVO_GRIPPER

### 4.5.5 UART Header Symbol (H_UART1)

Create symbol HDR_1x04_UART:

| Header Pin | Net |
|:--|:--|
| 1 | GND |
| 2 | UART_LEADER_RX |
| 3 | UART_LEADER_TX |
| 4 | +3V3_LOGIC (optional) |

### 4.5.6 SD Header Symbol (H_SD1)

Create symbol HDR_1x06_SD_SPI (mated to PinSocket_1x06_P2.54mm_Vertical on PCB):

| Header Pin | Net |
|:--|:--|
| 1 | +3V3_LOGIC |
| 2 | GND |
| 3 | SD_SCK |
| 4 | SD_MISO |
| 5 | SD_MOSI |
| 6 | SD_CS |

Add `SD_CD` as a separate 1-pin symbol/testpoint (for card detect input).

### 4.5.7 Power Symbols and Net Labels

Use global power symbols and labels:
- +24V_MOTOR
- +6V6_SERVO
- +3V3_LOGIC
- GND

For readability, place net labels on every cross-sheet entry even if wires are continuous.

---

## 5) Trace Width, Clearance, and Via Rules

These are safe prototype defaults for 1 oz copper, 2-layer-capable fab, but routed as single-layer where possible.

## 5.1 Net Class Rules (Recommended)

| Net Class | Width | Clearance | Via Drill / Dia | Applies To |
|:--|:--|:--|:--|:--|
| SIGNAL_FINE | 0.25 mm (10 mil) | 0.20 mm (8 mil) | 0.30 / 0.60 mm | STEP/DIR/PDN/UART/servo PWM |
| SIGNAL_STD | 0.30 mm (12 mil) | 0.20 mm (8 mil) | 0.30 / 0.60 mm | Generic logic |
| PWR_3V3_6V6 | 0.60 mm (24 mil) | 0.25 mm (10 mil) | 0.40 / 0.80 mm | +3V3_LOGIC, +6V6_SERVO trunks |
| PWR_24V | 1.00 mm (40 mil) | 0.30 mm (12 mil) | 0.40 / 0.80 mm | +24V_MOTOR trunk |
| GND_RETURN | 1.20 mm (47 mil) where traced, otherwise pour | 0.25 mm (10 mil) | 0.40 / 0.80 mm | Ground returns and links |

## 5.2 High-Current Guidance

- Keep +24V_MOTOR and motor return paths short and wide.
- Use local bulk capacitance at motor power entry (start with 220 uF to 470 uF electrolytic + 1 uF + 100 nF ceramics nearby).
- Keep servo rail trunk wide (at least 0.8 mm if space allows) from regulator to servo connector fan-out.

## 5.3 Single-Layer + Jumper Strategy

- Route power trunks first.
- Route STEP/DIR/PDN next as short direct lines.
- Drop labeled wire-jumper footprints where crossing is unavoidable.
- After autorouter/manual route, inspect ground pour islands; bridge each orphan island with explicit GND jumper wire.

---

## 6) Spacing and Placement Rules (Practical)

- Keep at least 3 mm creepage between +24V_MOTOR and low-voltage signal zones where feasible.
- Place TMC2209 modules in one row with consistent orientation.
- Keep ESP32 module away from motor phase traces and buck inductors.
- Keep UART and PDN traces away from motor outputs; cross at 90 degrees when unavoidable.
- Place test points near the ESP32 edge for probe access.

---

## 7) ERC/DRC Checklist Before Fabrication

Schematic/ERC:
- EN_ALL only goes to all four TMC EN pins.
- PDN_J1..PDN_J4 are unique and not tied together.
- UART_LEADER_RX/TX are correctly crossed to leader board.
- All servo connectors have GND and +6V6_SERVO in correct pin order.
- All stepper JST-6 connectors map active phases to pins 1/3/4/6 with pins 2 and 5 left NC.
- SD header maps exactly: 36/37/35/38 to SD_SCK/SD_MISO/SD_MOSI/SD_CS, and SD_CD on GPIO39.
- All listed passives are through-hole package footprints (no SMD passives).

PCB/DRC:
- No unrouted nets.
- No isolated copper islands left unconnected unless intentional.
- Ground pour continuity verified end-to-end with continuity tool.
- All jumper wires represented in schematic as net ties or jumper refs.

Bring-up:
- Power-on with no motors first, check +3V3_LOGIC and +6V6_SERVO.
- Verify EN_ALL toggles and each PDN_Jx can be driven independently.
- Verify UART link before motion test.

---

## 8) NodeMesh Notes for This Follower Board

For this follower motor board revision:
- Keep the board focused on motion/servo/UART.
- SD SPI header is optional for NodeMesh expansion and should use GPIO36/37/35/38 mapping above, with optional SD_CD on GPIO39.

This keeps the follower board modular and consistent with NodeMesh partitioning.

---

## 9) Quick Copy Block for CAD Project Notes

Pin map summary:
- STEP: GPIO12,9,15,5
- DIR: GPIO11,8,7,4
- EN_ALL: GPIO14
- PDN: GPIO13,10,16,6
- Servo PWM: GPIO40,41,42
- UART: RX GPIO18, TX GPIO17
- SD SPI: SCK GPIO36, MISO GPIO37, MOSI GPIO35, CS GPIO38, CD GPIO39

Prototype constraints:
- Single-layer priority.
- Use labeled jumper wires for crossings and GND pour stitching.
- Use wide power traces and keep motor-current loops compact.
- Use through-hole components and JST XH-style connectors for this prototype.
