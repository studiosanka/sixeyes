# SixEyes Documentation Index

Navigation guide for SixEyes firmware, ROS2, teleoperation, and hardware documentation.

## Quick Start

👉 **New to SixEyes?** Start here based on your role:

### 🧑‍💻 Firmware Developers
1. Read: [Implementation Summary](firmware/IMPLEMENTATION_SUMMARY.md) (5 min)
2. Read: [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md) (10 min)
3. Read: [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md) (dual-mode design)
4. Follow: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md) to build firmware
5. Run: [Unit Tests](testing/TESTING_AND_VALIDATION_GUIDE.md) to verify code

### 🎮 Teleoperation Developers
1. Read: [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
2. Reference: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md#teleoperation-streaming-messages-phase-2)
3. Build: `sixeyes/firmware/leader_esp32` (joint-state streamer)
4. Run bridge: `python sixeyes/tools/teleoperation_bridge.py --leader-port COM5 --follower-port COM6`
5. Validate: `JOINT_STATE` forwarding + optional JSONL logging
6. Optional: run `python sixeyes/tools/operator_control.py --port COM6 teleop-ready` for follower-side safety/enable command flow

### 🔧 Hardware Integrators
1. Gather: [Parts List](hardware/WIRING_AND_ASSEMBLY.md#parts-list)
2. Follow: [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md) step-by-step
3. Run: [Hardware Validation Tests](hardware/HARDWARE_VALIDATION.md) to verify setup
4. Deploy: [Flashing Guide](deployment/FLASHING_AND_DEPLOYMENT.md) to install firmware

### 🤖 ROS2 Engineers
1. Read: [ROS2 Heartbeat Integration](ros2/ROS2_HEARTBEAT_INTEGRATION.md) (understand protocol)
2. Follow: [ROS2 Quickstart](ros2/ROS2_HEARTBEAT_QUICKSTART.md) for testing
3. Reference: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md) for commands
4. Check: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md) for build automation

### 🚀 DevOps / Release Engineers
1. Read: [CI/CD Pipeline Documentation](ops/CI_CD_PIPELINE.md)
2. Review: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md#release--version-management)
3. Check: [GitHub Workflows](.github/workflows/) for automation details

---

## Documentation by Category

### 📘 Firmware Architecture & Design

#### [Implementation Summary](firmware/IMPLEMENTATION_SUMMARY.md)
- **Contents**: ROS2 heartbeat protocol, module specifications, compilation results
- **Best For**: Understanding core firmware architecture, protocol details
- **Read Time**: 20 minutes
- **Key Sections**:
  - Bidirectional heartbeat protocol (HB: and SB: packets)
  - Four firmware modules (HeartbeatReceiver, SafetyBridge, HeartbeatTransmitter, SafetyNodeBridge)
  - SafetyTask integration with ROS2 hooks
  - Performance metrics (6.1% RAM, 10% Flash)

#### [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md)
- **Contents**: System diagrams, control flow, timing analysis, pin mapping
- **Best For**: Visual learners, understanding data flow, timing guarantees
- **Read Time**: 15 minutes
- **Key Sections**:
  - High-level system architecture diagram
  - 400 Hz control loop timeline (2.5 ms per cycle breakdown)
  - Module dependency graph
  - Hardware pin mapping and configuration
  - Signal flow diagrams

#### [Teleoperation Mode Architecture](firmware/TELEOPERATION_MODE_ARCHITECTURE.md)
- **Contents**: Dual-mode firmware plan, teleoperation protocol, module roadmap
- **Best For**: Implementing leader-follower mirroring and data-collection flow
- **Read Time**: 15 minutes
- **Key Sections**:
  - VLA Inference vs Teleoperation mode comparison
  - `JOINT_STATE` and `TELEMETRY_STATE` data-flow design
  - Phase-based implementation plan (Phase 1 → Phase 3)

---

### 🔌 Hardware & Assembly

