# SixEyes — 6-DOF Robotic Arm with ROS2 Integration

An open-source, in-progress robotics platform for a 6-degree-of-freedom arm, spanning ESP32 firmware, ROS2 integration, teleoperation tooling, and hardware bring-up.

**Repository**: [studiosanka/sixeyes](https://github.com/studiosanka/sixeyes)  
**NodeMesh subproject** (optional, 4-MCU edge system): [studiosanka/nodemesh](https://github.com/studiosanka/nodemesh)  
**CycloCad subproject** (cycloidal gear generator): [studiosanka/cyclocad](https://github.com/studiosanka/cyclocad)

## Scope Clarity (Read This First)

SixEyes is the standard robotic-arm package.

- **Standard path** (this repo): arm hardware + follower/leader firmware + laptop/ROS2-assisted operation.
- **Optional path** (NodeMesh repo): 4-MCU edge firmware system with camera nodes and ESP-NOW. Not required for standard SixEyes operation.

Default policy:
- SD card reader hardware may be present as a compatibility option but is not part of the default runtime flow.
- ESP32-CAM perception nodes are not part of the standard package.

Canonical split: [Project Scope And Repo Map](docs/PROJECT_SCOPE_AND_REPO_MAP.md)

## Hardware Generations

| | Alpha | Beta Rev2 |
|---|---|---|
| Form factor | Through-hole DevKit carrier, single-layer protoboard | 60×60 mm 4-layer custom PCB |
| MCU | ESP32-S3 DevKit | ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB Octal PSRAM) |
| Motor drivers | External TMC2209 carrier modules | Direct TMC2209 QFN-28 on PCB |
| Power | Bench supply / external regulators | Onboard 6.6 V servo buck (TPS54540B) + 5 V logic buck (LMR14030S) + 3.3 V LDO |
| USB | 1× micro-USB | 2× USB-C (native OTG + CH340K debug/flash) |
| Control loop | 500 Hz | 400 Hz |
| Status | Working hardware baseline | PCB in design (KiCad schematic in progress) |

## Quick Links

- 📘 **Getting Started**: [Complete Documentation Index](docs/README.md#quick-start)
- 🚀 **Deploy Firmware**: [Flashing & Deployment Guide](docs/deployment/FLASHING_AND_DEPLOYMENT.md)
- 🔌 **Build Hardware**: [Wiring & Assembly Guide](docs/hardware/alpha/WIRING_AND_ASSEMBLY.md)
- 🧩 **Pinout Matrix (Alpha)**: [Alpha Pinout & Wiring Matrix](docs/hardware/alpha/PINOUT_MATRIX.md)
- 🗺️ **System Architecture**: [Technical Reference — Beta Rev2](docs/references/SixEyes%20Technical%20Reference%20June%202026.md)
- 🎮 **Teleoperation Architecture**: [Dual-Mode Firmware Plan](docs/firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
- ✅ **Validate Hardware**: [Hardware Validation Procedures](docs/hardware/alpha/HARDWARE_VALIDATION.md)
- 🛠️ **Run Tests**: [Testing & Validation Guide](docs/testing/TESTING_AND_VALIDATION_GUIDE.md)

## Project Overview

### System Architecture

```
┌─────────────────────────────────┐
│  ROS2 Safety Node (Laptop)      │
│  • Heartbeat control (50 Hz)    │
│  • Motion planning              │
│  • Emergency stop logic         │
└──────────────┬──────────────────┘
               │ USB-CDC (ASCII + JSON)
┌──────────────▼──────────────────┐
│  ESP32-S3 Follower (Embedded)   │
│  • 400/500 Hz control loop      │
│  • Motor drivers (TMC2209)      │
│  • Servo control (LEDC PWM)     │
│  • Safety monitoring            │
└──────────────┬──────────────────┘
               │ Power + Control
┌──────────────▼──────────────────┐
│  Hardware (SixEyes Robot)       │
│  • 4× NEMA23 steppers (24V)     │
│  • 3× MG996R/MG995 servos       │
│  • USB-CDC telemetry            │
└─────────────────────────────────┘
```

### Key Features

✅ **Safety-Critical**: Dual heartbeat monitoring with <2.5 ms motor disable latency  
✅ **Real-Time Control**: 400–500 Hz FreeRTOS deterministic control loop  
✅ **Extensible**: JSON message protocol for future expansion  
🚧 **In Active Development**: Beta PCB design in progress; teleoperation and ROS2 integration maturing  
✅ **Well-Tested**: Unit tests + hardware validation procedures  
✅ **CI/CD Ready**: GitHub Actions for automated builds and releases  

### Firmware Operation Modes

| Mode | Purpose | Data Path | Status |
|------|---------|-----------|--------|
| **VLA Inference** | Execute AI-planned tasks from laptop/ROS2 | Laptop ROS2 → Follower ESP32 | ✅ Active |
| **Teleoperation** | Stream human-driven leader joint states for mirroring/data collection | Leader ESP32 → Laptop → Follower | ✅ Fully wired (hardware pending) |

### Choose Your Mode First

Before building/flashing, set the follower mode in `platformio.ini`:

```ini
-DOPERATION_MODE=1   # VLA Inference: ROS2 AI planner sends motor targets
-DOPERATION_MODE=2   # Teleoperation: Leader ESP32 streams joint angles → Follower mirrors
```

## Repository Structure

```
firmware/
├── alpha/
│   ├── follower_esp32/      env: alpha_follower  (ESP32-S3 DevKit, 500 Hz)
│   └── leader_esp32/        env: alpha_leader    (ESP32-C6 SuperMini, 100 Hz ADC)
└── beta/
    ├── follower_esp32/      env: beta_follower   (ESP32-S3-WROOM-1-N16R8, 400 Hz)
    └── leader_esp32/        (shared leader hardware with Alpha)

ros2_ws/                     ← ROS2 workspace (runs on laptop)
├── src/
│   ├── safety_node/         ✅ Monitors firmware status, publishes /sixeyes/is_safe
│   ├── vla_inference_node/  🚧 VLA inference stub (in development)
│   ├── camera_node/         ✅ OpenCV camera → /camera/image_raw
│   ├── joint_state_node/    ✅ Bridges /sixeyes/joint_states → /joint_states (rad)
│   └── usb_bridge_node/     ✅ Owns serial port; heartbeat TX, telemetry RX, command TX

simulation/                  ← Gazebo simulation
├── models/
└── launch/

hardware_assets/             ← Mechanical + PCB production files
├── 3d_print_stl/            (STL files for printed parts)
├── pcb_project_files/       (KiCad projects)
│   └── SixEyes Follower PCB Beta/   (Beta Rev2 schematic, in progress)
└── pcb_latest_gerber/       (Gerber files for fabrication)

nodemesh/                    ← NodeMesh subproject (studiosanka/nodemesh)
cyclocad/                    ← CycloCad subproject (studiosanka/cyclocad)

docs/                        ← Documentation ⭐ START HERE
├── README.md                (doc navigation index)
├── firmware/                (implementation guides)
├── hardware/
│   ├── alpha/               (Alpha wiring, validation, pinout, PCB design)
│   └── beta/                (Beta wiring, pinout — from SixEyes_Node0.pdf)
├── deployment/              (flashing guide)
├── testing/                 (validation procedures)
├── protocols/               (message specs)
├── ros2/                    (ROS2 integration)
├── ops/                     (CI/CD pipeline)
└── references/              (datasheets, technical references)

.github/workflows/           ← CI/CD pipelines
├── platformio-build.yml
├── code-quality.yml
└── release.yml

CONTRIBUTING.md
LICENSE
README.md                    (this file)
```

## Getting Started

### For Firmware Developers

1. **Clone**:
   ```bash
   git clone https://github.com/studiosanka/sixeyes.git
   ```

2. **Build Alpha firmware** (working hardware):
   ```bash
   cd firmware/alpha/follower_esp32
   pio run -e alpha_follower
   pio run -e alpha_follower -t upload
   pio device monitor
   ```

3. **Build Beta firmware** (for Beta PCB when ready):
   ```bash
   cd firmware/beta/follower_esp32
   pio run -e beta_follower
   ```

⏭️ **Next**: [Flashing & Deployment Guide](docs/deployment/FLASHING_AND_DEPLOYMENT.md)

### For Teleoperation Mode

1. **Flash leader** (Alpha hardware):
   ```bash
   cd firmware/alpha/leader_esp32
   pio run -e alpha_leader -t upload
   pio device monitor
   ```

2. **Capture home pose zero**:
   ```text
   HOME_ZERO
   ```

3. **Flash follower in teleop mode** (`-DOPERATION_MODE=2`):
   ```bash
   cd firmware/alpha/follower_esp32
   pio run -e alpha_follower -t upload
   ```

4. **Run laptop bridge**:
   ```bash
   cd tools
   python teleoperation_bridge.py --leader-port COM5 --follower-port COM6 --log-file logs/teleop_session.jsonl
   ```

5. **Validate captured JSONL**:
   ```bash
   python validate_teleop_log.py --input logs/teleop_session.jsonl
   ```

⏭️ **Next**: [Teleoperation Mode Architecture](docs/firmware/TELEOPERATION_MODE_ARCHITECTURE.md)

### For Hardware Assembly

1. **Gather components**: [Parts List](docs/hardware/alpha/WIRING_AND_ASSEMBLY.md#parts-list)
2. **Wire the hardware**: [Wiring Guide](docs/hardware/alpha/WIRING_AND_ASSEMBLY.md)
3. **Validate**: [Hardware Validation](docs/hardware/alpha/HARDWARE_VALIDATION.md)

⏭️ **Next**: [Wiring & Assembly Guide](docs/hardware/alpha/WIRING_AND_ASSEMBLY.md)

### For ROS2 Integration

```bash
cd ros2_ws
colcon build
source install/setup.bash

# Teleoperation mode (4 nodes)
ros2 launch sixeyes_bringup teleop.launch.py port:=/dev/ttyACM0

# VLA inference mode (5 nodes)
ros2 launch sixeyes_bringup vla.launch.py port:=/dev/ttyACM0
```

```bash
ros2 topic echo /sixeyes/firmware_status
ros2 topic echo /sixeyes/is_safe
```

⏭️ **Next**: [ROS2 Integration Guide](docs/ros2/ROS2_INTEGRATION.md)

## Technical Specifications

| Feature | Alpha | Beta Rev2 |
|---------|-------|-----------|
| **MCU** | ESP32-S3 DevKit (240 MHz) | ESP32-S3-WROOM-1-N16R8 (240 MHz, 8 MB PSRAM) |
| **Control Loop** | 500 Hz FreeRTOS | 400 Hz FreeRTOS |
| **Steppers** | 4× NEMA23 via TMC2209 modules | 4× NEMA23 via TMC2209 QFN direct |
| **Servos** | 3× MG996R | 3× MG996R/MG995 (6.6V rail) |
| **USB** | 1× micro-USB | 2× USB-C (OTG + CH340K) |
| **Safety Timeout** | 500 ms heartbeat + <2.5 ms disable | Same |
| **StallGuard** | Not routed | GPIO1/2/3/48 DIAG pins |
| **Build Status** | Zero compiler warnings ✅ | In schematic design 🚧 |

## Communication Protocols

### ASCII Heartbeat (Safety-Critical)

**ROS2 → ESP32** (≥50 Hz):
```
HB:0,<sequence>\n
```

**ESP32 → ROS2** (10 Hz):
```
SB:<fault_bitmask>,<motors_enabled>,<ros2_alive>\n
```

### JSON Message Protocol

```json
{"cmd": "MOTOR_TARGET", "seq": 1, "targets": [0.0, 45.0, 90.0, 135.0]}
```

See [JSON Message Protocol](docs/protocols/JSON_MESSAGE_PROTOCOL.md) for full specification.

## Safety Guarantees

- **Auto-disable**: Motors off if ROS2 heartbeat lost for >500 ms
- **Latency**: Motor disable within 1 control loop cycle (~2.5 ms at 400 Hz)
- **Boot state**: Motors disabled by default; `RESET_FAULT` required after any fault
- **Heartbeat required**: Must be active before motors can be enabled
- **Beta DIAG pins**: Open-drain — 47 kΩ pull-ups mandatory on GPIO1/2/3/48
- **Beta forbidden GPIOs**: 26/33/34/35/36/37 committed to octal PSRAM/flash — never route externally

## Key Documentation

### Architecture & Design
- [Technical Reference — Beta Rev2](docs/references/SixEyes%20Technical%20Reference%20June%202026.md)
- [Teleoperation Mode Architecture](docs/firmware/TELEOPERATION_MODE_ARCHITECTURE.md)

### Hardware & Deployment
- [Wiring & Assembly (Alpha)](docs/hardware/alpha/WIRING_AND_ASSEMBLY.md)
- [Pinout Matrix (Alpha)](docs/hardware/alpha/PINOUT_MATRIX.md)
- [Leader PCB Design (Beta)](docs/hardware/beta/LEADER_PCB_DESIGN.md)
- [Hardware Validation](docs/hardware/alpha/HARDWARE_VALIDATION.md)
- [Flashing & Deployment](docs/deployment/FLASHING_AND_DEPLOYMENT.md)

### Testing & Quality
- [Testing & Validation Guide](docs/testing/TESTING_AND_VALIDATION_GUIDE.md)
- [CI/CD Pipeline](docs/ops/CI_CD_PIPELINE.md)

### Communication & ROS2
- [JSON Message Protocol](docs/protocols/JSON_MESSAGE_PROTOCOL.md)
- [ROS2 Integration](docs/ros2/ROS2_INTEGRATION.md)

### References
- [Technical Reference — Beta Rev2](docs/references/SixEyes%20Technical%20Reference%20June%202026.md)
- [References Directory](docs/references/) — TMC2209 datasheet, historical technical references

## Project Status

| Component | Status | Notes |
|-----------|--------|-------|
| Firmware Core — Alpha | ✅ Stable | 500 Hz control loop, safety heartbeat, teleop wired |
| Firmware Core — Beta | 🚧 In Progress | 400 Hz, DIAG/StallGuard, beta board_config.h |
| Beta PCB Schematic | 🚧 In Progress | KiCad, components placed, routing not started |
| Documentation | ✅ Active | Dual-mode docs current; beta hardware docs growing |
| Unit Tests | 🟡 Partial | 17 tests pass; teleop path tests needed |
| CI/CD Pipeline | ✅ Complete | 3 GitHub Actions workflows |
| Hardware Validation | ✅ Available | Alpha procedures documented |
| ROS2 Integration | 🟡 Partial | Heartbeat done; teleop ROS nodes need expansion |

## Development Workflow

```bash
# Alpha follower — build, flash, monitor
cd firmware/alpha/follower_esp32
pio run -e alpha_follower -t upload
pio device monitor

# Alpha leader — build, flash
cd firmware/alpha/leader_esp32
pio run -e alpha_leader -t upload

# Beta follower — build (hardware in progress)
cd firmware/beta/follower_esp32
pio run -e beta_follower

# Beta leader — build (ESP32-C3-MINI-1 integrated PCB)
cd firmware/beta/leader_esp32
pio run -e beta_leader

# Unit tests
cd firmware/alpha/follower_esp32
pio test
```

## Key Files

```
firmware/alpha/follower_esp32/src/modules/config/board_config.h        Alpha follower GPIO assignments
firmware/alpha/leader_esp32/src/...                                     Alpha leader GPIO assignments
firmware/beta/follower_esp32/src/modules/config/board_config.h         Beta follower GPIO assignments
firmware/beta/follower_esp32/src/modules/drivers/tmc2209/tmc2209_config.h  Beta TMC2209 pins + DIAG
firmware/beta/leader_esp32/src/...                                      Beta leader GPIO assignments (C3)

docs/hardware/alpha/PINOUT_MATRIX.md                                   Alpha GPIO quick reference
docs/hardware/beta/PINOUT_MATRIX.md                                    Beta follower GPIO quick reference
docs/hardware/beta/LEADER_PCB_DESIGN.md                                Beta leader PCB design (ESP32-C3-MINI-1)
docs/references/SixEyes Technical Reference June 2026.md               Authoritative system spec (Beta Rev2)

hardware_assets/pcb_project_files/                                     KiCad PCB project (Beta follower)
hardware_assets/pcb_latest_gerber/                                     Gerber files for fabrication
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for code style, PR process, commit conventions, and testing requirements.

## Troubleshooting

- **Compilation issues**: [Build Troubleshooting](docs/deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)
- **Hardware not responding**: [Hardware Validation](docs/hardware/alpha/HARDWARE_VALIDATION.md#troubleshooting)
- **ROS2 communication problems**: [ROS2 Integration](docs/ros2/ROS2_INTEGRATION.md)

## Licence

SixEyes uses a dual licence:

| | Licence |
|---|---|
| Firmware & software | [Apache 2.0](LICENSE) |
| Hardware design files (KiCad, Gerbers, STL) | [CERN OHL-S v2](hardware_assets/LICENSE_HARDWARE) |

See [CONTRIBUTING.md](CONTRIBUTING.md) for full usage terms, including what is required if you manufacture or sell hardware based on these designs.

## Contact & Support

- 🐛 Issues: https://github.com/studiosanka/sixeyes/issues
- 💬 Discussions: https://github.com/studiosanka/sixeyes/discussions

---

**Last Updated**: June 2026
