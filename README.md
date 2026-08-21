# SixEyes — 6-DOF Robotic Arm with ROS2 Integration

An open-source, in-progress robotics platform for a 6-degree-of-freedom arm, spanning ESP32 firmware, ROS2 integration, and hardware bring-up.

**Repository**: [studiosanka/sixeyes](https://github.com/studiosanka/sixeyes)
**NodeMesh subproject** (optional, 4-MCU edge system): [studiosanka/nodemesh](https://github.com/studiosanka/nodemesh) — currently targets legacy Alpha/Beta hardware only, not yet compatible with v1
**CycloCad subproject** (cycloidal gear generator): [studiosanka/cyclocad](https://github.com/studiosanka/cyclocad) — unaffected by the v1 rework

## Scope Clarity (Read This First)

SixEyes is the standard robotic-arm package.

- **Standard path** (this repo): arm hardware + distributed joint firmware + laptop/ROS2-assisted operation.
- **Optional path** (NodeMesh repo): 4-MCU edge firmware system with camera nodes and ESP-NOW. Not required for standard SixEyes operation, and not currently compatible with v1 hardware (see below).

Canonical split: [Project Scope And Repo Map](docs/PROJECT_SCOPE_AND_REPO_MAP.md)

## Architecture — v1 (Current)

SixEyes v1 is a **major architectural change from Alpha/Beta**: no leader arm, no single monolithic follower MCU. Instead, one Universal Joint PCB design (ESP32-C6-MINI-1, ~42×42mm, 4-layer) is populated four ways and distributed across the arm on a shared CAN bus.

| Node | Populated | Role |
|---|---|---|
| Base | Stepper 1 driver, USB-C, CAN master | Bus master, host bridge to laptop/ROS2 |
| Shoulder L | Stepper 2A driver | CAN node |
| Shoulder R | Stepper 2B driver | CAN node, inverse-direction logic |
| Elbow | Stepper 3 driver, 3× servo headers (open-loop) | CAN node, closest hop to wrist/gripper |

- **Bus**: CAN (TWAI), 1 Mbps, linear daisy chain Base → Shoulder L → Shoulder R → Elbow
- **Position feedback**: SPI magnetic encoder per node (MT6835 / AS5048A) — new in v1, Alpha/Beta had none on the follower side
- **Control input**: laptop/ROS2 only for now. The leader arm (Alpha/Beta's teleoperation input device) has been eliminated; a future IMU + inverse-kinematics input path is planned but not yet designed.
- **Status**: hardware spec and CAN protocol are drafted; firmware (`firmware/v1/`) and the KiCad PCB project are not yet started. See [`docs/V1_TODO.md`](docs/V1_TODO.md) for the live task list.

📐 **v1 Hardware Spec**: [Universal Joint PCB — Design Reference](docs/hardware/v1/v1_PCB_Design_Reference.md)
🚌 **v1 CAN Protocol** (draft): [CAN Message Protocol](docs/protocols/CAN_MESSAGE_PROTOCOL.md)
📋 **v1 Rework Task List**: [docs/V1_TODO.md](docs/V1_TODO.md)

## Legacy Hardware (Alpha/Beta)

Alpha and Beta are the prior generation: one monolithic follower MCU per arm (ESP32-S3) directly driving all steppers/servos over local GPIO, plus a separate leader-arm MCU streaming potentiometer joint state over UART for teleoperation. **Both are retired in favor of v1** and kept only as a working reference until v1 hardware is fabricated and validated — Alpha remains the last known-good build target in the meantime.

| | Alpha | Beta Rev2 |
|---|---|---|
| Form factor | Through-hole DevKit carrier, single-layer protoboard | 60×60 mm 4-layer custom PCB |
| MCU | ESP32-S3 DevKit | ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB Octal PSRAM) |
| Motor drivers | External TMC2209 carrier modules | Direct TMC2209 QFN-28 on PCB |
| Control loop | 500 Hz | 400 Hz |
| Status | Legacy — working, last known-good | Legacy — schematic only, not fabricated |

All Alpha/Beta code, PCB projects, and docs now live under `legacy/` subpaths — see [Legacy Hardware Docs](docs/hardware/legacy/) and [Legacy Firmware Docs](docs/firmware/legacy/). Full specs: [Technical Reference — Beta Rev2 (legacy)](docs/references/SixEyes%20Technical%20Reference%20June%202026.md).

## Quick Links

- 📘 **Getting Started**: [Complete Documentation Index](docs/README.md)
- 📐 **v1 Hardware Spec**: [Universal Joint PCB Design Reference](docs/hardware/v1/v1_PCB_Design_Reference.md)
- 🚌 **v1 CAN Protocol** (draft): [CAN Message Protocol](docs/protocols/CAN_MESSAGE_PROTOCOL.md)
- 📋 **v1 Task List**: [docs/V1_TODO.md](docs/V1_TODO.md)
- 🚀 **Deploy Legacy Firmware**: [Flashing & Deployment Guide](docs/deployment/FLASHING_AND_DEPLOYMENT.md)
- 🔌 **Build Legacy Hardware**: [Wiring & Assembly Guide (Alpha)](docs/hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
- 🛠️ **Run Tests**: [Testing & Validation Guide](docs/testing/TESTING_AND_VALIDATION_GUIDE.md)

## Repository Structure

```
firmware/
├── v1/                       ← Current generation — not yet implemented, see docs/V1_TODO.md
└── legacy/
    ├── alpha/
    │   ├── follower_esp32/    env: alpha_follower  (ESP32-S3 DevKit, 500 Hz)
    │   └── leader_esp32/      env: alpha_leader    (ESP32-C6 SuperMini, 100 Hz ADC)
    └── beta/
        ├── follower_esp32/    env: beta_follower   (ESP32-S3-WROOM-1-N16R8, 400 Hz)
        └── leader_esp32/      (shared leader hardware with Alpha)

ros2_ws/                      ← ROS2 workspace (runs on laptop)
├── src/
│   ├── safety_node/          Monitors firmware status, publishes /sixeyes/is_safe
│   ├── vla_inference_node/   VLA inference stub (in development)
│   ├── camera_node/          OpenCV camera → /camera/image_raw
│   ├── joint_state_node/     Bridges /sixeyes/joint_states → /joint_states (rad)
│   └── usb_bridge_node/      Owns serial port; heartbeat TX, telemetry RX, command TX
                               (targets legacy protocol today; needs CAN-relay rework for v1 — see docs/V1_TODO.md)

simulation/                   ← Gazebo simulation
├── models/
└── launch/

hardware_assets/              ← Mechanical + PCB production files
├── 3d_print_stl/             (STL files for printed parts)
├── pcb_project_files/
│   └── legacy/                (Alpha/Beta Follower + Leader KiCad projects)
└── pcb_latest_gerber/         (Gerber files for fabrication)

nodemesh/                     ← NodeMesh subproject (studiosanka/nodemesh)
cyclocad/                     ← CycloCad subproject (studiosanka/cyclocad)

docs/                         ← Documentation ⭐ START HERE
├── README.md                 (doc navigation index)
├── V1_TODO.md                (v1 rework task list)
├── firmware/
│   └── legacy/                (Alpha/Beta firmware architecture docs)
├── hardware/
│   ├── v1/                    (current — Universal Joint PCB spec)
│   └── legacy/
│       ├── alpha/              (Alpha wiring, validation, pinout, PCB design)
│       └── beta/               (Beta wiring, pinout, PCB design)
├── deployment/                (flashing guide, legacy)
├── testing/                   (validation procedures)
├── protocols/
│   ├── CAN_MESSAGE_PROTOCOL.md      (current, draft)
│   └── legacy/
│       └── JSON_MESSAGE_PROTOCOL.md
├── ros2/                      (ROS2 integration)
├── ops/                       (CI/CD pipeline)
└── references/                (datasheets, technical references)

.github/workflows/            ← CI/CD pipelines
├── platformio-build.yml       (builds legacy Alpha/Beta targets)
├── code-quality.yml
└── release.yml

CONTRIBUTING.md
LICENSE
README.md                     (this file)
```

## Getting Started

### For Firmware Developers (Legacy — current working baseline)

v1 firmware doesn't exist yet (see [docs/V1_TODO.md](docs/V1_TODO.md)). Until then, Alpha is the build target that actually works on real hardware.

1. **Clone**:
   ```bash
   git clone https://github.com/studiosanka/sixeyes.git
   ```

2. **Build Alpha firmware**:
   ```bash
   cd firmware/legacy/alpha/follower_esp32
   pio run -e alpha_follower
   pio run -e alpha_follower -t upload
   pio device monitor
   ```

3. **Build Beta firmware** (schematic-only, not fabricated):
   ```bash
   cd firmware/legacy/beta/follower_esp32
   pio run -e beta_follower
   ```

⏭️ **Next**: [Flashing & Deployment Guide](docs/deployment/FLASHING_AND_DEPLOYMENT.md)

### For Hardware Assembly (Legacy — Alpha)

1. **Gather components**: [Parts List](docs/hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md#parts-list)
2. **Wire the hardware**: [Wiring Guide](docs/hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
3. **Validate**: [Hardware Validation](docs/hardware/legacy/alpha/HARDWARE_VALIDATION.md)

⏭️ **Next**: [Wiring & Assembly Guide](docs/hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)

### For ROS2 Integration

```bash
cd ros2_ws
colcon build
source install/setup.bash

ros2 launch sixeyes_bringup vla.launch.py port:=/dev/ttyACM0
```

```bash
ros2 topic echo /sixeyes/firmware_status
ros2 topic echo /sixeyes/is_safe
```

⏭️ **Next**: [ROS2 Integration Guide](docs/ros2/ROS2_INTEGRATION.md)

### For v1 Development

v1 is in the design phase — see [docs/V1_TODO.md](docs/V1_TODO.md) for the current task list and open decisions blocking firmware work (control loop frequency, stall detection strategy, CAN checksum policy, bus-off recovery behavior).

## Communication Protocols

### v1 — CAN Bus (draft, not yet implemented)

One shared CAN bus (TWAI, 1 Mbps) carries E-stop, heartbeat, node status, motor/servo targets, and encoder telemetry between Base and the three joint nodes. Full spec: [CAN Message Protocol](docs/protocols/CAN_MESSAGE_PROTOCOL.md).

### Legacy — ASCII Heartbeat + JSON (Alpha/Beta, still real and working)

**ROS2 → ESP32** (≥50 Hz):
```
HB:0,<sequence>\n
```

**ESP32 → ROS2** (10 Hz):
```
SB:<fault_bitmask>,<motors_enabled>,<ros2_alive>\n
```

```json
{"cmd": "MOTOR_TARGET", "seq": 1, "targets": [0.0, 45.0, 90.0, 135.0]}
```

See [JSON Message Protocol (legacy)](docs/protocols/legacy/JSON_MESSAGE_PROTOCOL.md) for full specification.

## Safety Guarantees

### v1 (draft — see [CAN Message Protocol §5](docs/protocols/CAN_MESSAGE_PROTOCOL.md#5-safety-model--heartbeat-and-e-stop-over-can))

- Two independent timeout domains: bus heartbeat loss (per-node watchdog) and single-node liveness loss (detected by Base)
- E-stop is the highest-priority CAN ID — wins arbitration against any in-flight frame
- Target end-to-end disable latency: ≤2.63 ms, matching the legacy <2.5 ms guarantee

### Legacy (Alpha/Beta, implemented)

- **Auto-disable**: Motors off if ROS2 heartbeat lost for >500 ms
- **Latency**: Motor disable within 1 control loop cycle (~2.5 ms at 400 Hz)
- **Boot state**: Motors disabled by default; `RESET_FAULT` required after any fault
- **Beta DIAG pins**: Open-drain — 47 kΩ pull-ups mandatory on GPIO1/2/3/48
- **Beta forbidden GPIOs**: 26/33/34/35/36/37 committed to octal PSRAM/flash — never route externally

## Key Documentation

### v1 (Current)
- [Universal Joint PCB — Design Reference](docs/hardware/v1/v1_PCB_Design_Reference.md)
- [CAN Message Protocol](docs/protocols/CAN_MESSAGE_PROTOCOL.md) (draft)
- [v1 Rework Task List](docs/V1_TODO.md)

### Legacy — Architecture & Design
- [Technical Reference — Beta Rev2](docs/references/SixEyes%20Technical%20Reference%20June%202026.md)
- [Teleoperation Mode Architecture](docs/firmware/legacy/TELEOPERATION_MODE_ARCHITECTURE.md)
- [Open-Loop Stepper Mitigation Strategies](docs/firmware/legacy/OPEN_LOOP_STEPPER_STRATEGIES.md)

### Legacy — Hardware & Deployment
- [Wiring & Assembly (Alpha)](docs/hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
- [Pinout Matrix (Alpha)](docs/hardware/legacy/alpha/PINOUT_MATRIX.md)
- [Leader PCB Design (Beta)](docs/hardware/legacy/beta/LEADER_PCB_DESIGN.md)
- [Hardware Validation](docs/hardware/legacy/alpha/HARDWARE_VALIDATION.md)
- [Flashing & Deployment](docs/deployment/FLASHING_AND_DEPLOYMENT.md)

### Testing & Quality
- [Testing & Validation Guide](docs/testing/TESTING_AND_VALIDATION_GUIDE.md)
- [CI/CD Pipeline](docs/ops/CI_CD_PIPELINE.md)

### Communication & ROS2
- [JSON Message Protocol (legacy)](docs/protocols/legacy/JSON_MESSAGE_PROTOCOL.md)
- [ROS2 Integration](docs/ros2/ROS2_INTEGRATION.md)

## Project Status

| Component | Status | Notes |
|-----------|--------|-------|
| v1 Hardware Spec | ✅ Drafted | Universal Joint PCB design reference complete |
| v1 CAN Protocol | 🚧 Draft | Open decisions block firmware start — see docs/V1_TODO.md |
| v1 Firmware | ⬜ Not started | `firmware/v1/joint_node/` not yet scaffolded |
| v1 PCB | ⬜ Not started | No KiCad project yet |
| Legacy Firmware — Alpha | ✅ Stable | 500 Hz control loop, safety heartbeat, teleop wired — last known-good |
| Legacy Firmware — Beta | 🚧 Frozen | 400 Hz, DIAG/StallGuard — not being advanced further |
| Legacy Beta PCB | 🚧 Frozen | KiCad, components placed, routing not started — not being fabricated |
| Documentation | ✅ Active | v1 docs current; legacy docs preserved for reference |
| Unit Tests (legacy) | 🟡 Partial | 17 tests pass on Alpha/Beta |
| CI/CD Pipeline | ✅ Complete | Builds legacy Alpha/Beta targets |
| ROS2 Integration | 🟡 Partial | Heartbeat done on legacy protocol; needs CAN-relay rework for v1 |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for code style, PR process, commit conventions, and testing requirements.

## Troubleshooting

- **Compilation issues**: [Build Troubleshooting](docs/deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)
- **Hardware not responding**: [Hardware Validation](docs/hardware/legacy/alpha/HARDWARE_VALIDATION.md#troubleshooting)
- **ROS2 communication problems**: [ROS2 Integration](docs/ros2/ROS2_INTEGRATION.md)

## Licence

SixEyes uses a dual licence:

| | Licence |
|---|---|
| Firmware & software | [Apache 2.0](LICENSE) |
| Hardware design files (KiCad, Gerbers, STL) | [CERN OHL-S v2](hardware_assets/LICENSE_HARDWARE) |

See [CONTRIBUTING.md](CONTRIBUTING.md) for full usage terms, including what is required if you manufacture or sell hardware based on these designs.

## Credits

| Role | Contributor |
|---|---|
| Project Lead | Vincent Santosa ([@markantosa](https://github.com/markantosa)) |
| Firmware & System Architecture | Vincent Santosa ([@markantosa](https://github.com/markantosa)) |
| Follower PCB Design | Vincent Santosa ([@markantosa](https://github.com/markantosa)) |
| Leader PCB Design | Audrey ([@odriyy](https://github.com/odriyy)) |
| Mechanical Design | Ren Jie ([@trenjie03](https://github.com/trenjie03)) |

## Contact & Support

- 🐛 Issues: https://github.com/studiosanka/sixeyes/issues
- 💬 Discussions: https://github.com/studiosanka/sixeyes/discussions

---

**Last Updated**: August 2026 — v1 rework in progress
