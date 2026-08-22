# SixEyes Documentation Index

Navigation guide for SixEyes firmware, ROS2, and hardware documentation.

**Repository**: [studiosanka/sixeyes](https://github.com/studiosanka/sixeyes)
**NodeMesh subproject** (optional): [studiosanka/nodemesh](https://github.com/studiosanka/nodemesh) — targets legacy Alpha/Beta hardware only, not yet compatible with v1

## Scope Boundary

- This docs tree covers SixEyes hardware and firmware workflows: **v1** (current, distributed CAN-node architecture) and **legacy** (Alpha/Beta, retired but kept as reference).
- NodeMesh is optional and maintained in a separate repository. You do not need it for a standard build.

See: [Project Scope And Repo Map](PROJECT_SCOPE_AND_REPO_MAP.md)

## Read This First — v1

v1 replaces Alpha/Beta's leader-follower architecture with one Universal Joint PCB design (ESP32-C6-MINI-1) populated four ways — Base, Shoulder L, Shoulder R, Elbow — on a shared CAN bus. No leader arm; future control input is IMU + inverse kinematics (not yet designed).

1. [Universal Joint PCB — Design Reference](hardware/v1/v1_PCB_Design_Reference.md) — hardware spec
2. [CAN Message Protocol](protocols/CAN_MESSAGE_PROTOCOL.md) — protocol design (draft)
3. [V1_TODO.md](V1_TODO.md) — live task list and open decisions

v1 firmware and PCB layout are not started yet. Everything below this point is either legacy reference material or generation-agnostic (ROS2, testing, CI/CD).

---

## Minimal Start Path (Legacy — last working hardware)

If you want to get a physical arm running today, this is still Alpha:

1. [Wiring & Assembly — Alpha (legacy)](hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
2. [Flashing & Deployment (legacy)](deployment/FLASHING_AND_DEPLOYMENT.md)
3. [Hardware Validation — Alpha (legacy)](hardware/legacy/alpha/HARDWARE_VALIDATION.md)

---

## Quick Start by Role

### 🧑‍💻 v1 Firmware Developers
1. Read: [Universal Joint PCB Design Reference](hardware/v1/v1_PCB_Design_Reference.md)
2. Read: [CAN Message Protocol](protocols/CAN_MESSAGE_PROTOCOL.md) — note the open decisions in §6 blocking implementation
3. Check: [V1_TODO.md](V1_TODO.md) for current status before starting new work

### 🧑‍💻 Legacy Firmware Developers
1. Read: [Teleoperation Mode Architecture (legacy)](firmware/legacy/TELEOPERATION_MODE_ARCHITECTURE.md) — dual-mode design (VLA vs teleop)
2. Follow: [Flashing & Deployment (legacy)](deployment/FLASHING_AND_DEPLOYMENT.md) to build and flash
3. Run: [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md) to verify code

### 🔧 Hardware Integrators — v1
1. Read design spec: [Universal Joint PCB Design Reference](hardware/v1/v1_PCB_Design_Reference.md)
2. Note: no KiCad project exists yet for v1 — see [V1_TODO.md](V1_TODO.md)

### 🔧 Hardware Integrators — Legacy Alpha
1. Gather parts: [Wiring & Assembly — Alpha](hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md#parts-list)
2. Wire: [Wiring & Assembly — Alpha](hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
3. Validate: [Hardware Validation — Alpha](hardware/legacy/alpha/HARDWARE_VALIDATION.md)
4. Flash: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)

### 🔧 Hardware Integrators — Legacy Beta PCB (frozen, not fabricated)
1. Read design spec: [Sixeyes Node0 (Follower PCB Beta).txt](references/Sixeyes%20Node0%20(Folloer%20PCB%20Beta).txt)
2. Reference pinout: [Beta Pinout Matrix](hardware/legacy/beta/PINOUT_MATRIX.md)
3. Reference wiring: [Beta Wiring & Assembly](hardware/legacy/beta/WIRING_AND_ASSEMBLY.md)

### 🤖 ROS2 Engineers
1. Read: [ROS2 Integration](ros2/legacy/ROS2_INTEGRATION.md) — heartbeat protocol, node architecture, integration details (legacy protocol; CAN-relay rework for v1 is pending, see [V1_TODO.md](V1_TODO.md))
2. Reference: [JSON Message Protocol (legacy)](protocols/legacy/JSON_MESSAGE_PROTOCOL.md) for current commands, or [CAN Message Protocol](protocols/CAN_MESSAGE_PROTOCOL.md) for the v1 design
3. Check: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md) for build automation

### 🚀 DevOps / Release Engineers
1. Read: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)
2. Review: [Flashing & Deployment — Release Section](deployment/FLASHING_AND_DEPLOYMENT.md#release--version-management)

---

## Documentation by Category

### 📐 v1 (Current)

#### [Universal Joint PCB — Design Reference](hardware/v1/v1_PCB_Design_Reference.md)
- **Contents**: Board architecture, power design, ESP32-C6-MINI-1 pin map, CAN bus, interboard connectors, full BOM and net list
- **Best For**: Understanding the current hardware generation
- **Status**: Design reference complete; PCB layout not started

#### [CAN Message Protocol](protocols/CAN_MESSAGE_PROTOCOL.md)
- **Contents**: Node addressing, CAN ID allocation, message formats, distributed safety/heartbeat model, E-stop latency budget
- **Best For**: Understanding how v1 nodes communicate and fail safe
- **Status**: Draft — see §6 for decisions blocking firmware implementation

#### [V1_TODO.md](V1_TODO.md)
- **Contents**: Live task list — done, blocked, next up, deferred
- **Best For**: Checking what's actually implemented vs. designed vs. planned

---

### 🎮 Legacy Firmware Architecture & Design

#### [Teleoperation Mode Architecture (legacy)](firmware/legacy/TELEOPERATION_MODE_ARCHITECTURE.md)
- **Contents**: Dual-mode firmware plan, teleoperation protocol, VLA vs teleop comparison, module roadmap
- **Best For**: Understanding how Alpha/Beta's leader/follower interacted — this input model is retired in v1
- **Read Time**: 15 min

#### [Open-Loop Stepper Mitigation Strategies (legacy)](firmware/legacy/OPEN_LOOP_STEPPER_STRATEGIES.md)
- **Contents**: StallGuard homing, drift mitigation for steppers with no shaft encoder
- **Best For**: Understanding Alpha/Beta's open-loop limitations — v1 adds per-joint SPI encoders instead
- **Status**: Legacy — superseded by v1 encoder hardware

---

### 🔌 Legacy Hardware & Assembly

#### [Wiring & Assembly — Alpha](hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
- **Contents**: Parts list, pinout reference, power distribution, motor/servo wiring
- **Best For**: Physical Alpha hardware assembly and electrical integration — the last known-good build
- **Read Time**: 30 min

#### [Hardware Validation — Alpha](hardware/legacy/alpha/HARDWARE_VALIDATION.md)
- **Contents**: Testing procedures, validation checklists, troubleshooting
- **Best For**: Verifying Alpha hardware is correctly assembled
- **Read Time**: 20 min (execution ~2 hours full run)

#### [Pinout Matrix — Alpha](hardware/legacy/alpha/PINOUT_MATRIX.md)
- **Contents**: Consolidated pin map for leader ESP32-C6 SuperMini and follower ESP32-S3, 4× TMC2209 channels, servos, UART links, power rails
- **Best For**: Fast verification of inter-board wiring and GPIO assignments
- **Read Time**: 8 min

#### [Follower PCB Design — Alpha](hardware/legacy/alpha/FOLLOWER_PCB_DESIGN.md) · [Leader PCB Design — Alpha](hardware/legacy/alpha/LEADER_PCB_DESIGN.md)
- **Best For**: Understanding the Alpha board designs

#### [Wiring & Assembly — Beta](hardware/legacy/beta/WIRING_AND_ASSEMBLY.md)
- **Contents**: Beta Rev2 PCB-specific wiring and assembly notes
- **Status**: Frozen — Beta is not being fabricated, superseded by v1

#### [Pinout Matrix — Beta](hardware/legacy/beta/PINOUT_MATRIX.md)
- **Contents**: Beta Rev2 GPIO table (derived from the Node0 design spec)
- **Key Info**: Includes forbidden GPIO list (26/33/34/35/36/37 — octal PSRAM/flash)

---

### 🚀 Firmware Deployment (Legacy)

#### [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
- **Contents**: Build instructions, flashing methods, diagnostics, troubleshooting
- **Best For**: Compiling and installing legacy firmware on ESP32 (Alpha or Beta)
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

#### [CAN Message Protocol](protocols/CAN_MESSAGE_PROTOCOL.md) — Current (v1, draft)
See v1 section above.

#### [JSON Message Protocol (legacy)](protocols/legacy/JSON_MESSAGE_PROTOCOL.md)
- **Contents**: Message format, command types, response types, error handling
- **Best For**: Building ROS2 nodes or custom applications against legacy Alpha/Beta hardware
- **Read Time**: 25 min
- **Key Sections**:
  - Message format (cmd, seq, ts fields)
  - VLA mode commands: `MOTOR_TARGET`, `SERVO_TARGET`, `ENABLE_MOTORS`, `RESET_FAULT`, `HOME_ZERO`, `HOME_STALLGUARD`, `TUNE_PID`
  - Teleop mode: `JOINT_STATE`, `TELEMETRY_STATE`
  - Python/C++ integration examples

---

### 🤖 ROS2 Integration

#### [ROS2 Integration](ros2/legacy/ROS2_INTEGRATION.md)
- **Contents**: Heartbeat protocol, node architecture, integration details, quickstart testing
- **Best For**: Everything ROS2 — safety protocol, node setup, testing
- **Read Time**: 25 min
- **Status**: Documents the legacy protocol integration; `usb_bridge_node` needs a CAN-relay rework for v1 (see [V1_TODO.md](V1_TODO.md))
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
- **Status**: Builds legacy Alpha/Beta targets only; no v1 firmware exists yet to add to CI
- **Key Sections**:
  - Build workflow (PlatformIO, all environments)
  - Code quality workflow
  - Release workflow (versioning, artifact generation)
  - Troubleshooting build failures

---

### 📚 References

| File | Contents |
|---|---|
| [v1 PCB Design Reference](hardware/v1/v1_PCB_Design_Reference.md) | Current authoritative hardware spec (v1 Universal Joint PCB) |
| [Sixeyes Node0 (Follower PCB Beta).txt](references/Sixeyes%20Node0%20(Folloer%20PCB%20Beta).txt) | Beta Rev2 PCB full design specification (legacy) |
| [SixEyes Technical Reference June 2026.md](references/SixEyes%20Technical%20Reference%20June%202026.md) | Technical reference (Beta Rev2, legacy) |
| [tmc2209_datasheet_rev1.09.pdf](references/tmc2209_datasheet_rev1.09.pdf) | TMC2209 official datasheet |

---

## Task-Based Navigation

### "I need to understand v1"
1. [Universal Joint PCB Design Reference](hardware/v1/v1_PCB_Design_Reference.md)
2. [CAN Message Protocol](protocols/CAN_MESSAGE_PROTOCOL.md)
3. [V1_TODO.md](V1_TODO.md)

### "I need to build a working arm today" (legacy)
1. [Wiring & Assembly — Alpha](hardware/legacy/alpha/WIRING_AND_ASSEMBLY.md)
2. [Hardware Validation — Alpha](hardware/legacy/alpha/HARDWARE_VALIDATION.md)
3. [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)

### "I need to understand the legacy Beta PCB design"
1. [Sixeyes Node0 (Follower PCB Beta).txt](references/Sixeyes%20Node0%20(Folloer%20PCB%20Beta).txt) (authoritative spec, legacy)
2. [Beta Pinout Matrix](hardware/legacy/beta/PINOUT_MATRIX.md)
3. [Beta Wiring & Assembly](hardware/legacy/beta/WIRING_AND_ASSEMBLY.md)

### "I need to understand how the legacy firmware works"
1. [Teleoperation Mode Architecture](firmware/legacy/TELEOPERATION_MODE_ARCHITECTURE.md)
2. [JSON Message Protocol](protocols/legacy/JSON_MESSAGE_PROTOCOL.md)
3. [ROS2 Integration](ros2/legacy/ROS2_INTEGRATION.md)

### "I need to set up ROS2"
1. [ROS2 Integration](ros2/legacy/ROS2_INTEGRATION.md)
2. [JSON Message Protocol (legacy)](protocols/legacy/JSON_MESSAGE_PROTOCOL.md) or [CAN Message Protocol (v1, draft)](protocols/CAN_MESSAGE_PROTOCOL.md)

### "I need to run the tests"
1. [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md)
2. [Hardware Validation — Alpha](hardware/legacy/alpha/HARDWARE_VALIDATION.md)
3. [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)

### "I need to deploy firmware"
1. [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
2. [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)

### "Something's broken"
1. [Flashing & Deployment — Troubleshooting](deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)
2. [Hardware Validation — Troubleshooting](hardware/legacy/alpha/HARDWARE_VALIDATION.md#troubleshooting)
3. [ROS2 Integration — Troubleshooting](ros2/legacy/ROS2_INTEGRATION.md#troubleshooting)

---

## Documentation Map

```
docs/
├── README.md                          (this file — navigation index)
├── PROJECT_SCOPE_AND_REPO_MAP.md      (canonical repo split and scope boundaries)
├── V1_TODO.md                         (v1 rework task list)
├── firmware/
│   └── legacy/
│       ├── TELEOPERATION_MODE_ARCHITECTURE.md
│       └── OPEN_LOOP_STEPPER_STRATEGIES.md
├── hardware/
│   ├── v1/
│   │   └── v1_PCB_Design_Reference.md (current authoritative hardware spec)
│   └── legacy/
│       ├── alpha/
│       │   ├── WIRING_AND_ASSEMBLY.md     (Alpha parts list, pinout, wiring)
│       │   ├── HARDWARE_VALIDATION.md     (Alpha test procedures)
│       │   ├── PINOUT_MATRIX.md           (Alpha leader + follower GPIO map)
│       │   ├── FOLLOWER_PCB_DESIGN.md
│       │   └── LEADER_PCB_DESIGN.md
│       └── beta/
│           ├── WIRING_AND_ASSEMBLY.md     (Beta Rev2 PCB wiring)
│           └── PINOUT_MATRIX.md           (Beta GPIO map, forbidden pins)
├── deployment/
│   └── FLASHING_AND_DEPLOYMENT.md
├── testing/
│   └── TESTING_AND_VALIDATION_GUIDE.md
├── protocols/
│   ├── CAN_MESSAGE_PROTOCOL.md        (current, draft)
│   └── legacy/
│       └── JSON_MESSAGE_PROTOCOL.md
├── ros2/
│   └── legacy/
│       └── ROS2_INTEGRATION.md
├── ops/
│   └── CI_CD_PIPELINE.md
└── references/
    ├── Sixeyes Node0 (Folloer PCB Beta).txt   (Beta Rev2 design spec, legacy)
    ├── SixEyes Technical Reference June 2026.md
    └── tmc2209_datasheet_rev1.09.pdf
```

---

## Version History

| Date | Changes |
|------|---------|
| Aug 2026 | v1 rework: Alpha/Beta moved to legacy/, CAN protocol drafted, docs restructured around v1 as primary generation |
| Jun 2026 | Updated for Alpha/Beta split; fixed all file paths; added Beta hardware docs and references; fixed GitHub org to studiosanka |
| Feb 2026 | Documentation reorganised into docs/ directory structure |
| Jan 2026 | Initial documentation release |

---

**Last Updated**: August 2026
**Status**: v1 in design phase (hardware spec + CAN protocol drafted, firmware/PCB not started); legacy Alpha/Beta preserved as reference, Alpha remains last known-good hardware
