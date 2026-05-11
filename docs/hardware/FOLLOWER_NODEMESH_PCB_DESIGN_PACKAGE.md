# SixEyes Follower PCB Design Package (NodeMesh-Aware)

This document is a practical, copy-into-CAD checklist for the first follower PCB prototype.

Design intent for this revision:
- Single-layer routing for prototype bring-up.
- Use jumper wires for unavoidable crossovers and to stitch ground islands/pours.
- Keep compatibility with current follower firmware and NodeMesh direction.

---

## 1) Locked Pinout (Firmware-Verified)

Authoritative mapping is based on current follower firmware and NodeMesh pin docs.

### Stepper Drivers (TMC2209 x4)

| Channel | Joint | STEP | DIR | EN | PDN_UART |
|:--|:--|:--|:--|:--|:--|
| J1 | Base | GPIO4 | GPIO5 | GPIO6 (shared EN_ALL) | GPIO7 |
| J2 | Shoulder A | GPIO8 | GPIO9 | GPIO6 (shared EN_ALL) | GPIO11 |
| J3 | Shoulder B | GPIO12 | GPIO13 | GPIO6 (shared EN_ALL) | GPIO15 |
| J4 | Elbow | GPIO16 | GPIO17 | GPIO6 (shared EN_ALL) | GPIO21 |

### Servo PWM Outputs

| Function | GPIO | Net Name |
|:--|:--|:--|
| Wrist Pitch | GPIO35 | SERVO_WRIST_PITCH |
| Wrist Yaw | GPIO36 | SERVO_WRIST_YAW |
| Gripper | GPIO37 | SERVO_GRIPPER |

### Inter-Board UART (Leader <-> Follower)

| Signal | Follower GPIO | Net Name |
|:--|:--|:--|
| RX (from leader TX) | GPIO38 | UART_LEADER_RX |
| TX (to leader RX) | GPIO39 | UART_LEADER_TX |

### Power Nets

| Net | Notes |
|:--|:--|
| +24V_MOTOR | TMC VMOT rail input |
| +6V6_SERVO | Servo rail from buck converter |
| +3V3_LOGIC | ESP32-S3 logic, TMC VIO, pull-ups |
| GND | Common ground |

### Reserved/Free GPIO in this revision

GPIO10, GPIO14, GPIO18

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
| 4 | STEP_J1 |
| 5 | DIR_J1 |
| 6 | EN_ALL |
| 7 | PDN_J1 |
| 8 | STEP_J2 |
| 9 | DIR_J2 |
| 11 | PDN_J2 |
| 12 | STEP_J3 |
| 13 | DIR_J3 |
| 15 | PDN_J3 |
| 16 | STEP_J4 |
| 17 | DIR_J4 |
| 21 | PDN_J4 |
| 35 | SERVO_WRIST_PITCH |
| 36 | SERVO_WRIST_YAW |
| 37 | SERVO_GRIPPER |
| 38 | UART_LEADER_RX |
| 39 | UART_LEADER_TX |

Power/header pins:
- 3V3 -> +3V3_LOGIC
- GND -> GND

## 3.2 TMC2209 Drivers (x4)

Per driver block Jx:
- STEP pin -> STEP_Jx
- DIR pin -> DIR_Jx
- EN pin -> EN_ALL
- PDN_UART pin -> PDN_Jx
- VIO -> +3V3_LOGIC
- VMOT -> +24V_MOTOR
- GND pins -> GND
- Motor outputs OA1/OA2/OB1/OB2 -> matching motor connector Jx_MOTOR

Recommended support passives per driver:
- 100 nF ceramic decoupler at VIO-to-GND.
- Bulk capacitor near VMOT entry area (see power section).
- PDN line: keep short and include optional 0R series link footprint for debug isolation.

## 3.3 Servo Connectors (3-pin each)

For each servo connector:
- Pin 1: GND
- Pin 2: +6V6_SERVO
- Pin 3: Signal

Signal map:
- Servo 1 signal -> SERVO_WRIST_PITCH
- Servo 2 signal -> SERVO_WRIST_YAW
- Servo 3 signal -> SERVO_GRIPPER

## 3.4 Leader/Follower UART Connector

4-pin recommended header:
- Pin 1: GND
- Pin 2: UART_LEADER_RX (leader TX)
- Pin 3: UART_LEADER_TX (leader RX)
- Pin 4: +3V3_LOGIC (optional, only if power-sharing is required)

## 3.5 Power Input and Rails

Power entry terminal (2-pin minimum):
- VIN_24V -> +24V_MOTOR
- GND -> GND

Buck/regulator outputs:
- 24V -> 6.6V converter output -> +6V6_SERVO
- 24V -> 3.3V converter/LDO output -> +3V3_LOGIC

Ground strategy for single-layer prototype:
- One primary ground pour on bottom.
- If pour is segmented by traces, stitch segments with dedicated wire jumpers (labeled GND_JMP1, GND_JMP2, ...).

---

## 4) Footprints and Package Choices

Use common KiCad library footprints where possible to avoid custom risk.

## 4.1 Recommended Footprint Map

| Ref Type | Suggested Footprint |
|:--|:--|
| ESP32-S3 DevKitC-1 socket headers | PinSocket_1x19_P2.54mm_Vertical x2 (adjust pin count to module) |
| TMC2209 carrier sockets | PinSocket_1x08_P2.54mm_Vertical x2 per driver (for stepstick-style modules) |
| Servo connectors | JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical |
| Motor outputs | JST_VH_B4B-VH_1x04_P3.96mm_Vertical or screw terminal 4-pin |
| Main power in | TerminalBlock_1x02_P5.08mm |
| UART link | PinHeader_1x04_P2.54mm_Vertical |
| Test points | TestPoint_Pad_D1.5mm |
| Mount holes | MountingHole_3.2mm_M3 |

If your exact connector stock differs, keep net names and pin order unchanged and only swap footprint package.

## 4.2 Optional Custom Footprint Code (KiCad .kicad_mod Template)

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
- Do not route Node0 SD SPI (GPIO40/41/42/2) unless this board is intentionally merged with Node0 orchestrator roles.

This keeps the follower board modular and consistent with NodeMesh partitioning.

---

## 9) Quick Copy Block for CAD Project Notes

Pin map summary:
- STEP: GPIO4,8,12,16
- DIR: GPIO5,9,13,17
- EN_ALL: GPIO6
- PDN: GPIO7,11,15,21
- Servo PWM: GPIO35,36,37
- UART: RX GPIO38, TX GPIO39

Prototype constraints:
- Single-layer priority.
- Use labeled jumper wires for crossings and GND pour stitching.
- Use wide power traces and keep motor-current loops compact.