#### [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md)
- **Contents**: Parts list, pinout reference, power distribution, motor/servo wiring
- **Best For**: Physical hardware assembly, electrical integration
- **Read Time**: 30 minutes
- **Key Sections**:
  - Complete parts list with part numbers
  - ESP32-S3 pin assignments (all pins documented)
  - TMC2209 stepper driver wiring
  - Servo motor connections (LEDC PWM)
  - Power distribution strategy (star grounding)
  - Grounding and noise management

#### [Dual-Controller Pinout & Wiring Matrix](hardware/DUAL_CONTROLLER_PINOUT_MATRIX.md)
- **Contents**: Consolidated pin map for leader ESP32-C6 SuperMini and follower ESP32-S3, 4× TMC2209 channels, servos, UART links, and power rails
- **Best For**: Fast verification of inter-board wiring and GPIO assignments against Technical Reference 2
- **Read Time**: 8 minutes
- **Key Sections**:
  - Leader ADC and UART pin mapping
  - Follower TMC2209 + servo pin mapping
  - Inter-board UART cross-link table
  - Power rail distribution and grounding rules

#### [Hardware Validation](hardware/HARDWARE_VALIDATION.md)
- **Contents**: Testing procedures, validation checklists, troubleshooting
- **Best For**: Verifying hardware is correctly assembled
- **Read Time**: 20 minutes (execution time ~2 hours for full validation)
- **Key Sections**:
  - Pre-testing checklists (power, connections, firmware)
  - Unit-level tests (power-on, GPIO blink, UART loopback)
  - Integration tests (motor enable/disable, heartbeat timeout, servo response)
  - System-level tests (load testing, stress testing)
  - Failure mode testing and recovery

---

### 🚀 Firmware Deployment

#### [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
- **Contents**: Build instructions, flashing methods, diagnostics, troubleshooting
- **Best For**: Compiling and installing firmware on ESP32
- **Read Time**: 25 minutes (execution time ~10 minutes per flash)
- **Key Sections**:
  - Pre-deployment checklist (tools, hardware prep, prerequisites)
  - Firmware build with PlatformIO (CLI and VS Code)
  - Multiple flashing methods (USB in DFU mode, UART serial)
  - Serial monitor diagnostics and startup verification
  - Troubleshooting common build and flash errors
  - Release and version management

---

### 🧪 Testing & Quality Assurance

#### [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md)
- **Contents**: Unit test framework, running tests, coverage, simulation
- **Best For**: Developers writing and running unit tests
- **Read Time**: 20 minutes
- **Key Sections**:
  - Unit test framework architecture (mock hardware objects)
  - MockSerial, MockGPIO, MockTimer, MockFreeRTOS
  - Running tests on desktop (no ESP32 required)
  - Test coverage analysis (heartbeat parsing, safety logic, motor control)
  - Simulation harness for non-blocking operations
  - CI/CD integration with GitHub Actions

---

### 🛠️ Communication Protocols

#### [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)
- **Contents**: Message format, command types, response types, error handling
- **Best For**: Building ROS2 nodes or custom applications
- **Read Time**: 25 minutes
- **Key Sections**:
  - Protocol overview (extensible, non-blocking, type-safe)
  - Message format with common fields (cmd, seq, ts)
  - Command and response message reference (implemented + planned)
  - Examples for each message type
  - Error handling and edge cases
  - Integration examples with Python/C++

---

### 🤖 ROS2 Integration

#### [ROS2 Heartbeat Integration](ros2/ROS2_HEARTBEAT_INTEGRATION.md)
- **Contents**: Heartbeat protocol, module breakdown, architecture, integration details
- **Best For**: Understanding heart beat safety mechanism
- **Read Time**: 20 minutes
- **Key Sections**:
  - Bidirectional heartbeat protocol (HB and SB packets)
  - Safety guarantees (dual heartbeat monitoring, <2.5 ms disable latency)
  - Module architecture (HeartbeatReceiver, Monitor, Transmitter)
  - Integration with SafetyTask and motor control
  - Timing and latency analysis

