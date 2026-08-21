# Universal Joint PCB — Design Reference
**Project:** 6-DOF Robotic Arm — Modular Backplane Controller
**Board:** ~42×42mm, 4-layer, one design deployed to all joint nodes
**Revision:** v1.1

---

## 1. Architecture

| Node | Populated | Role |
|---|---|---|
| Base | Stepper 1 driver, USB-C, CAN master | Bus master, host bridge |
| Shoulder L | Stepper 2A driver | CAN node |
| Shoulder R | Stepper 2B driver | CAN node, inverse-direction logic |
| Elbow | Stepper 3 driver, 3× servo headers (open-loop) | CAN node, closest hop to wrist/gripper |
| Spare | — | Validated, unpowered spare (JLC 5-board MOQ) |

Servos are **open-loop** — PWM out only, no position feedback wired back to the MCU.

## 2. Power Architecture

```
24V IN → TVS (transient/reverse protection)
        → TMC2209 VMOTOR (direct)
        → TPS54560B buck + catch diode → 6.0–6.4V rail → AMS1117-3.3 LDO → 3.3V logic
```

**Buck IC note:** TPS54560B is non-synchronous — requires the external catch diode (DBK1 below), it's not optional. Guaranteed minimum current limit is 6.3A across full temp/Vin range (typ 7.9A) — below the original 7A peak target in worst-case corner conditions. Treat 6A as the reliable peak servo draw for this stage; if a real 7A+ peak is required, split servos across two regulators instead of pushing this IC past its guaranteed minimum.

## 3. ESP32-C6-MINI-1 Pin Map

| Function | Signal | GPIO |
|---|---|---|
| TMC2209 | STEP / DIR / UART | 0 / 1 / 2 |
| SPI Encoder | SCK / MISO / MOSI / CS | 18 / 19 / 20 / 21 |
| CAN (TWAI) | TX / RX | 4 / 5 ⚠ strapping pins — verify against TRM boot-config table before layout |
| Servo PWM (open-loop) | Ch1 / Ch2 / Ch3 | 14 / 22 / 23 |
| USB Serial/JTAG | D− / D+ | 12 / 13 |
| Reserved (strapping/flash) | — | 4, 5, 8, 9, 15, 24–30 |

No ADC feedback pins — removed with closed-loop servo path.

## 4. USB-C (data-only, every board)
D− → GPIO12, D+ → GPIO13 (was reversed in earlier draft — fixed) · CC1/CC2 → 5.1kΩ pulldown · VBUS unconnected · ESD array across D+/D− · board always powered from 24V terminal, not USB.

## 5. CAN Bus
SN65HVD230/VP230, 1 Mbps, linear daisy chain Base→Shoulder L→Shoulder R→Elbow. 120Ω termination populated at Base and Elbow only (DNP on Shoulder L/R).

## 6. Interboard Connector
XT30(2+2)-M, horizontal, PCB-mount, on both JPW1 (in) and JPW2 (out). Power pins carry 24V/GND, signal pins carry CAN_H/CAN_L — keyed shell prevents reversed power or swapped CAN pair. No positive latch (friction-fit bullet contacts) — add a strain-relief anchor point near each connector since Shoulder/Elbow joints see continuous motion, unlike a static mount. Mating cables are hand-crimped, 4-wire (2 power gauge + 2 signal gauge).

---

## 7. Component List (BOM)

