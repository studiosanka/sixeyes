# SixEyes Teleoperation Mode - Firmware Architecture

**Status**: Active architecture + implementation guide for teleoperation firmware mode (Leader-Follower-Laptop)  
**Current scope**: Dual-mode firmware is implemented with compile-time mode selection (`OPERATION_MODE`) and active teleoperation command path updates  
**Next phase**: Complete follower telemetry/state streaming and end-to-end ROS2 teleoperation runtime

---

## Executive Summary

### Current Implementation (VLA Inference Mode)
```
Laptop (ROS2 VLA Node)
        ↓
    Heartbeat (ASCII)
    Commands (JSON)
        ↓
Follower ESP32
        ↓
Motor Control (PID)
```

### Teleoperation Mode (Current Direction)
```
Leader Arm (GPIO/Sensor/Potentiometers)
        ↓
Leader ESP32 (record human motions)
        ↓ USB-CDC
Laptop (Optional: logging, replay, analysis)
        ↓ USB-CDC + WiFi (wireless optional)
Follower ESP32 (replay recorded motions)
        ↓
Follower Arm (mirror motions)
```

---

## Architecture Overview

### Two-Mode Firmware Design

```
┌─────────────────────────────────────────────────────────┐
│          Follower ESP32 Firmware (Dual-Mode)            │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  Configuration Layer                                     │
│  ├─ OPERATION_MODE setting (build-time)                 │
│  ├─ Either VLA_INFERENCE or TELEOPERATION               │
│  └─ Mode selected per firmware build                    │
│                                                           │
│  ┌─ VLA Inference Mode (Current) ──────────────────┐    │
│  │ ├─ Heartbeat RX (ASCII: HB:0,N)                │    │
│  │ ├─ Command RX (JSON: motor positions)          │    │
│  │ ├─ Status TX (SB: fault state)                 │    │
│  │ └─ Control Loop: Apply target positions        │    │
│  └──────────────────────────────────────────────────┘    │
│                                                           │
│  ┌─ Teleoperation Mode (In Progress) ──────────────┐    │
│  │ ├─ Joint State RX (JSON: leader positions)     │    │
│  │ ├─ Telemetry TX (JSON: follower positions)     │    │
│  │ ├─ Acknowledgment (confirms receipt)            │    │
│  │ └─ Control Loop: Mirror leader positions       │    │
│  └──────────────────────────────────────────────────┘    │
│                                                           │
│  Common Control Loop (400 Hz FreeRTOS)                  │
│  ├─ Safety Monitor (always active)                      │
│  ├─ Motor Controller (PID for both modes)               │
│  ├─ Servo Manager (same for both modes)                │
│  └─ Telemetry Collector (collects feedback)             │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

---

## Mode Comparison

| Feature | VLA Inference | Teleoperation |
|---------|---------------|---------------|
| **Leader Hardware** | None (laptop planner) | Potentiometers/Encoders (sensors) |
| **Leader Firmware** | None | ESP32 (sensor reading) |
| **Primary Input** | ROS2 AI predictions | Leader joint positions |
| **Input Format** | JSON commands (absolute positions) | JSON joint states (leader telemetry) |
| **Input Frequency** | 10-50 Hz (varies) | 100 Hz (real-time mirroring) |
| **Latency Requirement** | Moderate (<100ms) | Strict (20-50ms for smooth mirroring) |
| **Primary Use** | Task execution | Data collection, real-time control |
| **Safety Timeout** | 500ms heartbeat | 100ms command timeout |
| **Position Mapping** | Direct (leader frame) | Scaled (may differ between arms) |

---

## Firmware Structure for Dual-Mode Design

### Option 1: Compile-Time Mode Selection (Recommended for Initial Phase)

```
follower_esp32/
├── platformio.ini
│   └── build_flag: -DOPERATION_MODE=VLA_INFERENCE or TELEOPERATION
│
├── src/
│   ├── main.cpp              (mode-agnostic initialization)
│   ├── config/
│   │   ├── config.h          (OPERATION_MODE constant)
│   │   └── mode_config.h     (mode-specific parameters)
│   │
│   ├── modules/
│   │   ├── safety/           (always active)
│   │   ├── motor_control/    (always active)
│   │   ├── servo_control/    (always active)
│   │   │
│   │   ├── vla_inference/    (VLA mode only)
│   │   │   ├── heartbeat_receiver.h/cpp
│   │   │   ├── json_command_parser.h/cpp
│   │   │   └── status_transmitter.h/cpp
│   │   │
│   │   └── teleoperation/    (Teleoperation mode only)
│   │       ├── joint_state_receiver.h/cpp
│   │       ├── telemetry_transmitter.h/cpp
│   │       ├── position_mapper.h/cpp
│   │       └── sync_monitor.h/cpp
│   │
│   └── comms/
│       ├── uart_handler.h/cpp     (mode-aware routing)
│       └── message_router.h/cpp   (directs to correct parser)
│
└── test/
    ├── mock_hardware.h
    ├── test_vla_mode.cpp       (VLA-specific tests)
    └── test_teleoperation_mode.cpp  (Teleoperation tests)q 
