# SixEyes Documentation Index

Navigation guide for SixEyes firmware, ROS2, teleoperation, and hardware documentation.

**Repository**: [studiosanka/sixeyes](https://github.com/studiosanka/sixeyes)  
**NodeMesh subproject** (optional): [studiosanka/nodemesh](https://github.com/studiosanka/nodemesh)

## Scope Boundary

- This docs tree covers standard SixEyes hardware and firmware workflows (Alpha and Beta).
- NodeMesh is optional and maintained in a separate repository. You do not need it for a standard build.

See: [Project Scope And Repo Map](PROJECT_SCOPE_AND_REPO_MAP.md)

## Minimal Start Path

If you want to get running with as little reading as possible:

1. [Wiring & Assembly — Alpha](hardware/alpha/WIRING_AND_ASSEMBLY.md)
2. [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
3. [Hardware Validation — Alpha](hardware/alpha/HARDWARE_VALIDATION.md)

Then expand to other docs only as needed.

---

## Quick Start by Role

### 🧑‍💻 Firmware Developers
1. Read: [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md) — dual-mode design (VLA vs teleop)
2. Follow: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md) to build and flash
3. Run: [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md) to verify code

### 🎮 Teleoperation Developers
1. Read: [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
2. Reference: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md#teleoperation-streaming-messages-phase-2)
3. Build: `firmware/alpha/leader_esp32` (joint-state streamer, env: `alpha_leader`)
4. Run bridge: `python tools/teleoperation_bridge.py --leader-port COM5 --follower-port COM6`
5. Validate: `JOINT_STATE` forwarding + optional JSONL logging
6. Optional: `python tools/operator_control.py --port COM6 teleop-ready`

### 🔧 Hardware Integrators — Alpha
1. Gather parts: [Wiring & Assembly — Alpha](hardware/alpha/WIRING_AND_ASSEMBLY.md#parts-list)
2. Wire: [Wiring & Assembly — Alpha](hardware/alpha/WIRING_AND_ASSEMBLY.md)
3. Validate: [Hardware Validation — Alpha](hardware/alpha/HARDWARE_VALIDATION.md)
4. Flash: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)

### 🔧 Hardware Integrators — Beta PCB
1. Read design spec: [Sixeyes Node0 (Follower PCB Beta).txt](references/Sixeyes%20Node0%20(Folloer%20PCB%20Beta).txt) or [SixEyes_Node0.pdf](references/SixEyes_Node0.pdf)
2. Reference pinout: [Beta Pinout Matrix](hardware/beta/PINOUT_MATRIX.md)
3. Reference wiring: [Beta Wiring & Assembly](hardware/beta/WIRING_AND_ASSEMBLY.md)

### 🤖 ROS2 Engineers
1. Read: [ROS2 Integration](ros2/ROS2_INTEGRATION.md) — heartbeat protocol, node architecture, integration details
2. Reference: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md) for commands
3. Check: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md) for build automation

### 🚀 DevOps / Release Engineers
1. Read: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)
2. Review: [Flashing & Deployment — Release Section](deployment/FLASHING_AND_DEPLOYMENT.md#release--version-management)

---

## Documentation by Category

### 🎮 Firmware Architecture & Design

#### [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
- **Contents**: Dual-mode firmware plan, teleoperation protocol, VLA vs teleop comparison, module roadmap
- **Best For**: Understanding the two operating modes and how leader/follower interact
- **Read Time**: 15 min
- **Key Sections**:
  - VLA Inference vs Teleoperation mode comparison
  - `JOINT_STATE` and `TELEMETRY_STATE` data-flow design
  - Phase-based implementation plan

---

### 🔌 Hardware & Assembly

#### [Wiring & Assembly — Alpha](hardware/alpha/WIRING_AND_ASSEMBLY.md)
- **Contents**: Parts list, pinout reference, power distribution, motor/servo wiring
- **Best For**: Physical Alpha hardware assembly and electrical integration
- **Read Time**: 30 min
- **Key Sections**:
  - Complete parts list with part numbers
  - ESP32-S3 DevKit pin assignments
  - TMC2209 module wiring
  - Servo connections (LEDC PWM)
  - Power distribution and star grounding

#### [Hardware Validation — Alpha](hardware/alpha/HARDWARE_VALIDATION.md)
- **Contents**: Testing procedures, validation checklists, troubleshooting
- **Best For**: Verifying Alpha hardware is correctly assembled
- **Read Time**: 20 min (execution ~2 hours full run)
- **Key Sections**:
  - Pre-testing checklists
  - Unit-level tests (power-on, GPIO blink, UART loopback)
  - Integration tests (heartbeat timeout, servo response)
  - Failure mode testing and recovery

#### [Pinout Matrix — Alpha](hardware/alpha/PINOUT_MATRIX.md)
- **Contents**: Consolidated pin map for leader ESP32-C6 SuperMini and follower ESP32-S3, 4× TMC2209 channels, servos, UART links, power rails
- **Best For**: Fast verification of inter-board wiring and GPIO assignments
- **Read Time**: 8 min

#### [Follower PCB Design — Alpha](hardware/alpha/FOLLOWER_PCB_DESIGN.md)
- **Contents**: Alpha follower board design details
- **Best For**: Understanding the Alpha follower hardware design

#### [Leader PCB Design — Alpha](hardware/alpha/LEADER_PCB_DESIGN.md)
- **Contents**: Alpha leader board design details
- **Best For**: Understanding the Alpha leader hardware design

#### [Wiring & Assembly — Beta](hardware/beta/WIRING_AND_ASSEMBLY.md)
- **Contents**: Beta Rev2 PCB-specific wiring and assembly notes
- **Best For**: Beta PCB bring-up (when hardware is fabricated)
- **Status**: 🚧 In progress alongside PCB design

#### [Pinout Matrix — Beta](hardware/beta/PINOUT_MATRIX.md)
- **Contents**: Beta Rev2 GPIO table (derived from SixEyes_Node0.pdf design spec)
- **Best For**: Beta firmware development, schematic cross-checking
- **Key Info**: Includes forbidden GPIO list (26/33/34/35/36/37 — octal PSRAM/flash)

---

### 🚀 Firmware Deployment

#### [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
- **Contents**: Build instructions, flashing methods, diagnostics, troubleshooting
- **Best For**: Compiling and installing firmware on ESP32 (Alpha or Beta)
- **Read Time**: 25 min
- **Key Sections**:
  - Pre-deployment checklist
  - PlatformIO build (`alpha_follower`, `beta_follower`, `alpha_leader` environments)
  - Flashing via USB
  - Serial monitor diagnostics
  - Troubleshooting common build and flash errors

---

### 🧪 Testing & Quality Assurance

#### [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md)
- **Contents**: Unit test framework, running tests, coverage, simulation
- **Best For**: Developers writing and running unit tests
- **Read Time**: 20 min
- **Key Sections**:
  - Unit test framework (mock hardware objects: MockSerial, MockGPIO, MockTimer)
  - Running tests on desktop — no ESP32 required
  - CI/CD integration with GitHub Actions

---

### 🛠️ Communication Protocols

#### [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)
- **Contents**: Message format, command types, response types, error handling
- **Best For**: Building ROS2 nodes or custom applications
- **Read Time**: 25 min
- **Key Sections**:
  - Message format (cmd, seq, ts fields)
  - VLA mode commands: `MOTOR_TARGET`, `SERVO_TARGET`, `ENABLE_MOTORS`, `RESET_FAULT`, `HOME_ZERO`, `HOME_STALLGUARD`, `TUNE_PID`
  - Teleop mode: `JOINT_STATE`, `TELEMETRY_STATE`
  - Python/C++ integration examples

---

### 🤖 ROS2 Integration

#### [ROS2 Integration](ros2/ROS2_INTEGRATION.md)
- **Contents**: Heartbeat protocol, node architecture, integration details, quickstart testing
- **Best For**: Everything ROS2 — safety protocol, node setup, testing
- **Read Time**: 25 min
- **Key Sections**:
  - Bidirectional heartbeat protocol (HB: and SB: packets)
  - Safety guarantees (<2.5 ms disable latency, 500 ms timeout)
  - Node architecture (`usb_bridge_node`, `safety_node`, `joint_state_node`, `camera_node`)
  - Step-by-step integration testing
  - Common issues and solutions

---

### ⚙️ Operations & CI/CD

#### [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)
- **Contents**: GitHub Actions workflows, build configuration, testing automation
- **Best For**: DevOps engineers, release automation
- **Read Time**: 20 min
- **Key Sections**:
  - Build workflow (PlatformIO, all environments)
  - Code quality workflow
  - Release workflow (versioning, artifact generation)
  - Troubleshooting build failures

---

### 📚 References

| File | Contents |
|---|---|
| [SixEyes_Node0.pdf](references/SixEyes_Node0.pdf) | Beta Rev2 PCB full design specification (authoritative) |
| [Sixeyes Node0 (Follower PCB Beta).txt](references/Sixeyes%20Node0%20(Folloer%20PCB%20Beta).txt) | Beta Rev2 design spec — LaTeX source / text form |
| [SixEyes Technical Reference.txt](references/SixEyes%20Technical%20Reference.txt) | Alpha hardware technical specifications |
| [SixEyes Technical Reference 2.txt](references/SixEyes%20Technical%20Reference%202.txt) | Extended technical reference |
| [tmc2209_datasheet_rev1.09.pdf](references/tmc2209_datasheet_rev1.09.pdf) | TMC2209 official datasheet |

---

## Task-Based Navigation

### "I need to build Alpha hardware"
1. [Wiring & Assembly — Alpha](hardware/alpha/WIRING_AND_ASSEMBLY.md)
2. [Hardware Validation — Alpha](hardware/alpha/HARDWARE_VALIDATION.md)
3. [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)

### "I need to understand the Beta PCB design"
1. [SixEyes_Node0.pdf](references/SixEyes_Node0.pdf) (authoritative spec)
2. [Beta Pinout Matrix](hardware/beta/PINOUT_MATRIX.md)
3. [Beta Wiring & Assembly](hardware/beta/WIRING_AND_ASSEMBLY.md)

### "I need to understand how the firmware works"
1. [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
2. [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)
3. [ROS2 Integration](ros2/ROS2_INTEGRATION.md)

### "I need to set up ROS2"
1. [ROS2 Integration](ros2/ROS2_INTEGRATION.md)
2. [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)

### "I need to run the tests"
1. [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md)
2. [Hardware Validation — Alpha](hardware/alpha/HARDWARE_VALIDATION.md)
3. [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)

### "I need to deploy firmware"
1. [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
2. [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)

### "Something's broken"
1. [Flashing & Deployment — Troubleshooting](deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)
2. [Hardware Validation — Troubleshooting](hardware/alpha/HARDWARE_VALIDATION.md#troubleshooting)
3. [ROS2 Integration — Troubleshooting](ros2/ROS2_INTEGRATION.md#troubleshooting)

---

## Documentation Map

```
docs/
├── README.md                          (this file — navigation index)
├── PROJECT_SCOPE_AND_REPO_MAP.md      (canonical repo split and scope boundaries)
├── firmware/
│   └── TELEOPERATION_MODE_ARCHITECTURE.md
├── hardware/
│   ├── alpha/
│   │   ├── WIRING_AND_ASSEMBLY.md     (Alpha parts list, pinout, wiring)
│   │   ├── HARDWARE_VALIDATION.md     (Alpha test procedures)
│   │   ├── PINOUT_MATRIX.md           (Alpha leader + follower GPIO map)
│   │   ├── FOLLOWER_PCB_DESIGN.md
│   │   └── LEADER_PCB_DESIGN.md
│   └── beta/
│       ├── WIRING_AND_ASSEMBLY.md     (Beta Rev2 PCB wiring — in progress)
│       └── PINOUT_MATRIX.md           (Beta GPIO map, forbidden pins)
├── deployment/
│   └── FLASHING_AND_DEPLOYMENT.md
├── testing/
│   └── TESTING_AND_VALIDATION_GUIDE.md
├── protocols/
│   └── JSON_MESSAGE_PROTOCOL.md
├── ros2/
│   └── ROS2_INTEGRATION.md
├── ops/
│   └── CI_CD_PIPELINE.md
└── references/
    ├── SixEyes_Node0.pdf              (Beta Rev2 design spec — authoritative)
    ├── Sixeyes Node0 (Folloer PCB Beta).txt
    ├── SixEyes Technical Reference.txt
    ├── SixEyes Technical Reference 2.txt
    └── tmc2209_datasheet_rev1.09.pdf
```

---

## Version History

| Date | Changes |
|------|---------|
| Jun 2026 | Updated for Alpha/Beta split; fixed all file paths; added Beta hardware docs and references; fixed GitHub org to studiosanka |
| Feb 2026 | Documentation reorganised into docs/ directory structure |
| Jan 2026 | Initial documentation release |

---

**Last Updated**: June 2026  
**Status**: Active — Beta PCB design in progress; Alpha firmware and docs stable