Reference designator format: **[Type][Subcircuit][#]**
Tags — `MC`=MCU support, `BK`=buck reg, `LD`=LDO, `CN`=CAN, `EC`=encoder, `DR`=stepper driver/motor power, `SV`=servo (open-loop), `US`=USB-C, `PW`=power/CAN connectors + input protection

| RefDes | Component | Value / Part | Notes |
|---|---|---|---|
| UMC1 | ESP32-C6-MINI-1 module | — | Core MCU |
| CMC1, CMC2 | Decoupling caps | 100nF, 10µF | Near module VDD |
| RMC1 | EN pull-up | 10kΩ | To 3.3V |
| CMC3 | EN delay cap | 100nF | To GND |
| RMC2 | GPIO9 (boot) pull-up | 10kΩ | To 3.3V |
| SWMC1 | Reset button | — | EN to GND, momentary |
| SWMC2 | Boot button | — | GPIO9 to GND, momentary |
| DPW1 | TVS diode | SMBJ33A, 33V standoff, unidirectional | Across 24V/GND at power input |
| JPW1 | Power/CAN in | XT30(2+2)-M, horizontal PCB mount | Power pins: 24V/GND · Signal pins: CAN_H/CAN_L |
| JPW2 | Power/CAN out (pass-through) | XT30(2+2)-M, horizontal PCB mount | Base has JPW1 only (no upstream) |
| UBK1 | Buck regulator | TPS54560B (DDA, reel) | 24V→6V, non-synchronous |
| DBK1 | Catch diode | 60V Schottky (e.g. B560C) | Required — SW to GND |
| CBK1, CBK2 | Input caps | 10µF, 50V X7R | At VIN |
| LBK1 | Inductor | ~7–8µH, ≥7A sat | Per TI reference design |
| CBK3, CBK4 | Output caps | 22µF, 25V X7R ×2 | Buffer servo bursts |
| RBK1, RBK2 | FB divider | per 6.0–6.4V target | To FB pin |
| RBK3 | RT/CLK resistor | sets ~400–500kHz | Switching freq |
| CBK5 | Boot cap | 100nF | BOOT to SW |
| RBK4, CBK6, CBK7 | Compensation network | Type II | COMP pin |
| RBK5, RBK6 | EN/UVLO divider | — | Optional, sets turn-on threshold |
| ULD1 | LDO | AMS1117-3.3 | 6V→3.3V |
| CLD1 | Input cap | 10µF | |
| CLD2 | Output cap | 22µF | |
| UCN1 | CAN transceiver | SN65HVD230/VP230 | |
| CCN1 | Bypass cap | 100nF | VCC pin |
| RCN1 | Rs (slope) | 0Ω or per datasheet | |
| RCN2 | Termination | 120Ω | Base/Elbow only, DNP elsewhere |
| UEC1 | Magnetic encoder | MT6835 / AS5048A | SPI, motor position only |
| CEC1 | Decoupling cap | 100nF | |
| JDR1 | TMC2209 socket | 2×5 female header | Motor driver module |
| CDR1 | VM bulk cap | 100µF electrolytic | Local motor current buffer |
| JSV1–JSV3 | Servo headers | 3-pin (power, gnd, PWM) | Elbow only; DNP elsewhere |
| JUS1 | USB-C receptacle | — | Data-only |
| RUS1, RUS2 | CC pulldowns | 5.1kΩ | CC1/CC2 to GND |
| DUS1 | ESD protection array | USBLC6-2SC6 | D+/D− |
| RUS3, RUS4 | Series resistors | 22–33Ω | Optional, D+/D− |

---

## 8. Net List

| Net | Connects |
|---|---|
| VIN_24V | JPW1/JPW2, DPW1, UBK1 (VIN), JDR1 (VM) |
| GND | All grounds, common plane |
| V6V | UBK1 (VOUT), DBK1, CBK3/4, JSV1-3 (servo power), ULD1 (VIN) |
| V3V3 | ULD1 (VOUT), UMC1, UCN1, UEC1, RMC1/RMC2 pull-ups |
| SW_BK | UBK1 (SW), DBK1, LBK1, CBK5 |
| FB_BK | UBK1 (FB), RBK1, RBK2 |
| COMP_BK | UBK1 (COMP), RBK4, CBK6, CBK7 |
| CAN_H, CAN_L | UCN1 (bus side), JPW1, JPW2, RCN2 |
| CAN_TX, CAN_RX | UMC1 GPIO4/5, UCN1 |
| SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS | UMC1 GPIO18–21, UEC1 |
| STEP, DIR, UART_CFG | UMC1 GPIO0/1/2, JDR1 |
| PWM1, PWM2, PWM3 | UMC1 GPIO14/22/23, JSV1–3 |
| USB_DM, USB_DP | UMC1 GPIO12/13, JUS1, DUS1, RUS3/4 |
| CC1, CC2 | JUS1, RUS1, RUS2 |
| EN_MCU | UMC1 EN, RMC1, CMC3, SWMC1 |
| BOOT_MCU | UMC1 GPIO9, RMC2, SWMC2 |
