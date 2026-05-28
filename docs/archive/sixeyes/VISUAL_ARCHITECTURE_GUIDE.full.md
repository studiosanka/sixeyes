# SixEyes Visual Architecture Guide

Complete visual reference for the SixEyes 6-DOF robotic arm firmware and distributed control system.

---

## System Overview

### Dual-Mode Runtime View

```
                    ┌───────────────────────────────────┐
                    │    FOLLOWER ESP32 (single FW)     │
                    │    Compile-time OPERATION_MODE     │
                    └───────────────────────────────────┘
                               /            \
                              /              \
         MODE_VLA_INFERENCE (1)              MODE_TELEOPERATION (2)
         Laptop ROS2 -> follower             leader_esp32 -> bridge -> follower
         HEARTBEAT + JSON commands           JOINT_STATE + TELEMETRY_STATE
```

In both modes, the same 400 Hz safety/motor/servo core remains active; only command routing and comms behavior differ.

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    ROS2 ECOSYSTEM (Laptop)                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ ROS2 Safety Node (safety_node)                          │    │
│  │ ├─ Heartbeat Publisher (50 Hz): HB:0,<seq>             │    │
│  │ ├─ Status Subscriber: SB:<fault>,<en>,<ros2>          │    │
│  │ ├─ Emergency Stop Logic                                │    │
│  │ └─ Visualization (RViz)                                │    │
│  └─────────────────────────────────────────────────────────┘    │
│                           ↕ USB-CDC                              │
│                    (ASCII heartbeat protocol)                     │
└─────────────────────────────────────────────────────────────────┘
                              ↕
┌─────────────────────────────────────────────────────────────────┐
│            ESP32-S3 FOLLOWER (Embedded Firmware)                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ 400 Hz Deterministic Control Loop (FreeRTOS)            │   │
│  │                                                          │   │
│  │  ┌─────────────────────────────────────────────────┐    │   │
│  │  │ SafetyTask (Highest Priority)                  │    │   │
│  │  │ ├─ ROS2 Heartbeat Monitor (500 ms timeout)    │    │   │
│  │  │ ├─ Motor EN Pin Control (immediate disable)   │    │   │
│  │  │ └─ Fault Latch & Recovery                     │    │   │
│  │  └─────────────────────────────────────────────────┘    │   │
│  │                      ↓                                   │   │
│  │  ┌─────────────────────────────────────────────────┐    │   │
│  │  │ MotorController (Closed-loop PID)              │    │   │
│  │  │ ├─ 4× Stepper Motor Position Tracking          │    │   │
│  │  │ ├─ S-curve Trajectory Interpolation (1 sec)   │    │   │
│  │  │ └─ Send commands to TMC2209 via UART          │    │   │
│  │  └─────────────────────────────────────────────────┘    │   │
│  │                      ↓                                   │   │
│  │  ┌─────────────────────────────────────────────────┐    │   │
│  │  │ ServoManager (PWM Control)                      │    │   │
│  │  │ ├─ 3× MG996R Servo Control (LEDC)             │    │   │
│  │  │ ├─ Degree-to-Microseconds Mapping             │    │   │
│  │  │ └─ Watchdog (100 ms timeout)                  │    │   │
│  │  └─────────────────────────────────────────────────┘    │   │
│  │                      ↓                                   │   │
│  │  ┌─────────────────────────────────────────────────┐    │   │
│  │  │ TelemetryCollector & USB-CDC Transmitter        │    │   │
│  │  │ ├─ Aggregate: joint positions, velocities      │    │   │
│  │  │ ├─ Aggregate: servo angles, motor states       │    │   │
│  │  │ └─ Non-blocking USB write (10 Hz)             │    │   │
│  │  └─────────────────────────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Hardware Drivers (Below Control Loop)                    │   │
│  │ ├─ TMC2209 Stepper Drivers (4× via UART)               │   │
│  │ ├─ LEDC PWM Servo Control (3× GPIO)                    │   │
│  │ ├─ GPIO: Motor EN pin control                          │   │
│  │ └─ ADC: Optional current sensing                       │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              ↕ (Power + Control Signals)
┌─────────────────────────────────────────────────────────────────┐
│              MECHANICAL HARDWARE                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ 6-DOF Arm (SixEyes)                                      │   │
│  │ ├─ Base Rotation (NEMA23 Stepper)                       │   │
│  │ ├─ Shoulder Pitch (NEMA23 Stepper) + Servo Assist      │   │
│  │ ├─ Shoulder Roll (NEMA23 Stepper) + Servo Assist       │   │
│  │ ├─ Elbow Pitch (NEMA23 Stepper)                        │   │
│  │ ├─ Wrist Pitch (MG996R Servo)                          │   │
│  │ ├─ Wrist Yaw (MG996R Servo)                            │   │
│  │ └─ Gripper (MG996R Servo)                              │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 400 Hz Control Loop Timing

