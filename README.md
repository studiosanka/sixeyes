# SixEyes - 6-DOF Robotic Arm with ROS2 Integration

An open-source, in-progress robotics platform for a 6-degree-of-freedom arm, spanning ESP32 firmware, ROS2 integration, teleoperation tooling, and hardware bring-up.

NodeMesh-specific firmware and IC-design work has moved to a dedicated repository: [https://github.com/SixEyes-Open-Source/nodemesh](https://github.com/SixEyes-Open-Source/nodemesh).

## Scope Clarity (Read This First)

SixEyes is the standard robotic-arm package.

- Standard path in this repository: arm hardware + follower/leader firmware + laptop/ROS2-assisted operation.
- Optional path in NodeMesh repository: 4-MCU edge firmware system with camera nodes and ESP-NOW.

Important default policy for this repository:
- SD card reader hardware may be present as a compatibility option, but it is not part of the default sixeyes runtime flow.
- ESP32-CAM perception nodes are not part of the standard sixeyes package.

Canonical split across all repos:
- [Project Scope And Repo Map](docs/PROJECT_SCOPE_AND_REPO_MAP.md)

## Quick Links

- 📘 **Getting Started**: Read [Complete Documentation Index](docs/README.md#quick-start)
- 🧠 **NodeMesh Repository**: [SixEyes-Open-Source/nodemesh](https://github.com/SixEyes-Open-Source/nodemesh)
- 🚀 **Deploy Firmware**: [Flashing & Deployment Guide](docs/deployment/FLASHING_AND_DEPLOYMENT.md)
- 🔌 **Build Hardware**: [Wiring & Assembly Guide](docs/hardware/WIRING_AND_ASSEMBLY.md)
- 🧩 **Pinout Matrix**: [Dual-Controller Pinout & Wiring Matrix](docs/hardware/DUAL_CONTROLLER_PINOUT_MATRIX.md)
-  **System Architecture**: [Visual Architecture Guide](docs/firmware/VISUAL_ARCHITECTURE_GUIDE.md)
- 🎮 **Teleoperation Architecture**: [Dual-Mode Firmware Plan](docs/firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
- ✅ **Validate Hardware**: [Hardware Validation Procedures](docs/hardware/HARDWARE_VALIDATION.md)
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
               │ USB-CDC (ASCII)
┌──────────────▼──────────────────┐
│  ESP32-S3 Follower (Embedded)   │
│  • 500 Hz control loop          │
│  • Motor drivers (TMC2209)      │
│  • Servo control (LEDC PWM)     │
│  • Safety monitoring            │
└──────────────┬──────────────────┘
               │ Power + Control
┌──────────────▼──────────────────┐
│  Hardware (SixEyes Robot)       │
│  • 4× NEMA23 steppers (24V)     │
│  • 3× MG996R servos (6.6V rail) │
│  • USB-CDC telemetry            │
└─────────────────────────────────┘
```

### Key Features

✅ **Safety-Critical**: Dual heartbeat monitoring with <2.5 ms motor disable latency  
✅ **Real-Time Control**: 500 Hz FreeRTOS deterministic control loop  
✅ **Extensible**: JSON message protocol for future expansion  
🚧 **In Active Development**: Architecture and workflows are evolving as teleoperation and ROS2 integration mature  
✅ **Well-Tested**: Unit tests + hardware validation procedures  
✅ **CI/CD Ready**: GitHub Actions for automated builds and releases  

### Firmware Operation Modes

| Mode | Purpose | Data Path | Status |
|------|---------|-----------|--------|
| **VLA Inference** | Execute AI-planned tasks from laptop/ROS2 | Laptop ROS2 → Follower ESP32 | ✅ Active |
| **Teleoperation** | Stream human-driven leader joint states for mirroring/data collection | Leader ESP32 → Laptop → Follower | ✅ Fully wired (hardware pending) |

Current implementation includes `leader_esp32` JOINT_STATE streaming at 100 Hz, laptop bridge forwarding, teleoperation command handling on follower, and operator utilities for safe command sequencing.

### Choose Your Mode First (Important)

Before building/flashing, choose one workflow and set follower mode accordingly:

1. **VLA Inference workflow**
    - Set in `firmware/follower_esp32/platformio.ini`:
       ```ini
       -DOPERATION_MODE=1
       ```
    - Flash only `follower_esp32`
    - Run ROS2 safety + command nodes from laptop

2. **Teleoperation workflow**
    - Set in `firmware/follower_esp32/platformio.ini`:
       ```ini
       -DOPERATION_MODE=2
       ```
    - Flash both `leader_esp32` and `follower_esp32`
    - Run laptop bridge (`tools/teleoperation_bridge.py`)

## Repository Structure

```
firmware/
├── follower_esp32/              ← Main firmware
│   ├── src/
│   │   ├── main.cpp             (entry point)
│   │   ├── modules/
│   │   │   ├── motor_control/   (500 Hz PID control loop)
│   │   │   ├── servo_control/   (LEDC PWM servo manager)
│   │   │   ├── safety/          (heartbeat, fault manager, safety task)
│   │   │   ├── comms/           (USB-CDC, JSON parser, message router)
│   │   │   ├── modes/           (vla_inference, teleoperation handlers)
│   │   │   ├── drivers/         (TMC2209, LEDC)
│   │   │   └── hal/             (GPIO, timekeeping)
│   │   └── config/              (board_config.h, mode_config.h)
│   ├── test/                    (unit tests & mocks)
│   ├── platformio.ini           (build config)
│   └── lib_deps                 (dependencies)
└── leader_esp32/                (teleoperation joint-state streamer, 100 Hz)

ros2_ws/                         ← ROS2 workspace (runs on laptop)
├── src/
│   ├── safety_node/             ✅ Monitors firmware status, publishes /sixeyes/is_safe
│   ├── vla_inference_node/      🚧 VLA inference stub (in development)
│   ├── camera_node/             ✅ OpenCV camera → /camera/image_raw
│   ├── joint_state_node/        ✅ Bridges /sixeyes/joint_states → /joint_states (rad)
│   └── usb_bridge_node/         ✅ Owns serial port; heartbeat TX, telemetry RX, command TX

simulation/                      ← Gazebo simulation
├── models/
└── launch/

hardware_assets/                 ← Mechanical + PCB production files
├── 3d_print_stl/                (STL files for printed parts)
└── pcb_schematic_gerber/        (schematics + Gerbers)

SixEyes Follower PCB/            ← KiCad PCB project (follower controller board)
├── SixEyes Follower PCB.kicad_pcb
├── SixEyes Follower PCB.kicad_sch
├── SixEyes Follower PCB.kicad_pro
├── SixEyes_Custom.pretty/       (custom footprint library)
└── design_inputs/               (netlists, footprint maps)

docs/                            ← Documentation ⭐ START HERE
├── README.md                    (doc navigation)
├── firmware/                    (implementation guides)
├── hardware/                    (wiring & assembly)
├── deployment/                  (flashing guide)
├── testing/                     (validation procedures)
├── protocols/                   (message specs)
├── ros2/                        (ROS2 integration)
├── ops/                         (CI/CD pipeline)
└── references/                  (datasheets, etc)

.github/workflows/               ← CI/CD pipelines
├── platformio-build.yml
├── code-quality.yml
└── release.yml

CONTRIBUTING.md                  (contribution guidelines)
LICENSE                          (project license)
README.md                        (this file)
```

## Getting Started (5 minutes)

### Quick Mode Setup

```bash
# Follower mode switch (edit before build)
cd firmware/follower_esp32
# platformio.ini -> -DOPERATION_MODE=1  (VLA)
# platformio.ini -> -DOPERATION_MODE=2  (Teleoperation)
```

### For Firmware Developers

1. **Clone the repository**:
   ```bash
   git clone https://github.com/SixEyes-Open-Source/sixeyes.git
   ```
   All code is at the repository root — `firmware/`, `ros2_ws/`, `tools/` etc. are top-level folders.

2. **Install PlatformIO**:
   ```bash
   pip install platformio
   cd firmware/follower_esp32
   ```

3. **Build firmware**:
   ```bash
   pio run
   ```

4. **Flash to ESP32**:
   ```bash
   pio run -t upload
   ```

5. **Monitor output**:
   ```bash
   pio device monitor
   ```

⏭️ **Next**: Read [Flashing & Deployment Guide](docs/deployment/FLASHING_AND_DEPLOYMENT.md)

### For Teleoperation Mode (Phase 2)

1. **Build leader streamer firmware**:
   ```bash
   cd firmware/leader_esp32
   pio run
   ```

2. **Build follower in teleoperation mode**:
   ```bash
   cd ../follower_esp32
   # Ensure platformio.ini has: -DOPERATION_MODE=2
   pio run
   ```

3. **Flash leader ESP32**:
   ```bash
   cd ../leader_esp32
   pio run -t upload
   ```

4. **Flash follower ESP32**:
   ```bash
   cd ../follower_esp32
   pio run -t upload
   ```

5. **Monitor JOINT_STATE output (100 Hz)**:
   ```bash
   cd ../leader_esp32
   pio device monitor
   ```

6. **Capture leader pot home pose as zero (in leader monitor)**:
   ```text
   HOME_ZERO
   ```
   Or JSON form:
   ```json
   {"cmd":"CAPTURE_ZERO"}
   ```
   Expected monitor response:
   ```text
   [CAL] Leader pot zero captured from current pose
   ```

7. **Run laptop bridge with dataset capture**:
   ```bash
   cd tools
   python teleoperation_bridge.py --leader-port COM5 --follower-port COM6 --log-file logs/teleop_session.jsonl
   ```

8. **Validate captured JSONL against schema**:
   ```bash
   cd tools
   python validate_teleop_log.py --input logs/teleop_session.jsonl
   ```

9. **Run operator workflow helper (optional, follower commands + heartbeat)**:
   ```bash
   cd tools
   python operator_control.py --port COM6 teleop-ready
   python operator_control.py --port COM6 home
   python operator_control.py --port COM6 stallguard-home
   ```

⏭️ **Next**: Read [Teleoperation Mode Architecture](docs/firmware/TELEOPERATION_MODE_ARCHITECTURE.md)

### For Hardware Assembly

1. **Gather components** from [Parts List](docs/hardware/WIRING_AND_ASSEMBLY.md#parts-list)
2. **Follow wiring diagram** in [Wiring Guide](docs/hardware/WIRING_AND_ASSEMBLY.md)
3. **Run hardware validation** tests from [Hardware Validation](docs/hardware/HARDWARE_VALIDATION.md)

⏭️ **Next**: Read [WIRING_AND_ASSEMBLY.md](docs/hardware/WIRING_AND_ASSEMBLY.md)

### For ROS2 Integration

1. **Set up ROS2 workspace**:
   ```bash
   cd ros2_ws
   colcon build
   source install/setup.bash
   ```

2. **Run all nodes** with a single launch command:
   ```bash
   # Teleoperation mode (4 nodes)
   ros2 launch sixeyes_bringup teleop.launch.py port:=/dev/ttyACM0

   # VLA inference mode (5 nodes, includes vla_inference_node stub)
   ros2 launch sixeyes_bringup vla.launch.py port:=/dev/ttyACM0
   ```

3. **Monitor firmware status**:
   ```bash
   ros2 topic echo /sixeyes/firmware_status
   ros2 topic echo /sixeyes/is_safe
   ```

⏭️ **Next**: Read [ROS2 Integration Guide](docs/ros2/ROS2_HEARTBEAT_INTEGRATION.md)

## Technical Specifications

| Feature | Specification |
|---------|---------------|
| **Controller** | ESP32-S3 (240 MHz dual-core, 320 KB RAM) |
| **Control Loop** | 500 Hz FreeRTOS deterministic timing |
| **Steppers** | 4× NEMA23 (24V, 2.8A nominal) via TMC2209 |
| **Servos** | 3× MG996R (6.6V rail, 0.17 sec/60°) |
| **Communication** | USB-CDC + heartbeat protocol (50+ Hz) |
| **Safety Timeout** | 500 ms heartbeat detection + <2.5 ms motor disable |
| **Memory Usage** | 6.1% RAM, 10% Flash |
| **Build Status** | Zero compiler warnings ✅ |

## Key Documentation Files

### 📚 Architecture & Design
- [Visual Architecture Guide](docs/firmware/VISUAL_ARCHITECTURE_GUIDE.md) - System diagrams, control flow, timing diagrams
- [Implementation Summary](docs/firmware/IMPLEMENTATION_SUMMARY.md) - Technical specifications, protocol details

### 🔧 Hardware & Deployment
- [Wiring & Assembly](docs/hardware/WIRING_AND_ASSEMBLY.md) - Complete pin mapping, parts list, assembly procedures
- [Hardware Validation](docs/hardware/HARDWARE_VALIDATION.md) - Testing & qualification procedures
- [Flashing & Deployment](docs/deployment/FLASHING_AND_DEPLOYMENT.md) - Firmware build, flash, troubleshooting

### 🧪 Testing & Quality
- [Testing & Validation Guide](docs/testing/TESTING_AND_VALIDATION_GUIDE.md) - Unit tests, test framework, coverage
- [CI/CD Pipeline](docs/ops/CI_CD_PIPELINE.md) - GitHub Actions, automated builds, release process

### 🚀 Communication & ROS2
- [JSON Message Protocol](docs/protocols/JSON_MESSAGE_PROTOCOL.md) - Message specifications, command reference
- [ROS2 Heartbeat Integration](docs/ros2/ROS2_HEARTBEAT_INTEGRATION.md) - Safety protocol, integration details
- [ROS2 Quickstart](docs/ros2/ROS2_HEARTBEAT_QUICKSTART.md) - Step-by-step testing guide

### 📋 References
- [Datasheets & References](docs/references/) - TMC2209, hardware specs, technical references
- [Firmware System Datasheet](docs/references/FIRMWARE_SYSTEM_DATASHEET.md) - IC-style firmware system snapshot (interfaces, safety, limits, maturity)

## Communication Protocols

### ASCII Heartbeat (High-Priority Safety)

**ROS2 → ESP32** (≥50 Hz):
```
HB:0,<sequence>\n
```

**ESP32 → ROS2** (10 Hz):
```
SB:<fault>,<motors_enabled>,<ros2_alive>\n
```

### JSON Message Protocol (Extensible Commands)

```json
{
  "cmd": "MOTOR_TARGET",
  "seq": 1,
  "targets": [0.0, 45.0, 90.0, 135.0]
}
```

See [JSON Message Protocol](docs/protocols/JSON_MESSAGE_PROTOCOL.md) for full specification.

## Safety & Reliability

### Motor Disable Guarantees

- **Automatic Disable**: If ROS2 heartbeat lost for >500 ms
- **Latency**: Motor disable within 1 control loop cycle (~2.5 ms)
- **Dual Monitoring**: Both ROS2 and internal heartbeat must be healthy
- **Non-blocking**: All safety checks run asynchronously to preserve control loop timing

### Testing & Validation

✅ 17 unit tests (all passing)  
✅ 15 hardware validation procedures  
✅ CI/CD automated builds with memory constraints  
⚠️ Some non-blocking ArduinoJson deprecation warnings remain  
✅ Build-ready code quality (leader + follower compile successfully)  

## Development Workflow

### Building Locally

```bash
cd firmware/follower_esp32
pio run              # Build
pio run -t upload    # Flash
pio device monitor   # Monitor
```

### Running Unit Tests

```bash
cd firmware/follower_esp32
pio test             # Run all tests
```

### Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Code style guidelines
- Pull request process
- Commit message conventions
- Testing requirements

## Troubleshooting

### Compilation Issues
👉 See [Build Troubleshooting](docs/deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)

### Hardware Not Responding
👉 See [Hardware Validation](docs/hardware/HARDWARE_VALIDATION.md#troubleshooting)

### ROS2 Communication Problems
👉 See [ROS2 Quickstart - Troubleshooting](docs/ros2/ROS2_HEARTBEAT_QUICKSTART.md#troubleshooting)

## Project Status

| Component | Status | Notes |
|-----------|--------|-------|
| Firmware Core (VLA) | ✅ Stable | Follower control loop + safety heartbeat |
| Firmware Core (Teleop) | ✅ Active | Leader stream + bridge + follower telemetry + schema-validated logging |
| Documentation | ✅ Active | Dual-mode docs updated; ongoing refinements |
| Unit Tests | 🟡 Partial | Existing tests pass; teleop path tests still needed |
| CI/CD Pipeline | ✅ Complete | 3 GitHub Actions workflows |
| Hardware Validation | ✅ Available | Baseline + teleoperation checks documented |
| ROS2 Integration | 🟡 Partial | Heartbeat path done; teleop ROS nodes need expansion |
| Production Readiness | 🚧 In Progress | Core safety + control are advancing; end-to-end teleop and ROS2 workflows are still being completed |

## Current TODO (High Priority)

1. Add automated integration test for `leader_esp32` → bridge → `follower_esp32`
2. Expand ROS2 workspace teleoperation nodes (`joint_state_node`, `usb_bridge_node`) for end-to-end runtime
3. Finalize hardware bring-up checklist for dual-controller (leader + follower) setups

## Performance Metrics

```
Control Loop Frequency:      400 Hz ✓
Motor Disable Latency:       < 2.5 ms ✓
Firmware Size:               335 KB ✓
RAM Usage:                   6.1% ✓
Flash Usage:                 10% ✓
Compiler Warnings:           0 ✓
Unit Test Coverage:          17 tests passing ✓
```

## License

This project is licensed under the [LICENSE](LICENSE) file.

## Contact & Support

For issues, questions, or contributions:
- 📧 Email: sixeyesopensource@gmail.com
- 🐛 Issues: https://github.com/SixEyes-Open-Source/sixeyes/issues
- 💬 Discussions: https://github.com/SixEyes-Open-Source/sixeyes/discussions

---

**Last Updated**: March 2026  
**Latest Version**: Check [Releases](https://github.com/SixEyes-Open-Source/sixeyes/releases)