```

### Option 2: Runtime Mode Selection (More Flexible, Phase 2)

Modify config at runtime:
- Boot into default mode
- Allow config command: `{"cmd":"SET_MODE","mode":"TELEOPERATION"}`
- Requires state machine to handle mode transitions
- Slightly more complex but allows switching between modes without reflash

---

## Teleoperation Mode - Component Specification

### 1. Joint State Receiver Module

**Purpose**: Parse incoming leader joint positions and map to follower coordinates

**Input Format (JSON)**:
```json
{
  "cmd": "JOINT_STATE",
  "seq": 42,
  "ts": 1000500,
  "leader_joints": [0.0, 45.0, 90.0, 135.0, 90.0, 90.0],
  "valid_mask": [1, 1, 1, 1, 1, 1]
}
```

**Fields**:
- `leader_joints`: Array of 6 joint angles (degrees) or -1 for invalid
- `valid_mask`: Binary flags for each joint (1=valid, 0=invalid/disconnected)

**Key Methods**:
```cpp
class JointStateReceiver {
  bool parseJointStateMessage(const JsonDocument& doc);
  float* getLeaderJointAngles();        // Pointer to latest joint angles
  uint32_t getSequenceNumber() const;
  uint32_t getTimestamp() const;
  bool isJointValid(int joint_idx) const;
  uint32_t getTimeSinceLastMessage() const;  // For timeout detection
};
```

**Behavior**:
- Non-blocking parsing (same as VLA mode)
- Tracks sequence numbers for gap detection
- Stores previous command for interpolation if needed
- Validates joint angles (within hardware limits)

---

### 2. Position Mapper Module

**Purpose**: Convert leader joint angles to follower target positions

**Features**:
- **Identity mapping** (default): `follower_target = leader_angle`
- **Scaled mapping**: Scale by mechanical ratio 
- **Inverted mapping**: Mirror specific axes
- **Offset mapping**: Adjust for different home positions

**Configuration Structure**:
```cpp
struct JointMapping {
  float scale;        // 1.0 = 1:1, 0.5 = half speed, etc.
  float offset;       // Home position offset (degrees)
  bool inverted;      // Mirror this joint
  float min_angle;    // Clamp to valid range
  float max_angle;
};

class PositionMapper {
  JointMapping mappings[6];  // Per-joint configuration
  