### Execution Timeline (One 2.5 ms Cycle)

```
0 ms     SafetyTask starts
├─ Parse ROS2 heartbeat (HB:0,N)              [<0.5 ms]
├─ Check heartbeat timeout (500 ms)           [<0.1 ms]
├─ Update EN pin state (GPIO write)           [<0.1 ms]
└─ Queue status packet (SB:...)               [<0.1 ms]
    Total: ~1 ms
         ↓
1 ms     MotorController::update()
├─ Calculate PID output for each motor        [~0.5 ms]
├─ Apply S-curve interpolation                [~0.3 ms]
└─ Transmit position targets to TMC2209       [~0.2 ms]
    Total: ~1 ms
         ↓
2 ms     ServoManager::update()
├─ Calculate PWM duty cycle for 3 servos      [~0.2 ms]
├─ Write PWM values via LEDC                  [~0.2 ms]
└─ Check servo watchdog timers                [~0.05 ms]
    Total: ~0.5 ms
         ↓
2.5 ms   Control loop complete
         Wait until next 2.5 ms boundary
         ↓
5 ms     Control loop iteration #2 starts
         (Cycle repeats at 400 Hz)
```

### Transmitter Scheduling

```
Control Loop [400 Hz]  │ USB-CDC TX      │ Decision
───────────────────────┼─────────────────┼────────────────────
Cycle 0   (0 ms)       │ SB:0,0,0        │ Every 100ms (10 Hz)
Cycle 1   (2.5 ms)     │ [skip]          │
Cycle 2   (5 ms)       │ [skip]          │
⋮         ⋮            │ ⋮               │
Cycle 40  (100 ms)     │ SB:0,1,1        │ TX window opened
Cycle 41  (102.5 ms)   │ [skip]          │
⋮         ⋮            │ ⋮               │
Cycle 80  (200 ms)     │ SB:0,1,1        │ TX window opened
```

---

## Module Dependency Graph

```
┌─────────────────────────────────────────────────────────────┐
│                    main.cpp (Setup)                         │
│                          │                                   │
│        ┌─────────────────┼─────────────────┐                │
│        ↓                 ↓                 ↓                │
│    SafetyTask      MotorController    ServoManager          │
│        │                 │                 │                │
│        ├─ HeartbeatMonitor               │                │
│        ├─ HeartbeatReceiver              │                │
│        ├─ ROS2SafetyBridge              │                │
│        ├─ HeartbeatTransmitter          │                │
│        ├─ FaultManager                  │                │
│        └─ HAL_GPIO                      │                │
│                          │                 │                │
│        ┌─────────────────┼─────────────────┘                │
│        ↓                 ↓                                   │
│   MotorControlScheduler (FreeRTOS Task)                    │
│        │                                                    │
│        ├─ Calls SafetyTask::update()                        │
│        ├─ Calls MotorController::update()                  │
│        ├─ Calls ServoManager::update()                     │
│        └─ Calls TelemetryCollector::collect()              │
│                          │                                   │
│        ┌─────────────────┴─────────────────┐                │
│        ↓                                   ↓                │
│   TMC2209Driver (4x via UART)     TelemetryFormatter       │
│        │                                   │                │
│        ├─ PDN_UART pins                   └─→ USB-CDC TX   │
│        ├─ Register protocol                                │
│        └─ StallGuard monitoring                            │
│                                                             │
│   Dependencies (FreeRTOS, Arduino)                         │
│   ├─ FreeRTOS (task scheduling)                           │
│   ├─ Arduino (Serial, GPIO, LEDC, SPI)                    │
│   └─ TMCStepper library (motor driver support)            │
└─────────────────────────────────────────────────────────────┘
```

---

## Hardware Pin Mapping

### ESP32-S3 Pin Configuration

