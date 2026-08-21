# Beta Hardware — Wiring & Assembly Guide

PCB: SixEyes Follower Beta Rev2, 60×60 mm 4-layer custom  
Module: ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB Octal PSRAM)  
Source: `docs/references/SixEyes_Node0.pdf`

**Key difference from Alpha**: Beta integrates the MCU, buck converters, and TMC2209 driver ICs directly on a 4-layer PCB. There is no external devkit or breakout board.

---

## Hardware Topology

```
24V PSU (J_PWR, XT30)
  ├── 4× TMC2209 VM direct (fused)
  ├── TPS54540B buck → 6.6V → 3× MG996R servos (J_SV1–3)
  └── LMR14030S buck → 5.0V → Node1/2/3 headers
                              └── MIC5219-3.3 LDO → ESP32-S3 + TMC VIO

Laptop
  └── J_USB1 (Native USB-OTG) → ROS2 bridge / telemetry
  └── J_USB2 (CH340K) → UART debug / PlatformIO flash

Node1 ESP32-C6 (offboard leader board)
  └── J_LDR (UART 921600 baud + 5V power)

Node2/Node3 ESP32-CAM (wireless)
  └── J_CAM1/J_CAM2 (5V power only; data via ESP-NOW ch.6)
```

---

## Parts List

### PCB & Module
- **Beta Follower PCB Rev2** — 60×60 mm 4-layer (JLCPCB JLC4L or equivalent)
- **ESP32-S3-WROOM-1-N16R8** — 16 MB Flash, 8 MB Octal PSRAM (required variant)
- **4× TMC2209-LA** — QFN-28, 5×5 mm (direct IC, not module)
- **TPS54540B** — 5.5A sync buck, HSOP-8-EP (6.6V servo regulator)
- **LMR14030S** — 3A async buck, HSOP-8-EP (5.0V peripheral regulator)
- **MIC5219-3.3YM5** — 1A LDO, SOT-23-5 (3.3V logic regulator)
- **CH340K** — UART bridge, ESSOP-10 (debug/flash USB)

### Actuators (unchanged from Alpha)
- **4× NEMA23 bipolar steppers** — 2.8A nominal, 4-wire
- **3× MG996R or MG995 digital servos** — 6.6V rail

### Power
- **24V DC supply** — ≥6A (XT30 connector)

### Connectors (per connector directory)
- XT30 right-angle (J_PWR)
- 2× USB-C receptacle (J_USB1, J_USB2)
- 4× Molex Micro-Fit 3.0 4-pin (J_M1–M4, stepper phases)
- 3× 2.54 mm 3-pin header (J_SV1–SV3, servos)
- 2× Molex Micro-Fit 3.0 2-pin (J_CAM1, J_CAM2)
- 1× 2.54 mm 4-pin header (J_LDR, leader link)
- MicroSD push-pull slot (J_SD)

---

## Power Architecture

| Rail | Regulator | Voltage | Load |
|------|-----------|---------|------|
| Motor | Direct 24V (fused) | 24V | 4× TMC2209 VM |
| Servo | TPS54540B (Rtop=102kΩ, Rbot=14kΩ) | 6.6V | 3× servos, 5A cont / 7A stall |
| Peripheral | LMR14030S | 5.0V | Node1 (250mA), Node2/3 (400mA ea.), LDO input |
| Logic | MIC5219-3.3 (1A) | 3.3V | ESP32-S3 (500mA), 4× TMC VIO (60mA) |

Worst-case 5V load: 1670 mA peak (LMR14030S 3A → 1.3× headroom).  
3.3V LDO dissipation: (5.0 − 3.3) × 0.58 A ≈ 1.0W — ensure copper spread under SOT-23-5 pad.

**Capacitor rule**: Use low-ESR SMD electrolytic only for 24V bulk caps. Do NOT substitute tantalum — motor back-EMF causes catastrophic tantalum failure.

---

## Stepper Motor Wiring

Motor phases connect to J_M1–J_M4 (Molex Micro-Fit 3.0, 4-pin):

| Joint | Connector | Motor Phases |
|-------|-----------|-------------|
| Base | J_M1 | A1/A2/B1/B2 |
| Shoulder A | J_M2 | A1/A2/B1/B2 |
| Shoulder B | J_M3 | A1/A2/B1/B2 |
| Elbow | J_M4 | A1/A2/B1/B2 |