  float mapPosition(int joint_idx, float leader_angle);
  void loadMappingFromEEPROM();
  void saveMappingToEEPROM();
};
```

**Use Cases**:
- Different arm kinematics
- Asymmetric arms (left vs right)
- Non-1:1 gear ratios
- Mirror bilateral teleoperation (left leader controls right follower)

---

### 3. Telemetry Transmitter Module

**Purpose**: Send follower joint positions back to leader/laptop

**Output Format (JSON)**:
```json
{
  "cmd": "TELEMETRY_STATE",
  "seq": 42,
  "ts": 1000500,
  "follower_joints": [0.0, 45.0, 90.0, 135.0, 90.0, 91.2],
  "errors": [0, 0, 0, 0, 0, 1.2],
  "motor_currents": [1.2, 1.5, 2.1, 0.8, 0.2, 0.15],
  "faults": [0, 0, 0, 0, 0, 0]
}
```

**Transmission**:
- **Frequency**: 100 Hz (real-time feedback for smooth operation)
- **Medium**: USB-CDC (non-blocking, drop if buffer full)

**Key Methods**:
```cpp
class TelemetryTransmitter {
  void queueTelemetryMessage(const float* joint_angles,
                            const float* errors,
                            const float* currents,
                            const uint8_t* faults);
  void update();  // Called from control loop, sends if time window reached
};
```

---

### 4. Synchronization Monitor Module

**Purpose**: Monitor communication health and latency in teleoperation mode

```cpp
class SynchronizationMonitor {
  // Tracks message flow
  uint32_t last_valid_command_ts;    // When we last got valid JOINT_STATE
  uint32_t last_ack_sent_ts;         // When we last sent TELEMETRY_STATE
  
  // Latency tracking
  uint32_t message_latency_ms;       // One-way latency estimate
  uint32_t roundtrip_latency_ms;     // If laptop echoes timestamp
  
  // Detection
  bool isCommandTimeout() const;     // >100ms since last valid command
  bool isHighLatency() const;        // >50ms latency
  bool isCommandGap() const;         // Sequence number gap
  
  // Action
  void handleTimeout();              // Stop motors, raise fault
  void handleHighLatency();          // Log warning, continue
};
```

**Timeout Behavior**:
- **100ms command timeout**: Drop to fail-safe (motors disable)
- **Stricter than VLA mode** because teleoperation is real-time
- If leader loses connection, follower stops immediately

---

## Communication Protocol Comparison

### VLA Inference Mode (Current)
```
Command (Laptop → Follower):
{"cmd":"MOTOR_TARGET","seq":1,"targets":[0,45,90,135,0,0]}
         ↓
Parse in follower → Apply positions
         ↓ 
Response (Follower → Laptop):
{"cmd":"MOTOR_STATUS","seq":1,"positions":[0,45,90,135,0,0]}

Frequency: 10-50 Hz (sporadic, task-driven)
Latency: Moderate (<100ms acceptable)
Timeout: 500ms
```

### Teleoperation Mode (Proposed)
```
Command (Leader → Follower via Laptop):
{"cmd":"JOINT_STATE","seq":42,"leader_joints":[0,45,90,135,90,90],
 "valid_mask":[1,1,1,1,1,1]}
         ↓
Parse in follower → Map leader angles → Target positions
         ↓
Response (Follower → Laptop):
{"cmd":"TELEMETRY_STATE","seq":42,"follower_joints":[0,45,90,135,90,91.2],
 "errors":[0,0,0,0,0,1.2],"motor_currents":[...]}