```
┌──────────────────────────────────────────────────────────────────┐
│                     ESP32-S3 PINOUT                              │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  STEPPER MOTORS (TMC2209 via PDN_UART)                          │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ UART Port: Serial2 (GPIO17/RX, GPIO18/TX)  @ 115200 baud  │  │
│  │                                                            │  │
│  │ Device Selection (PDN pins):                             │  │
│  │   • GPIO7   = Motor 0 (Base rotation)                    │  │
│  │   • GPIO11  = Motor 1 (Shoulder pitch)                   │  │
│  │   • GPIO15  = Motor 2 (Shoulder roll)                    │  │
│  │   • GPIO21  = Motor 3 (Elbow pitch)                      │  │
│  │                                                            │  │
│  │ Motor Enable (EN):                                        │  │
│  │   • GPIO6   = Enables all 4 steppers (active HIGH)       │  │
│  │                                                            │  │
│  │ Power: 24V, 1.5A per motor, 6Ω current sense resistor   │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  SERVO MOTORS (MG996R via LEDC PWM)                             │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ Frequency: 50 Hz, Resolution: 16-bit                      │  │
│  │                                                            │  │
│  │ Servo Pins (GPIO → Angle mapping):                        │  │
│  │   • GPIO35 = Pitch servo (0-180°)                        │  │
│  │   • GPIO36 = Yaw servo (0-180°)                          │  │
│  │   • GPIO37 = Gripper (0-180°)                            │  │
│  │                                                            │  │
│  │ Pulse Width Mapping:                                      │  │
│  │   500 µs  → 0°                                            │  │
│  │   1500 µs → 90° (neutral/safe power-up)                  │  │
│  │   2500 µs → 180°                                          │  │
│  │                                                            │  │
│  │ Power: 6.6V rail (XL4016), ~1A per servo                 │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  COMMUNICATION & DIAGNOSTICS                                    │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ USB-CDC (Native ESP32-S3):                                │  │
│  │   • Serial output (Serial class)                          │  │
│  │   • Heartbeat RX: "HB:0,<seq>"                           │  │
│  │   • Status TX: "SB:<fault>,<en>,<ros2>"                 │  │
│  │   • Baud: 115200 (doesn't matter, USB virtual)           │  │
│  │                                                            │  │
│  │ LED (diagnostic, optional):                               │  │
│  │   • GPIO46 = Status LED (activity indicator)             │  │
│  │                                                            │  │
│  │ ADC (optional, future expansion):                         │  │
│  │   • GPIO3  = Battery voltage (100k/100k divider)         │  │
│  │   • GPIO4  = Current sensor feedback (TMC diagnostics)   │  │
│  │                                                            │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  RESERVED / FUTURE USE                                          │
│  ├─ GPIO8,9   = Flash SPI (usually reserved)                    │
│  ├─ GPIO42    = TX (Serial monitor)                             │
│  ├─ GPIO43    = RX (Serial monitor)                             │
│  └─ GPIO1,2   = Strapping pins (boot/mode select)              │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Power Distribution

```
┌──────────────────────────────────────────────────────────────────┐
│                    POWER SUPPLY ARCHITECTURE                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  24V Stepper Power (4× NEMA23 @ 1.5A each, total 6A max)        │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━──►  │
│      24V PSU (100W minimum)                                      │
│         │                                                         │
│         ├─ Thick gauge wire (12AWG or thicker)                  │
│         ├─ 1000µF electrolytic cap (surge protection)           │
│         │                                                         │
│         ├─► TMC2209 Driver 1 (sense: 24V - 0.11Ω = motor)       │
│         ├─► TMC2209 Driver 2 ─────┐                            │
│         ├─► TMC2209 Driver 3       │  SHARED STEPPER BUS        │
│         └─► TMC2209 Driver 4 ─────┘  (GPIO EN pin disables all) │
│                                                                   │
│  6.6V Servo Power (XL4016 buck, 3× MG996R @ 1A each, 3A max)   │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━──►  │
│      Derived from 24V main input via onboard XL4016              │
│         │                                                         │
│         ├─ Gauge wire (16-18AWG)                                │
│         ├─ ≥1000µF electrolytic cap near servo headers          │
│         │                                                         │
│         ├─► MG996R Pitch servo (GPIO35 PWM)                     │
│         ├─► MG996R Yaw servo (GPIO36 PWM)                       │
│         └─► MG996R Gripper (GPIO37 PWM)                         │
│                                                                   │
│  3.3V Logic / ESP32-S3 (MP1584 buck from 24V input)             │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━──►  │
│      24V input → MP1584 buck → 3.3V logic rail                  │
│         │                                                         │
│         ├─ ≥22µF input/output converter caps                     │
│         ├─ 100nF bypass cap (near MCU)                          │
│         │                                                         │
│         └─► ESP32-S3 & GPIO logic circuits                      │
│                                                                   │
│  GROUNDING STRATEGY (Star topology)                              │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━──►  │
│      Single: 24V PSU GND ──┐                                     │
│                             ├─→ Central GND Point (PCB/chassis)  │
│      Single: 6.6V rail GND ─┤                                    │
│      Single: USB GND ──────┘                                    │
│                                                                   │
│      From central GND point:                                     │
│         ├─ Thick wire to ESP32-S3 GND                           │
│         ├─ Thick wire to all motor driver GNDs                  │
│         └─ Thick wire to servo power GND                        │
│                                                                   │
│      CRITICAL: No ground loops! All PSUs reference same point.   │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## ROS2 Heartbeat Protocol Flow

