# SixEyes — AI Agent Context Reference

## Repos (3 GitHub repos, 1 local workbench)
| Folder | GitHub Repo |
|---|---|
| `nodemesh/` | https://github.com/studiosanka/nodemesh |
| `cyclocad/` | https://github.com/studiosanka/cyclocad |
| everything else | https://github.com/studiosanka/sixeyes |

---

## Project
6-DOF robotic arm. Apache 2.0. Studio Sanka.  
Two operating modes (compile-time switch `-DOPERATION_MODE`):
- `1` = VLA Inference: ROS2 AI planner sends motor targets
- `2` = Teleoperation: Leader ESP32 streams joint angles → Follower mirrors

Two hardware generations:
- **Alpha** — through-hole DevKit carrier, single-layer protoboard, external TMC2209 modules
- **Beta Rev2** — 60×60 mm 4-layer custom PCB, integrated MCU/drivers/bucks, ESP32-S3-WROOM-1-N16R8

---

## Firmware Structure
```
firmware/
  alpha/
    follower_esp32/   env: alpha_follower  (ESP32-S3 DevKit, 500 Hz)
    leader_esp32/     env: alpha_leader    (ESP32-C6 SuperMini, 100 Hz ADC)
  beta/
    follower_esp32/   env: beta_follower   (ESP32-S3-WROOM-1-N16R8, 400 Hz)
    leader_esp32/     env: beta_leader     (ESP32-C3-MINI-1, 100 Hz ADC)
                      NOTE: GPIO map differs from alpha leader. GPIO0–5 for ADC,
                      GPIO6 RX / GPIO7 TX for UART1. Update board_config before flashing.
```

Build:
```bash
cd firmware/alpha/follower_esp32 && pio run -e alpha_follower
cd firmware/beta/follower_esp32  && pio run -e beta_follower
```

**Beta IntelliSense note**: After first `pio run` in beta dir, VS Code include-path squiggles resolve automatically.

---

## Alpha vs Beta — Key Differences

| Item | Alpha | Beta Rev2 |
|---|---|---|
| Follower MCU | ESP32-S3 DevKit | ESP32-S3-WROOM-1-N16R8 on PCB |
| Leader MCU | ESP32-C6 SuperMini (devkit) | ESP32-C3-MINI-1 (integrated beta leader PCB) |
| PSRAM | None | 8 MB Octal (GPIO26/33/34/35/36/37 forbidden) |
| TMC2209 | Carrier modules | Direct QFN-28 on PCB |
| DIAG pins | Not routed | GPIO1/2/3/48 (StallGuard2) |
| EN pin | GPIO14, active LOW | GPIO4 (DRV_EN_ALL), active LOW |
| Servo GPIO | 40, 41, 42 | 38, 39, 40 |
| Leader ADC GPIO | GPIO0,1,2,3,4,6 | GPIO0,1,2,3,4,5 (all ADC1_CH0–5) |
| Leader UART (Node1 side) | GPIO17 TX / GPIO18 RX, 115200 baud | GPIO7 TX / GPIO6 RX, 921600 baud |
| Leader UART (Node0 side) | GPIO17/18, 115200 baud | GPIO41/42, 921600 baud |
| Leader power | Via J_LDR_PWR (follower 5V rail) | Self-powered USB-C on leader PCB |
| SD card | MOSI=35,SCK=36,MISO=37,CS=38,CD=39 | CS=10,MOSI=11,SCK=12,MISO=13 |
| Control loop | 500 Hz | 400 Hz |
| Follower USB | 1× micro-USB | 2× USB-C (native OTG + CH340K) |
| Leader USB | Via devkit USB | USB-C on leader PCB (native USB JTAG, GPIO18/19) |

---

## Beta Forbidden GPIOs (N16R8 octal PSRAM/flash — never route externally)
GPIO26, GPIO33, GPIO34, GPIO35, GPIO36, GPIO37

---

## Beta GPIO Quick Reference

| GPIO | Function |
|---|---|
| 1/2/3/48 | TMC2209 DIAG (J1/J2/J3/J4, StallGuard) |
| 4 | DRV_EN_ALL (active LOW, all 4 drivers) |
| 5/8/15/18 | STEP (J1/J2/J3/J4) |
| 6/9/16/21 | DIR (J1/J2/J3/J4) |
| 7/14/17/47 | PDN UART (J1/J2/J3/J4) |
| 10/11/12/13 | SD CS/MOSI/SCK/MISO |
| 19/20 | USB D-/D+ (Native OTG → J_USB1) |
| 38/39/40 | Servo PWM (Wrist Pitch/Yaw/Gripper) |
| 41/42 | Leader UART TX/RX (921600 baud) |
| 43/44 | UART0 TXD/RXD (CH340K → J_USB2) |