Frequency: 100 Hz (constant streaming)
Latency: Strict (<50ms for smooth mirroring)
Timeout: 100ms (tight safety margin)
```

---

## Firmware Modification Plan

### Phase 1: Prepare for Dual-Mode (Non-breaking)

**Goal**: Make current VLA code mode-aware without changing behavior

1. **Create `mode_config.h`**:
   ```cpp
   #ifndef OPERATION_MODE
   #define OPERATION_MODE VLA_INFERENCE  // Default
   #endif
   
   #define VLA_INFERENCE 1
   #define TELEOPERATION 2
   ```

2. **Create mode-specific directories**:
   - `src/modules/vla_inference/` (move current heartbeat/command modules)
   - `src/modules/teleoperation/` (new: placeholder until Phase 2)

3. **Create message router** in `comms/message_router.h`:
   ```cpp
   void routeIncomingMessage(const JsonDocument& doc) {
     #if OPERATION_MODE == VLA_INFERENCE
       vla_handlers.routeCommand(doc);
     #elif OPERATION_MODE == TELEOPERATION
       tele_handlers.routeCommand(doc);
     #endif
   }
   ```

4. **Update main.cpp initialization**:
   ```cpp
   void setup() {
     initCommonModules();  // Safety, Motors, Servos (always)
     
     #if OPERATION_MODE == VLA_INFERENCE
       initVLAMode();      // Heartbeat, JSON parser, status TX
     #elif OPERATION_MODE == TELEOPERATION
       initTeleopMode();   // Joint state RX, position mapper, telemetry TX
     #endif
   }
   ```

**Effort**: 4-6 hours (refactoring, no new features)  
**Risk**: Low (VLA mode behavior unchanged)  
**Benefit**: Foundation for Phase 2, clean separation

### Phase 2: Implement Teleoperation Mode

**Goal**: Add full teleoperation capability

1. **Implement JointStateReceiver** (similar to HeartbeatReceiver):
   - Parse JSON with 6 joint angles
   - Sequence tracking
   - Timeout detection

2. **Implement PositionMapper**:
   - Load/save calibration from EEPROM
   - Support multiple mapping profiles

3. **Implement TelemetryTransmitter**:
   - Queue telemetry at 100 Hz
   - Include motor currents, error feedback

4. **Implement SynchronizationMonitor**:
   - Track 100ms timeout
   - Latency measurements

5. **Unit tests**:
   - Mock leader joint inputs
   - Verify position mapping accuracy
   - Test timeout behavior

**Effort**: 8-12 hours (new modules + testing)  
**Risk**: Medium (tight timing requirements)  
**Benefit**: Full teleoperation capability

### Phase 3: Hardware Integration (Leader ESP32)

**Goal**: Complement with leader firmware

1. **Create `leader_esp32/` firmware**:
   - Read potentiometers/encoders (ADC inputs)
   - Publish joint angles at 100 Hz
   - Simple protocol (no safety critical)

2. **Integrate with Laptop coordinator**:
   - Laptop reads from leader
   - Forwards to follower
   - Collects follower telemetry (optional: log for replay)

**Effort**: 6-8 hours (simple, largely existing code patterns)  
**Risk**: Low (hardware-agnostic)  
**Benefit**: Complete teleoperation pipeline

---

## Teleoperation Mode - Safety Considerations

### Motor Disable Hierarchy

```
Priority 1 (Highest): Command Timeout (100ms)
├─ Trigger: No valid JOINT_STATE received for 100ms
├─ Action: Immediate motor disable (EN pin LOW)
└─ Recovery: New JOINT_STATE resumes operation

Priority 2: Safety Fault (same as VLA mode)
├─ Trigger: Stall detection, current overload, etc.
├─ Action: Motors disabled, fault state TX'd
└─ Recovery: Manual reset via RESET_FAULT command

Priority 3: High Latency Warning (50ms)
├─ Trigger: Message latency >50ms
├─ Action: Log warning, continue operation
└─ Recovery: Automatic when latency improves
```

### Fail-Safe Defaults

- **No leader connection**: Follower stays in last-known position (motors enabled, holding position)
- **Communication gap**: Immediate motor disable (fail-safe)
- **Position error too large**: Disable joint (flag in joint_faults)
- **Current spike**: Disable motor, raise fault

---

## Integration with Existing Modules

### Shared Components (Both Modes)

```
MotorController (PID)
  ├─ VLA mode: Target from JSON command
  └─ Tele mode: Target from position mapper

ServoManager (LEDC PWM)
  ├─ VLA mode: Target from JSON command
  └─ Tele mode: Target from position mapper

SafetyTask
  ├─ VLA mode: 500ms heartbeat timeout
  └─ Tele mode: 100ms command timeout + same faults

TelemetryCollector
  ├─ VLA mode: Collected at 10Hz, sent on demand
  └─ Tele mode: Collected at 100Hz, streamed continuously