```
LAPTOP (ROS2 safety_node)              FIRMWARE (ESP32-S3)
════════════════════════════════════   ═════════════════════════════

     ┌─ Heartbeat Timer ─┐
     │  (50 Hz, 20 ms)   │
     ├─ Increment seq.   │
     └─ Format: HB:0,42  │
          │
          ├─→ USB-CDC ────────────────────→ Serial Input Buffer
                                                │
                                         ┌──────┴──────┐
                                         │ Every cycle │
                                         └──────┬──────┘
                                                │
                                     HeartbeatReceiver::update()
                                     ├─ Non-blocking UART read
                                     ├─ Parse "HB:0,42\n"
                                     ├─ Feed to HeartbeatMonitor
                                     └─ Track packets_received++
                                                │
                                         ┌──────┴──────┐
                                         │ SafetyTask  │
                                         └──────┬──────┘
                                                │
                                      ┌─────────┴─────────┐
                                      │ Check ROS2 alive? │
                                      ├─ Last seq: 42     │
                                      ├─ Timeout: 1 sec   │
                                      └─────────┬─────────┘
                                                │
                                    ┌───────────┴────────────┐
                                    │ heartbeat_ok = TRUE    │
                                    └───────────┬────────────┘
                                                │
                                         Enable EN pin
                                    (motors can now run)
                                                │
                                                ├─→ USB-CDC ───────────→
                                                │  "SB:0,1,1"            │
                                                │                        ▼
                                          Status TX Queue          Serial Output
                                       (queued, non-blocking)           │
                                                                        │
                                    [Every 100ms (10 Hz):              │
                                     Actually write to USB buffer]      │
                                                                        ▼
                                                            ┌─ ROS2 RX Buffer
                                                            │
                                                    ROS2 Status Handler
                                                    ├─ Parse "SB:0,1,1"
                                                    ├─ Update fault=FALSE
                                                    ├─ Update motors_en=TRUE
                                                    ├─ Update ros2_alive=TRUE
                                                    └─ Publish /firmware_status


TIMEOUT SCENARIO:
════════════════

LAPTOP                                  FIRMWARE
  │ Send HB:0,0                            │
  ├────────────────────────────────────────→
  │                              Feed to monitor
  │                              heartbeat_ok=TRUE
  │                              Motors: ON
  │
  │ (Stop sending heartbeats)
  │
  │    [250 ms elapsed]              heartbeat_ok=TRUE (still within window)
  │                              Motors: ON
  │
  │    [500 ms elapsed]          ⚠ TIMEOUT DETECTED
  │                              heartbeat_ok=FALSE
  │                              → EN pin LOW (motors OFF immediately)
  │                              → Raise HEARTBEAT_TIMEOUT fault
  │                              → Queue "SB:1,0,0"
  │                              
  │    [505 ms elapsed]          SB:1,0,0 transmitted to ROS2
  │  ◄────────────────────────────────────────
  │
  │ ROS2 receives: Firmware faulted!
  │ ├─ Publish emergency_stop
  │ └─ Log event
```

---

## Data Flow Diagram (Control Loop)