#### [ROS2 Quickstart Guide](ros2/ROS2_HEARTBEAT_QUICKSTART.md)
- **Contents**: Step-by-step testing, hardware setup, troubleshooting
- **Best For**: Quick hands-on testing of ROS2 integration
- **Read Time**: 15 minutes (testing ~30 minutes)
- **Key Sections**:
  - Quick verification test (2 minutes)
  - Full integration test (15 minutes)
  - Python node examples with diagnostics
  - Stress testing and failure injection
  - Common issues and solutions

---

### ⚙️ Operations & CI/CD

#### [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)
- **Contents**: GitHub Actions workflows, build configuration, testing automation
- **Best For**: DevOps engineers, release automation
- **Read Time**: 20 minutes
- **Key Sections**:
  - Pipeline architecture overview
  - Build workflow (PlatformIO compilation, memory constraints)
  - Code quality workflow (static analysis, linting)
  - Release workflow (versioning, artifact generation)
  - Configuration customization
  - Troubleshooting build failures

---

### 📚 References & Resources

#### [References Directory](references/)
- **Contents**: Datasheets, technical specifications, hardware manuals
- **Best For**: Looking up chip specifications, pinouts, electrical specs
- **Files**:
  - `FIRMWARE_SYSTEM_DATASHEET.md` - Firmware-level system datasheet (interfaces, limits, safety behavior, maturity)
  - `SixEyes Technical Reference.txt` - Hardware technical specifications

---

## Documentation Map by Task

### Task: "I need to build the hardware"
1. Start: [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md)
2. Verify: [Hardware Validation](hardware/HARDWARE_VALIDATION.md)
3. Install: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)

### Task: "I need to understand how it works"
1. Start: [Implementation Summary](firmware/IMPLEMENTATION_SUMMARY.md)
2. Visualize: [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md)
3. Deep dive: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)

### Task: "I need to set up ROS2"
1. Start: [ROS2 Heartbeat Integration](ros2/ROS2_HEARTBEAT_INTEGRATION.md)
2. Test: [ROS2 Quickstart](ros2/ROS2_HEARTBEAT_QUICKSTART.md)
3. Command: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)

### Task: "I need to run the tests"
1. Start: [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md)
2. Hardware: [Hardware Validation](hardware/HARDWARE_VALIDATION.md)
3. CI/CD: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)