TMC2209 is now a direct QFN-28 IC on the PCB. No VREF potentiometer — current is set via UART register (IHOLD_IRUN).

DIAG pins are now routed (GPIO1/2/3/48) enabling StallGuard2 sensorless homing and CoolStep adaptive current.

---

## Servo Wiring

Servos connect to J_SV1–J_SV3 (2.54 mm 3-pin, South edge, vertical orientation):

| Servo | Connector | GPIO | Joint |
|-------|-----------|------|-------|
| Wrist Pitch | J_SV1 | GPIO38 | Wrist pitch |
| Wrist Yaw | J_SV2 | GPIO39 | Wrist yaw |
| Gripper | J_SV3 | GPIO40 | Gripper |

Power pin: 6.6V from TPS54540B buck. Do NOT connect servo power to 5V or 3.3V rails.

---

## Leader Link (Node1)

Node1 = ESP32-C6 SuperMini (offboard). Connect to J_LDR (South edge, 2.54 mm 4-pin):

| Pin | Signal | Notes |
|-----|--------|-------|
| 1 | TX → Node1 RX | 921600 baud, 8N1 |
| 2 | RX ← Node1 TX | 921600 baud, 8N1 |
| 3 | GND | Common ground |
| 4 | 5V out | Powers Node1 from Beta PCB 5V rail |

---

## USB Connections

| Port | Connector | Use |
|------|-----------|-----|
| J_USB1 | USB-C (West edge) | ROS2 bridge, telemetry, VLA dataset logging |
| J_USB2 | USB-C (West edge) | CH340K debug monitor, PlatformIO flash (DTR/RTS auto-reset) |

J_USB2 eliminates the need for an external USB-to-serial programmer.

---

## Boot Strapping Requirements

| GPIO | Net | Requirement |
|------|-----|-------------|
| GPIO0 | MCU_BOOT | 10 kΩ pull-up to 3.3V; tactile switch to GND for manual boot-mode |
| GPIO46 | MCU_STRAP_LOW | 10 kΩ pull-down to GND (required by ESP32-S3 strapping spec) |
| GPIO45 | — | Leave floating (internal OTP default) |

---

## PCB Edge Zone Summary

| Edge | Connectors |
|------|-----------|
| North | RF keep-out zone for ESP32-S3 antenna (≥2 mm overhang, no copper/vias) |
| West | J_PWR (XT30), J_USB1, J_USB2 |
| South | J_M1–M4 (motors), J_SV1–3 (servos), J_CAM1–2, J_LDR |
| East | Overflow for J_M3/J_M4 if South space insufficient |

---

## Power-On Checklist

- [ ] 24V input verified at J_PWR before connecting motors
- [ ] 6.6V servo rail verified (should be 6.3–6.9V) before connecting servos
- [ ] 3.3V logic rail verified before powering MCU
- [ ] No solder bridges on QFN-28 TMC2209 pads (magnification required)
- [ ] Firmware flashed via J_USB2 (PlatformIO: `pio run -t upload`)
- [ ] Startup log shows all modules initialized
- [ ] Heartbeat timeout test passes (motors disable within 500 ms)

---

## Key Differences from Alpha

| Item | Alpha | Beta Rev2 |
|------|-------|-----------|
| MCU | ESP32-S3 DevKit (external) | ESP32-S3-WROOM-1-N16R8 on PCB |
| PSRAM | None / N8R8 | 8 MB Octal PSRAM (N16R8) |
| TMC2209 | Carrier modules | Direct QFN-28 ICs on 4-layer PCB |
| DIAG pins | Not routed | GPIO1/2/3/48 (StallGuard2 enabled) |
| Buck converters | XL4016, MP1584 (external) | TPS54540B + LMR14030S (on PCB) |
| LDO | AP2112K-3.3 (600mA) | MIC5219-3.3 (1A) |
| Servo GPIO | 40, 41, 42 | 38, 39, 40 |
| Leader UART | GPIO17/18, 115200 baud | GPIO41/42, 921600 baud |
| SD card GPIO | MOSI=35, SCK=36, MISO=37, CS=38, CD=39 | CS=10, MOSI=11, SCK=12, MISO=13 |
| Control loop | 500 Hz | 400 Hz |
| USB ports | 1× micro-USB | 2× USB-C (native OTG + CH340K) |