```
┌───────────────────────────────────────────────────────────────────┐
│                   CONTROL LOOP DATA FLOW                          │
│                       (400 Hz, 2.5 ms)                            │
└───────────────────────────────────────────────────────────────────┘

INPUTS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

┌──────────────────────┐    ┌──────────────────────┐
│ ROS2 Heartbeat       │    │ Motor Commands       │
│ (from USB-CDC RX)    │    │ (implicit: heartbeat │
│                      │    │  alive = enable)     │
│ Content:             │    └──────────────────────┘
│ ├─ Source: 0         │
│ ├─ Sequence: N       │
│ └─ Timestamp: T      │
└──────────────────────┘        (UART from leader
                                 or ROS2 optional)


PROCESSING
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

            ┌──────────────────────────────────┐
            │    SafetyTask::update()          │
            │  (Monitors heartbeat, EN pin)    │
            ├──────────────────────────────────┤
            │ Decision Tree:                   │
            │                                  │
            │ if no heartbeat for 500ms:       │
            │   ├─ Set fault = TRUE            │
            │   └─ motors_enabled = FALSE      │
            │                                  │
            │ else if heartbeat OK:            │
            │   └─ motors_enabled = TRUE       │
            │       (if no other faults)       │
            │                                  │
            │ Output: EN pin value             │
            └──────────────────────────────────┘
                         ↓
            ┌──────────────────────────────────┐
            │  MotorController::update()       │
            │  (4× closed-loop PID)            │
            ├──────────────────────────────────┤
            │ For each motor:                  │
            │ ├─ Read current position (enc.) │
            │ ├─ Calculate error vs target    │
            │ ├─ PID: P(2.0) + I(0.05)        │
            │ │       + D(0.1)                 │
            │ ├─ Apply S-curve interp. over  │
            │ │  1.0 sec (smooth accel)       │
            │ └─ Send step rate to UART       │
            │                                  │
            │ Output: 4 step/dir commands     │
            └──────────────────────────────────┘
                         ↓
            ┌──────────────────────────────────┐
            │  ServoManager::update()          │
            │  (3× PWM servo control)          │
            ├──────────────────────────────────┤
            │ For each servo:                  │
            │ ├─ Convert angle (0-180°)       │
            │ │  to pulse (500-2500 µs)       │
            │ ├─ Calculate duty cycle         │
            │ │  (16-bit LEDC value)          │
            │ ├─ Check watchdog (100 ms)      │
            │ │  auto-disable if stale        │
            │ └─ Write PWM (50 Hz freq)       │
            │                                  │
            │ Output: 3 PWM signals (GPIO35,36,37)
            └──────────────────────────────────┘
                         ↓
            ┌──────────────────────────────────┐
            │  TelemetryCollector             │
            │  (Aggregate state → JSON)        │
            ├──────────────────────────────────┤
            │ Gather:                          │
            │ ├─ Motor positions (4)           │
            │ ├─ Motor velocities (4)          │
            │ ├─ Servo angles (3)              │
            │ ├─ Motor enable state            │
            │ ├─ Fault status                  │
            │ └─ ROS2 alive status             │
            │                                  │
            │ JSON: {"t":ms, "s":[pos...],    │
            │        "sv":[vel...], "servo":...}
            └──────────────────────────────────┘
                         ↓
            ┌──────────────────────────────────┐
            │  USB-CDC Transmit                │
            │  (Non-blocking, 10 Hz)           │
            ├──────────────────────────────────┤
            │ If TX interval elapsed:          │
            │ ├─ Check USB buffer available   │
            │ ├─ Send telemetry JSON on USB   │
            │ └─ Update TX statistics         │
            │                                  │
            │ Output: Status to ROS2           │
            └──────────────────────────────────┘


OUTPUTS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

┌──────────────────────┐  ┌──────────────────────┐  ┌──────────────┐
│ GPIO6 (EN pin)       │  │ UART TX (motor cmds) │  │ USB-CDC TX   │
│ ├─ HIGH: enabled     │  │                      │  │ (telemetry)  │
│ └─ LOW: disabled     │  │ Format:              │  │              │
│                      │  │ ├─ Step rate         │  │ Format:      │
│ To: Stepper drivers  │  │ ├─ Direction         │  │ ├─ Status    │
│                      │  │ └─ Microsteps        │  │ ├─ Positions │
└──────────────────────┘  │                      │  │ └─ Faults    │
                          │ To: TMC2209 drivers  │  │              │
                          └──────────────────────┘  │ To: ROS2     │
                                                     └──────────────┘

                    ┌──────────────────────┐
                    │ GPIO35/36/37 (PWM)   │
                    │ ├─ Frequency: 50 Hz │
                    │ ├─ Duty: var (Pulse) │
                    │ └─ Resolution: 16-bit│
                    │                      │
                    │ To: MG996R servos    │
                    └──────────────────────┘
```

---

## Fault State Machine

```
                    ┌─────────────────────────────────┐
                    │ NORMAL OPERATION                │
                    │ (No faults active)              │
                    │ Motors: CAN ENABLE (heartbeat OK)
                    └────────────┬──────────────────┬─┘
                                 │                  │
                    ┌────────────┴─┐        ┌──────┴──────────┐
                    │              │        │                 │
                    ▼              ▼        ▼                 ▼
        ┌──────────────────┐ ┌───────────────────┐ ┌─────────────────┐
        │ HEARTBEAT        │ │ MOTOR STALL       │ │ OVER_VOLTAGE    │
        │ TIMEOUT          │ │ (StallGuard)      │ │ or THERMAL      │
        └────────┬─────────┘ └─────────┬─────────┘ └────────┬────────┘
                 │                     │                     │
                 │ ROS2 stops          │ Stepper driver      │ Driver
                 │ sending HB for      │ detects stall &     │ detects
                 │ >500ms              │ triggers fault      │ error
                 │                     │                     │
                 └─────────┬───────────┴────────┬────────────┘
                           │                    │
                        ┌──▼────────────────────▼──┐
                        │  FAULT STATE TRIGGERED   │
                        │  Motors: DISABLED        │
                        │  EN pin: LOW             │
                        │  Fault latch: ACTIVE    │
                        └───┬───────────────────┬──┘
                            │                   │
                 ┌──────────┴─┐         ┌──────┴────────┐
                 │            │         │               │
                 ▼            ▼         ▼               ▼
        ┌──────────────┐ ┌───────┐ ┌──────────────┐  ┌──────────┐
        │ FAULT LATCH  │ │ User  │ │ ROS2 Safety  │  │ Firmware │
        │ (memory of   │ │ clear │ │ Node detects │  │ restart  │
        │ fault)       │ │ fault │ │ fault status │  │ (soft)   │
        └────────┬─────┘ │ via   │ │ & publishes  │  └──────┬───┘
                 │       │ USB   │ │ emergency    │         │
                 │       │ or    │ │ stop         │         │
                 │       │ ROS2  │ └──────────────┘         │
                 │       └───┬───┘         │                │
                 │           │            │                │
                 └───────────┴────────┬───┴──────┬──────────┘
                                      │          │
                                ┌─────▼──────────▼─┐
                                │ FAULT CLEARED    │
                                │ Motors: CAN      │
                                │ enable again     │
                                │                  │
                                └────────┬─────────┘
                                         │
                                    (return to
                                    NORMAL if
                                    heartbeat OK)


LATCHES:
════════
┌──────────────────────────────────────────────────────┐
│ Once a fault is raised:                              │
│ ├─ Stays ACTIVE until explicitly cleared            │
│ ├─ Prevents accidental motor re-enable after error  │
│ ├─ Forces user/ROS2 to acknowledge & clear          │
│ └─ Can be cleared by:                               │
│    ├─ SafetyTask::clearFault() (firmware API)       │
│    ├─ ROS2 safety_node (via command)                │
│    └─ Power cycle (full reset)                      │
└──────────────────────────────────────────────────────┘
```