```

### Mode-Specific Component Lifecycle

```
VLA Mode Startup:
  1. HeartbeatReceiver starts listening
  2. Motors stay disabled until first heartbeat
  3. Once connected, motors enable (if no other faults)

Teleoperation Mode Startup:
  1. PositionMapper loads calibration
  2. Motors enabled (ready to receive commands)
  3. Motors disable if no JOINT_STATE for 100ms

Mode Switching (Runtime, Phase 2+):
  1. Send {"cmd":"SET_MODE","mode":"TELEOPERATION"}
  2. Firmware stops current mode listeners
  3. Firmware initializes new mode
  4. Motors disable (safe transition)
  5. Return {"cmd":"MODE_CHANGED_ACK","mode":"TELEOPERATION"}
```

---

## Development Roadmap

### Week 1-2: Refactoring (Phase 1)
- [ ] Create mode configuration system
- [ ] Reorganize modules into mode directories
- [ ] Implement message router
- [ ] Update main.cpp
- [ ] Run existing VLA tests (should all pass)

### Week 3-4: Teleoperation Core (Phase 2)
- [ ] Implement JointStateReceiver
- [ ] Implement PositionMapper with EEPROM support
- [ ] Implement TelemetryTransmitter
- [ ] Implement SynchronizationMonitor
- [ ] Write unit tests (20+ tests)

### Week 5-6: Integration & Testing (Phase 2)
- [ ] Integrate with SafetyTask
- [ ] Test mode-aware timeout handling
- [ ] Integration tests (VLA + Tele switching)
- [ ] Performance profiling (latency, RAM usage)

### Week 7-8: Leader Firmware (Phase 3)
- [ ] Create leader_esp32 project structure
- [ ] Implement sensor reader (ADC potentiometers)
- [ ] Implement joint publisher (100 Hz)
- [ ] Simple test (sensor → leader → follower)

---

## Questions for Architecture Review

1. **Position Mapping Complexity**: Should we support per-joint calibration from day 1, or just identity mapping for initial teleoperation?

2. **Laptop Coordination**: Does the laptop act as:
   - Simple router (leader → follower only)?
   - Data logger (records telemetry for VLA training)?
   - Safety monitor (enforces additional constraints)?

3. **Latency Target**: Is 50ms one-way latency acceptable, or need tighter (<30ms)?

4. **Wireless Future**: Should we architect for WiFi uplink (leader → WiFi → laptop → WiFi → follower), or assume USB for Phase 1?

5. **Encoder vs Potentiometer**: What sensors will leader have? Affects calibration complexity.

6. **Simultaneous Modes**: Should firmware support per-joint mode (some joints VLA, some tele), or full-system mode only?

---

## Comparison: Current vs Proposed

### Current VLA Inference Mode
✅ Core implementation active  
✅ 400 Hz control loop  
✅ Safety timeout: 500ms  
✅ JSON command protocol  
✅ Motor control + homing command support  
🚧 Documentation and integration still being refined  

### Teleoperation Mode
✅ Compile-time mode wiring in place  
✅ Leader JOINT_STATE path and bridge forwarding in place  
✅ Follower teleop command handling includes `ENABLE_MOTORS`, `RESET_FAULT`, `HOME_ZERO`, `HOME_STALLGUARD`  
⏳ Safety timeout: 100ms (tighter)  
⏳ Follower `TELEMETRY_STATE` expansion still pending  
⏳ Position mapper + sync monitor still evolving  
⏳ New: Telemetry streaming (100 Hz)  

**Current Risk Level**: Medium (safety-critical coordination still under active iteration)  
**Benefit**: Full teleoperation data collection capability once telemetry + ROS2 teleop runtime are finalized  

---

## Conclusion

**Current Status**: 🚧 VLA + teleoperation firmware path is functional but still in active development  
**Proposed Direction**: Dual-mode firmware supporting both VLA inference and teleoperation  
**Architecture Pattern**: Mode-aware modular firmware with shared safety/control components  
**Timeline**: Continue iterative hardening of teleoperation, telemetry, and ROS2 integration
