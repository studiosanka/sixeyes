# Beta Follower PCB — Pinout & Connector Reference

PCB: SixEyes Follower Beta Rev2, 60×60 mm 4-layer  
Module: ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB Octal PSRAM)  
Source: `docs/references/SixEyes_Node0.pdf`

---

## Forbidden GPIOs (N16R8 — must not connect externally)

| GPIO | Reason |
|------|--------|
| GPIO26 | Internal octal flash clock |
| GPIO33 | Flash D4 |
| GPIO34 | Flash D5 |
| GPIO35 | Octal PSRAM D6 / WP |
| GPIO36 | Octal PSRAM D7 / HOLD |
| GPIO37 | Octal PSRAM CLK |

---

## GPIO Allocation Table

| GPIO | Net Name | Notes |
|------|----------|-------|
| 1 | J1_BASE_DIAG | StallGuard/fault; 47 kΩ pull-up to 3.3V ← TMC2209-1 DIAG |
| 2 | J2_SHLDA_DIAG | StallGuard/fault; 47 kΩ pull-up ← TMC2209-2 DIAG |
| 3 | J3_SHLDB_DIAG | StallGuard/fault; 47 kΩ pull-up ← TMC2209-3 DIAG |
| 4 | DRV_EN_ALL | Active LOW; shared EN → Pin 18 of all 4× TMC2209 |
| 5 | J1_BASE_STEP | → TMC2209-1 STEP |
| 6 | J1_BASE_DIR | → TMC2209-1 DIR |
| 7 | J1_BASE_PDN | Single-wire UART; 1 kΩ series + 4.7 kΩ pull-up → TMC2209-1 PDN |
| 8 | J2_SHLDA_STEP | → TMC2209-2 STEP |
| 9 | J2_SHLDA_DIR | → TMC2209-2 DIR |
| 10 | SD_CS | SPI chip select; 10 kΩ pull-up |
| 11 | SD_MOSI | SPI MOSI; 10 kΩ pull-up |
| 12 | SD_SCK | SPI clock |
| 13 | SD_MISO | SPI MISO; 10 kΩ pull-up |
| 14 | J2_SHLDA_PDN | Single-wire UART; 1 kΩ + 4.7 kΩ pull-up → TMC2209-2 PDN |
| 15 | J3_SHLDB_STEP | → TMC2209-3 STEP |
| 16 | J3_SHLDB_DIR | → TMC2209-3 DIR |
| 17 | J3_SHLDB_PDN | Single-wire UART; 1 kΩ + 4.7 kΩ pull-up → TMC2209-3 PDN |
| 18 | J4_ELBOW_STEP | → TMC2209-4 STEP |
| 19 | USB_D- | 90 Ω diff pair → J_USB1 (Native USB-OTG); route L1 only |
| 20 | USB_D+ | 90 Ω diff pair → J_USB1 (Native USB-OTG); route L1 only |
| 21 | J4_ELBOW_DIR | → TMC2209-4 DIR |
| 38 | SRV_WRIST_PITCH | LEDC/MCPWM PWM → J_SV1 |
| 39 | SRV_WRIST_YAW | LEDC/MCPWM PWM → J_SV2 |
| 40 | SRV_GRIPPER | LEDC/MCPWM PWM → J_SV3 |
| 41 | LDR_UART_TX | 921600 baud → Node1 ESP32-C6 RX (J_LDR Pin 1) |
| 42 | LDR_UART_RX | 921600 baud ← Node1 ESP32-C6 TX (J_LDR Pin 2) |
| 43 | UART0_TXD | → CH340K RXD (J_USB2 debug/flash) |
| 44 | UART0_RXD | ← CH340K TXD (J_USB2 debug/flash) |
| 47 | J4_ELBOW_PDN | Single-wire UART; 1 kΩ + 4.7 kΩ pull-up → TMC2209-4 PDN |
| 48 | J4_ELBOW_DIAG | StallGuard/fault; 47 kΩ pull-up ← TMC2209-4 DIAG |
| 0 | MCU_BOOT | 10 kΩ pull-up; tactile switch to GND |
| 46 | MCU_STRAP_LOW | 10 kΩ pull-down to GND (required by ESP32-S3 strapping spec) |
| 45 | — | Leave floating (internal OTP default) |

---

## Connector Directory

| Ref | Type | Signal / Net | Edge | Power Rail |
|-----|------|--------------|------|------------|
| J_PWR | XT30 RA 2-pin | 24V DC in / GND | West | 24V |
| J_USB1 | USB-C receptacle | Native USB-OTG (D−/D+) | West | — |
| J_USB2 | USB-C receptacle | CH340K UART debug/flash | West | — |
| J_M1 | Micro-Fit 3.0 4-pin | Base stepper phases A1/A2/B1/B2 | South/East | 24V |
| J_M2 | Micro-Fit 3.0 4-pin | Shoulder A phases | South/East | 24V |
| J_M3 | Micro-Fit 3.0 4-pin | Shoulder B phases | South/East | 24V |
| J_M4 | Micro-Fit 3.0 4-pin | Elbow phases | South/East | 24V |
| J_SV1 | 2.54 mm 3-pin | Wrist Pitch PWM / 6.6V / GND | South | 6.6V |
| J_SV2 | 2.54 mm 3-pin | Wrist Yaw PWM / 6.6V / GND | South | 6.6V |
| J_SV3 | 2.54 mm 3-pin | Gripper PWM / 6.6V / GND | South | 6.6V |
| J_CAM1 | Micro-Fit 3.0 2-pin | Node2 5V / GND (global cam) | South | 5.0V |
| J_CAM2 | Micro-Fit 3.0 2-pin | Node3 5V / GND (wrist cam) | South | 5.0V |
| J_LDR | 2.54 mm 4-pin | Node1 UART TX, RX, GND, 5V | South | — |
| J_SD | MicroSD push-pull | SPI: CS/MOSI/SCK/MISO | Interior | 3.3V |

---

## Power Rails

| Rail | Regulator | Primary Loads |
|------|-----------|---------------|
| 24V | Direct input (fused) | 4× TMC2209 VM |
| 6.6V | TPS54540B sync buck | 3× MG996R/MG995 servos |
| 5.0V | LMR14030S async buck | Node1/2/3 power headers, LDO input |
| 3.3V | MIC5219-3.3YM5 LDO (1A) | ESP32-S3, TMC2209 VIO |

---

## Leader (Node1) Interface

Node1 = ESP32-C6 SuperMini (offboard). Two separate connectors:

| Connector | Signals | Notes |
|-----------|---------|-------|
| J_LDR | TX, RX, GND, 5V | UART signals + 5V power |

UART config: 921600 baud, 8N1, non-blocking async. Parser uses magic-number scan + CRC-CCITT (poly 0x1021).

---

## Camera Nodes (Node2/Node3)

J_CAM1/J_CAM2 supply 5V power only. All data via wireless ESP-NOW, channel 6.  
Payload: 128-byte brightness histogram embedded in ExperiencePacket (magic `0x4E4D5650`).  
Freshness gate: Node0 discards frames older than 300 ms.