---

## Memory Layout

```
┌──────────────────────────────────────────────────────────────────┐
│           ESP32-S3 MEMORY USAGE (320 KB RAM Total)               │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─ IRAM (Fast, instruction RAM, 128 KB) ──────────────┐        │
│  │  ├─ ISR vectors & fast code                         │        │
│  │  └─ Interrupt handlers (UART RX, timers)           │        │
│  │  Utilization: ~40 KB (good headroom)               │        │
│  └────────────────────────────────────────────────────┘        │
│                                                                   │
│  ┌─ DRAM (Data RAM, 192 KB) ────────────────────────────┐        │
│  │  ├─ Static variables                                │        │
│  │  │  ├─ MotorController state (500 B)               │        │
│  │  │  ├─ ServoManager state (200 B)                  │        │
│  │  │  ├─ SafetyTask state (300 B)                    │        │
│  │  │  ├─ HeartbeatReceiver buffer (64 B)             │        │
│  │  │  └─ USB telemetry buffer (256 B)                │        │
│  │  │  Subtotal: ~1.5 KB                              │        │
│  │  │                                                  │        │
│  │  ├─ FreeRTOS kernel structures                      │        │
│  │  │  ├─ Task control blocks (TCB) × 4 tasks        │        │
│  │  │  ├─ Scheduler data structures                   │        │
│  │  │  └─ Queue/semaphore objects                     │        │
│  │  │  Subtotal: ~8 KB                                │        │
│  │  │                                                  │        │
│  │  ├─ Task stacks (pre-allocated)                    │        │
│  │  │  ├─ MotorControlScheduler task: 4 KB            │        │
│  │  │  ├─ Arduino event loop: 4 KB                    │        │
│  │  │  ├─ Other background tasks: 4 KB                │        │
│  │  │  Subtotal: ~12 KB                               │        │
│  │  │                                                  │        │
│  │  └─ HEAP (dynamic allocation)                      │        │
│  │     ├─ String objects                              │        │
│  │     ├─ Vectors (if used)                           │        │
│  │     └─ Optional buffers                            │        │
│  │     Subtotal: ~0 KB (minimal, deterministic code)│        │
│  │                                                  │        │
│  │  USED: ~20 KB (6.1%)                           │        │
│  │  FREE: ~172 KB (93.9%) ✓ PLENTY OF HEADROOM    │        │
│  └────────────────────────────────────────────────────┘        │
│                                                                   │
│  NOTE: No malloc() in control loop (stack allocation only).     │
│        All data structures pre-allocated at startup.            │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘


FLASH LAYOUT (3.3 MB Total)
═════════════════════════════

0x00000000  ┌─ Bootloader (read-only, ~64 KB) ─────┐
            │  (second-stage bootloader)            │
            ├─ NVS (non-volatile storage, 20 KB) ──┤
            │  (WiFi credentials, if used)          │
            ├─ OTA (over-the-air update, 64 KB) ───┤
            │  (app binary for update)              │
0x00100000  ├─ Partition table & app code ─────────┤
            │                                       │
            │  • Firmware binary (335 KB)           │
            │    ├─ Standard library code (~100 KB) │
            │    ├─ Arduino framework (~80 KB)      │
            │    ├─ FreeRTOS kernel (~30 KB)        │
            │    ├─ User application (~90 KB)       │
            │    │  ├─ SafetyTask                   │
            │    │  ├─ MotorController              │
            │    │  ├─ ServoManager                 │
            │    │  ├─ TMC2209Driver                │
            │    │  ├─ HeartbeatReceiver            │
            │    │  ├─ HeartbeatTransmitter         │
            │    │  └─ Support modules              │
            │    └─ Constant data (strings, etc.)   │
            │                                       │
            │  USED: 335 KB (10.0%)                │
            │  FREE: 3008 KB (90.0%) ✓ EXCELLENT  │
            │                                       │
0x320000    ├─ SPIFFS (filesystem, optional)  ──┤
            │  (for config files, logging)        │
            │                                     │
            └─────────────────────────────────────┘

FIRMWARE SIZE BREAKDOWN:
═══════════════════════

Sections:
  .text (code):              191 KB (IAM + IRAM)
  .rodata (constants):       72 KB (Flash)
  .dram0.data (init data):   13 KB (DRAM)
  .dram0.bss (zero init):     6 KB (DRAM)
  
Total image: 282 KB (compressed)
Decompressed: 335 KB in flash

Compilation notes:
  • -Os (optimize for size)
  • No -DDEBUG (saves ~5 KB)
  • Minimal std library linking
```