---

## Alpha GPIO Quick Reference

| GPIO | Function |
|---|---|
| 13/10/16/6 | TMC2209 PDN (J1/J2/J3/J4) |
| 12/9/15/5 | STEP (J1/J2/J3/J4) |
| 11/8/7/4 | DIR (J1/J2/J3/J4) |
| 14 | EN_ALL (active LOW) |
| 17/18 | Leader UART TX/RX (115200 baud) |
| 35/36/37/38/39 | SD MOSI/SCK/MISO/CS/CD |
| 40/41/42 | Servo PWM |

---

## Key File Locations
```
firmware/alpha/follower_esp32/src/modules/config/board_config.h   Alpha GPIO config
firmware/beta/follower_esp32/src/modules/config/board_config.h    Beta GPIO config
firmware/beta/follower_esp32/src/modules/drivers/tmc2209/tmc2209_config.h  Beta motor pins + DIAG
docs/references/SixEyes_Node0.pdf                                  Beta PCB design spec (authoritative)
docs/hardware/alpha/                                               Alpha-specific hardware docs
docs/hardware/beta/                                                Beta-specific hardware docs
hardware_assets/pcb_project_files/                                 KiCad PCB project
hardware_assets/pcb_latest_gerber/                                 Gerber files
```

---

## Protocols (hardware-agnostic)

**Safety heartbeat (ASCII)**
- ROS2→ESP32: `HB:source_id,seq\n` at ≥50 Hz
- ESP32→ROS2: `SB:fault_bitmask,motors_en,ros2_alive\n` at 10 Hz
- Timeout: >500ms → motors disabled, latched until `RESET_FAULT`

**JSON commands (line-delimited, \n framed, max 512B)**
- VLA mode: `MOTOR_TARGET`, `SERVO_TARGET`, `ENABLE_MOTORS`, `RESET_FAULT`, `HOME_ZERO`, `HOME_STALLGUARD`, `TUNE_PID`
- Teleop mode: `JOINT_STATE` (leader→follower), `TELEMETRY_STATE` (follower→host)

**Beta leader UART** (Node1 inter-board): 921600 baud, 8N1, magic-number scan + CRC-CCITT (poly 0x1021), absolute joint targets.

---

## ROS2 Nodes
| Node | Role |
|---|---|
| `usb_bridge_node` | owns serial port; HB TX (50Hz), telemetry RX, command TX |
| `safety_node` | parses SB: → publishes `/sixeyes/is_safe` (Bool) |
| `joint_state_node` | converts `/sixeyes/joint_states` (deg) → `/joint_states` (rad) |
| `camera_node` | USB cam → `/camera/image_raw` |
| `vla_inference_node` | stub (not yet implemented) |

---

## Docs Structure
```
docs/
  hardware/
    alpha/    WIRING_AND_ASSEMBLY, HARDWARE_VALIDATION, PINOUT_MATRIX,
              LEADER_PCB_DESIGN, FOLLOWER_PCB_DESIGN
    beta/     WIRING_AND_ASSEMBLY, PINOUT_MATRIX, LEADER_PCB_DESIGN
  firmware/   TELEOPERATION_MODE_ARCHITECTURE
  ros2/       ROS2_INTEGRATION (merged)
  protocols/  JSON_MESSAGE_PROTOCOL
  deployment/ FLASHING_AND_DEPLOYMENT
  testing/    TESTING_AND_VALIDATION_GUIDE
  ops/        CI_CD_PIPELINE
  references/ SixEyes_Node0.pdf (beta PCB), Technical Reference .txt files
```

---

## Subprojects (independent repos, optional)
- **nodemesh/**: 4-MCU edge learning system. Node0=Beta PCB orchestrator, Node1=leader, Node2/3=cams. On-device MLP behavioral cloning. Experimental.
- **cyclocad/**: Cycloidal gear profile generator → DXF/CSV + SolidWorks macro. Mature, standalone.

---

## Safety Rules (never violate)
- Motors disabled by default at boot
- `RESET_FAULT` required to re-enable after any fault
- Heartbeat must be active before enabling motors
- Beta DIAG pins (GPIO1/2/3/48) are open-drain — 47kΩ pull-ups mandatory
- Beta forbidden GPIOs (26/33/34/35/36/37) must never be used in firmware