### Task: "I need to deploy a new version"
1. Start: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md)
2. CI/CD: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md)
3. Release: [Flashing Guide - Release Section](deployment/FLASHING_AND_DEPLOYMENT.md#release--version-management)

### Task: "Something's broken, how do I fix it?"
1. Check: [Flashing & Deployment - Troubleshooting](deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)
2. Or: [Hardware Validation - Troubleshooting](hardware/HARDWARE_VALIDATION.md#troubleshooting)
3. Or: [ROS2 Quickstart - Troubleshooting](ros2/ROS2_HEARTBEAT_QUICKSTART.md#troubleshooting)

---

## Document Statistics

| Category | Document | Pages | Read Time |
|----------|----------|-------|-----------|
| Firmware | Implementation Summary | 20 | 20 min |
| Firmware | Visual Architecture | 35 | 15 min |
| Hardware | Wiring & Assembly | 40 | 30 min |
| Hardware | Hardware Validation | 22 | 20 min |
| Deployment | Flashing & Deployment | 42 | 25 min |
| Testing | Testing & Validation | 24 | 20 min |
| Protocols | JSON Message Protocol | 34 | 25 min |
| ROS2 | Heartbeat Integration | 15 | 20 min |
| ROS2 | Quickstart Guide | 20 | 15 min |
| Ops | CI/CD Pipeline | 24 | 20 min |
| **Total** | **10 Documents** | **276** | **190 min** |

---

## Navigation Tips

### By File Size (Least to Most Reading)
1. ⚡ Quick (5-10 min): ROS2 Quickstart, CI/CD Pipeline intro
2. 📖 Medium (15-20 min): Implementation Summary, Hardware Validation
3. 📚 Deep Dive (25-30 min): Wiring & Assembly, Flashing & Deployment, JSON Protocol

### By Priority
1. 🔴 **Critical** (must read): Implementation Summary, Wiring & Assembly, Flashing & Deployment
2. 🟡 **Important** (should read): Hardware Validation, ROS2 Integration, Visual Architecture
3. 🟢 **Reference** (look up as needed): JSON Protocol, CI/CD Pipeline, Testing Guide

### By Audience
- **Hardware Teams**: Wiring & Assembly → Hardware Validation → Flashing
- **Firmware Teams**: Implementation Summary → Visual Architecture → Testing & Validation
- **ROS2 Teams**: ROS2 Integration → JSON Protocol → ROS2 Quickstart
- **DevOps**: CI/CD Pipeline → Flashing (Release section)

---

## Cross-Reference Guide

### Protocol Details
- **Heartbeat packets**: [Implementation Summary](firmware/IMPLEMENTATION_SUMMARY.md#protocol-specification)
- **JSON messages**: [JSON Message Protocol](protocols/JSON_MESSAGE_PROTOCOL.md)
- **Timing specs**: [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md#400-hz-control-loop-timing)
- **Safety guarantees**: [ROS2 Integration](ros2/ROS2_HEARTBEAT_INTEGRATION.md#safety-guarantees)

### Hardware Details
- **Pin assignments**: [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md#hardware-pin-mapping)
- **Wiring diagrams**: [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md#pinout-reference)
- **Motor specs**: [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md#parts-list)
- **Power distribution**: [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md#power-distribution)

### Testing & Validation
- **Unit tests**: [Testing Guide](testing/TESTING_AND_VALIDATION_GUIDE.md#unit-test-framework)
- **Hardware tests**: [Hardware Validation](hardware/HARDWARE_VALIDATION.md)
- **Integration tests**: [ROS2 Quickstart](ros2/ROS2_HEARTBEAT_QUICKSTART.md)
- **CI/CD tests**: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md#build-workflow)

### Build & Deployment
- **Compilation**: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md#firmware-build--compilation)
- **Flashing**: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md#flashing-methods)
- **Troubleshooting**: [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md#troubleshooting)
- **Release process**: [CI/CD Pipeline](ops/CI_CD_PIPELINE.md#release-workflow)

---

## Getting Help

### Common Questions

**Q: Where do I start?**
A: See the Quick Start section above for your role.

**Q: How do I understand the system architecture?**
A: Read [Implementation Summary](firmware/IMPLEMENTATION_SUMMARY.md) first, then [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md).

**Q: How do I wire the hardware?**
A: Follow [Wiring & Assembly](hardware/WIRING_AND_ASSEMBLY.md) step-by-step, using the pin maps in [Visual Architecture Guide](firmware/VISUAL_ARCHITECTURE_GUIDE.md).

**Q: How do I test my hardware?**
A: Run the procedures in [Hardware Validation](hardware/HARDWARE_VALIDATION.md).

**Q: How do I flash the firmware?**
A: Follow [Flashing & Deployment](deployment/FLASHING_AND_DEPLOYMENT.md).

**Q: How do I integrate with ROS2?**
A: Read [ROS2 Heartbeat Integration](ros2/ROS2_HEARTBEAT_INTEGRATION.md), then follow [ROS2 Quickstart](ros2/ROS2_HEARTBEAT_QUICKSTART.md).

**Q: How do I run tests?**
A: See [Testing & Validation Guide](testing/TESTING_AND_VALIDATION_GUIDE.md).

**Q: How do I set up CI/CD?**
A: See [CI/CD Pipeline](ops/CI_CD_PIPELINE.md).

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| Feb 2026 | 2.0 | Documentation reorganized into docs/ directory structure |
| Jan 2026 | 1.0 | Initial documentation release |

---

**Last Updated**: February 2026  
**Documentation Version**: 2.0  
**Status**: 🚧 Active and evolving with ongoing teleoperation + ROS2 updates