---

## Component Interaction Matrix

| SafetyTask | MotorController | ServoManager | TMC2209Driver | USB-CDC | ROS2 |
|:-----------|:----------------|:-------------|:--------------|:--------|:-----|
| **SafetyTask** | ✓ Enables/disables | ✗ No direct | ✗ No direct | ✓ TX status | ✓ RX HB |
| **MotorController** | ✓ Reads EN state | ✓ Position feedback | ✗ No direct | ✓ Telemetry | ✗ (via Safety) |
| **ServoManager** | ✓ Respects disable | ✗ No direct | ✗ No direct | ✓ Telemetry | ✗ (via Safety) |
| **TMC2209Driver** | ✓ Receives commands | ✓ SPI/UART ops | ✗ No direct | ✗ Passive | ✗ Passive |
| **USB-CDC** | ✓ Reads status | ✓ Reads state | ✓ Reads state | ✗ N/A | ✓ Bidirectional |
| **ROS2** | ✓ TX/RX HB | ✗ (via Safety) | ✗ (via Safety) | ✗ Passive | ✓ Controller |

**Key**: ✓ = Direct interaction, ✗ = No direct link, (via X) = Indirect through X

---

## Watchdog Timers

```
┌────────────────────────────────────────────────────────────────┐
│                  SAFETY WATCHDOG STRATEGY                      │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  HEARTBEAT WATCHDOG (ROS2 safety_node → ESP32)                │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━    │
│  Window: 500 ms (conservative, allows for network jitter)      │
│  Trigger: No "HB:..." packet for 500+ ms                       │
│  Action: Immediate motor disable (this cycle)                  │
│  Recovery: New heartbeat packet re-enables                     │
│  Rationale: Detect laptop crash or communication loss fast     │
│                                                                 │
│  SERVO WATCHDOG (Motor commands → Servo position)              │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━    │
│  Window: 100 ms                                                 │
│  Trigger: No servo update command for 100+ ms                   │
│  Action: Hold last position (safe state)                       │
│  Recovery: New command resumes operation                       │
│  Rationale: Detect servo control loop stall                    │
│                                                                 │
│  ROS2 CONNECTION WATCHDOG (Bidirectional status)               │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━    │
│  Window: 1 second (tracks HB packet sequence)                  │
│  Trigger: No new sequence number for 1+ sec                    │
│  Action: Set ros2_alive = FALSE (but motors stay on if HB OK) │
│  Recovery: New sequence number updates ros2_alive              │
│  Rationale: Distinguish temporary glitches from full loss      │
│                                                                 │
│  NOTE: All watchdogs are non-blocking (no waits in control   │
│        loop). Checked via simple timestamp comparisons.        │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

---

## Testing & Validation Flow

```
┌──────────────────────────────────────────────────────────────────┐
│              VALIDATION & TESTING PROGRESSION                    │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  Phase 1: UNIT TESTS (Local, isolated modules)                  │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━   │
│  ✓ HeartbeatReceiver packet parsing (ASCII format)              │
│  ✓ Motor PID controller (step response, overshoot)              │
│  ✓ S-curve interpolation (smoothness, duration)                │
│  ✓ Servo PWM calculation (angle → pulse mapping)                │
│  ✓ Fault latch state machine                                    │
│  ✓ Memory allocation (no leaks)                                 │
│                                                                   │
│  Phase 2: INTEGRATION TESTS (On-device, no load)               │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━   │
│  ✓ Control loop timing (measure actual period vs 2.5 ms)        │
│  ✓ Heartbeat timeout (500 ms ± 10 ms)                          │
│  ✓ Motor enable/disable latency (<1 cycle)                      │
│  ✓ USB-CDC telemetry streaming (10 Hz)                          │
│  ✓ Servo PWM frequency (50 Hz ± 1 Hz)                          │
│  ✓ Packet loss rate (<0.1%)                                     │
│                                                                   │
│  Phase 3: SIMULATION (Hardware-in-loop, unloaded)               │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━   │
│  ✓ PID controller response (no real motors)                     │
│  ✓ S-curve smoothness (simulate motor acceleration)            │
│  ✓ Fault state transitions (manual injection)                  │
│  ✓ ROS2 heartbeat loss scenarios                               │
│  ✓ Jitter resilience (worst-case scheduling)                   │
│                                                                   │
│  Phase 4: HARDWARE BRING-UP (Actual robot, unloaded)           │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━   │
│  ✓ Motor enable pin controls stepper enable line              │
│  ✓ Servo PWM produces 0-180° motion (via scope)               │
│  ✓ Stepper motors step in response to PID output               │
│  ✓ Telemetry values match actual encoder readings              │
│  ✓ ROS2 heartbeat → motor enable handshake                     │
│  ✓ Fault detection triggers motor disable                      │
│                                                                   │
│  Phase 5: STRESS TEST (Loaded robot, sustained)                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━   │
│  ✓ Hold loaded position (gravity load, servos)                 │
│  ✓ Sweep motors continuously (thermal, vibration)              │
│  ✓ Trigger stall detection (mechanical overload)               │
│  ✓ ROS2 E-stop during operation                                │
│  ✓ 1-hour reliability run (no resets, no glitches)            │
│  ✓ Power cycling resilience                                    │
│                                                                   │
│  Phase 6: FIELD DEPLOYMENT (Real vision-language tasks)        │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━   │
│  ✓ Follow ROS2 trajectory commands (pick & place)              │
│  ✓ Recover from collisions (soft touch)                        │
│  ✓ Respond to vision feedback (object tracking)                │
│  ✓ Long-term stability (days of operation)                     │
│  ✓ Thermal management (continuous duty)                        │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference: Critical Paths

```
┌─────────────────────────────────────────────────────────────────┐
│              CRITICAL EXECUTION PATHS                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  FASTEST PATH: Heartbeat timeout → Motor disable               │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                  │
│  t=0 ms      ROS2 stops sending heartbeat                       │
│  t=500 ms    SafetyTask::update() detects timeout              │
│              → EN pin = LOW (immediate GPIO write)              │
│              → Fault raised                                     │
│              Total latency: 500 ms + <2.5 ms (next cycle)      │
│                                                                  │
│  CRITICAL PROPERTY:                                             │
│  Even if ROS2 crashes, motors MUST disable within 500 ms!      │
│                                                                  │
│  ═══════════════════════════════════════════════════════════   │
│                                                                  │
│  FASTEST PATH: Enable motors after ROS2 connects              │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                  │
│  t=0 ms      ROS2 sends "HB:0,0"                               │
│  t=<1 ms     UART RX interrupt → buffer filled                 │
│  t=2.5 ms    Control loop cycle starts                          │
│              → HeartbeatReceiver::update() parses packet      │
│              → SafetyTask checks heartbeat_ok = TRUE           │
│              → EN pin = HIGH (motors enabled)                  │
│              Total latency: 2.5 - 10 ms depending on timing   │
│                                                                  │
│  CRITICAL PROPERTY:                                             │
│  ROS2 heartbeat affects motor enable within one control cycle! │
│                                                                  │
│  ═══════════════════════════════════════════════════════════   │
│                                                                  │
│  SLOWEST PATH: Telemetry collection & USB transmission         │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                  │
│  t=0 ms      Motor positions updated                            │
│  t=2.5 ms    ServoManager completes                             │
│  t=2.5 ms    TelemetryCollector::collect() gathers state      │
│  t=100 ms    HeartbeatTransmitter TX interval elapsed          │
│              → Format JSON                                      │
│              → Check USB buffer                                 │
│              → Write to USB (if space available)               │
│  t=105 ms    ROS2 receives feedback                            │
│              Total latency: "state → feedback" = ~100 ms       │
│                                                                  │
│  PROPERTY:                                                      │
│  Telemetry delay is much slower than control (expected, OK).   │
│  But motor disable is FAST (immediate safety response).        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Summary: Architecture at a Glance

| Aspect | Specification |
|:-------|:--------------|
| **Control Loop Frequency** | 400 Hz (2.5 ms period) |
| **Heartbeat Timeout** | 500 ms (ROS2 safety check) |
| **Motor Enable Latency** | 1 cycle (2.5 ms) |
| **Telemetry Update Rate** | 10 Hz (100 ms) |
| **Memory Usage** | 6.1% RAM (20 KB / 320 KB) |
| **Flash Usage** | 10.0% (335 KB / 3.3 MB) |
| **Stepper Drivers** | 4× TMC2209 via UART |
| **Servo Motors** | 3× MG996R via LEDC PWM |
| **Communication** | USB-CDC (ASCII protocol) |
| **Power Supply** | 24V input + onboard 6.6V (XL4016) + 3.3V (MP1584) |
| **Safety Mechanism** | Dual heartbeat + fault latch |
| **OS** | FreeRTOS on core 0 @ 240 MHz |

---

**This Visual Architecture Guide completes the SixEyes firmware documentation. Ready for hardware deployment! 🚀**
