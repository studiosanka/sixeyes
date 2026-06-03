# SixEyes Hardware Validation & Testing Procedures

Complete reference for hardware bring-up testing, validation, and qualification procedures.

---

## Table of Contents

1. [Pre-Testing Checklists](#pre-testing-checklists)
2. [Unit-Level Testing](#unit-level-testing)
3. [Integration Testing](#integration-testing)
4. [System-Level Testing](#system-level-testing)
5. [Failure Mode Testing](#failure-mode-testing)
6. [Performance Validation](#performance-validation)
7. [Production Readiness](#production-readiness)

---

## Pre-Testing Checklists

### Mode Selection Validation

- [ ] Confirm target mode before test session:
  - VLA Inference (`OPERATION_MODE=1`)
  - Teleoperation (`OPERATION_MODE=2`)
- [ ] If Teleoperation mode:
  - Leader ESP32 is flashed and streaming `JOINT_STATE`
  - Laptop bridge is running and forwarding packets
  - Follower emits `TELEMETRY_STATE` stub or full telemetry

### Hardware Assembly Verification

- [ ] **Power Supply Checks**
  - 24V PSU: Output 24V ± 1V under full load (6A)
  - 6.6V servo rail (XL4016): Output 6.6V ± 0.3V under load
  - 3.3V logic rail (MP1584): Output 3.3V ± 0.1V under load
  - No reverse polarity (check with multimeter)
  - All capacitors present and correct voltage rating
  - No solder bridges visible under magnification

- [ ] **Motor Connections**
  - All 4 steppers connected to respective TMC2209 drivers
  - Pin mapping verified (A+, A-, B+, B-)
  - UART connections: TX/RX to ESP32 Serial2
  - No loose connectors (pull test with 5 N force)

- [ ] **Servo Connections**
  - Three servos on GPIO 35/36/37 (verify with schematic)
  - Power and ground distributed with capacitors
  - Signal wires not crossing power lines
  - Connectors fully seated and confirmed with visual inspection

- [ ] **Communication Wiring**
  - USB-CDC: Micro-USB connector functional
  - Test with: `dmesg | grep ttyUSB` (Linux) or Device Manager (Windows)
  - UART2 optional: TX/RX routed appropriately
  - Ground connections: Star topology verified

- [ ] **Safety Circuit**
  - EN pin (GPIO 6) connected to all TMC2209 EN lines
  - FAULT pins daisy-chained correctly
  - Pull-up resistors present on FAULT
  - Watchdog timer circuit (if present) verified

### Firmware Verification

- [ ] **Build Status**
  - Latest firmware compiles without warnings
  - Memory: < 50% RAM used, < 50% Flash used
  - Binary size acceptable (~335 KB)

- [ ] **Flash Verification**
  - Bootloader present
  - App partition has valid firmware
  - Partition table correct (check with `esptool.py read_flash`)

---

## Unit-Level Testing

### Test 1: Power-On Self-Test

**Objective**: Verify ESP32 boots and initializes all modules

**Procedure**:
```
1. Disconnect all motors and servos (safety)
2. Connect USB-CDC only (no external power yet)
3. Open serial monitor: 115200 baud
4. Observe startup sequence

Expected Output:
═════════════════════════════════════════════
rst:0x1 (POWERON_RESET),boot:0xd (SPI_FAST_FLASH_BOOT)
ESP-IDF v4.4.7

SixEyes follower_esp32 starting
[HAL_GPIO] Initializing                    ← Each module initializes
[TMC2209Driver] Initialized
[MotorController] Initialized
[ServoManager] Initialized
[SafetyTask] initialized
[MotorControlScheduler] Initialized
Initialization complete; control loop running on FreeRTOS

[Waiting for ROS2 heartbeat...]
═════════════════════════════════════════════

Acceptance:
✅ All modules initialize (no error messages)
✅ Control loop starts (mentionned in output)
✅ Waits for heartbeat (ready for commands)
```

**Troubleshooting**:
| Symptom | Cause | Solution |
|:--------|:------|:---------|
| No serial output | USB driver missing | Install CH340 driver |
| Reboot loop | Bad firmware | Reflash with esptool.py |
| Module init fails | Missing dependency | Check lib_deps in platformio.ini |

---

### Test 2: GPIO & LED Blink

**Objective**: Verify basic GPIO I/O functionality

**Procedure** (if LED on GPIO present):
```
1. Send dummy JSON message: {"cmd":"HEARTBEAT","seq":0}
2. Observe LED behavior (blink pattern = parsing success)
3. Or monitor EN pin with oscilloscope
```

**Expected**:
- EN pin toggles within 10 ms of heartbeat reception
- No noise or ringing on signal

---

### Test 3: UART Loopback

**Objective**: Verify USB-CDC communication integrity

**Procedure**:
```bash
# Linux/Mac: Use socat for loopback
(cat echo '{"cmd":"HEARTBEAT","seq":1}' ; sleep 0.1) | nc localhost 9000

# Or manual test with screen/picocom
screen /dev/ttyUSB0 115200
Type: HB:0,1
Observe: [SafetyTask] EN pin HIGH (response received)
```

**Expected**:
- Command received and echoed back
- No corruption or character loss
- Latency < 10 ms

---

## Integration Testing

### Test 3A: Teleoperation Link Validation (Leader → Bridge → Follower)

**Objective**: Verify serial chain for teleoperation mode.

**Procedure**:
```
1. Set follower firmware to OPERATION_MODE=2 and flash.
2. Flash leader_esp32 and start serial monitor.
3. Run laptop bridge:
  python sixeyes/tools/teleoperation_bridge.py --leader-port COMx --follower-port COMy
4. Verify bridge stats:
  - packets_forwarded increments
  - telemetry_in increments
```

**Acceptance**:
- Bridge forwards JOINT_STATE without sustained drops
- Follower returns TELEMETRY_STATE
- No serial disconnect/reset loops during 5-minute run

### Test 4: Motor Enable/Disable Sequence

**Objective**: Verify EN pin control and motor response

**Hardware Setup**:
- Only 24V PSU connected (no load on motors)
- No actual motor motion expected (just power up)

**Procedure**:
```
1. Monitor EN pin with oscilloscope (3.3V logic level)
2. Send commands:
   
   # Enable motors
   {"cmd":"ENABLE_MOTORS","seq":1,"enable":true}
   Expected: EN pin → LOW_TO_HIGH transition (enabled)
   
   # Disable motors
   {"cmd":"ENABLE_MOTORS","seq":2,"enable":false}
   Expected: EN pin → HIGH_TO_LOW transition (disabled)
   
3. Repeat 10 times, check for consistency
```

**Acceptance**:
- EN pin transitions are clean (no bouncing)
- Command latency < 5 ms
- Repeatable 100% of the time

---

### Test 5: Heartbeat Timeout Safety

**Objective**: Verify motors disable on heartbeat timeout

**Procedure**:
```
1. Send heartbeat: {"cmd":"HEARTBEAT","seq":1}
2. Observe EN pin → HIGH (motors enabled)
3. Wait 510 ms without sending another heartbeat
4. Observe EN pin → LOW (safety disable)

Expected Timeline:
t=0 ms:   Heartbeat received, EN=HIGH
t=510 ms: Timeout triggers, EN=LOW
t=600 ms: Send new heartbeat, EN=HIGH (recovery)

Repeat 5 times, verify timing consistency ±50 ms
```

**Acceptance**:
- Timeout always occurs between 500-550 ms
- Motors disable reliably on timeout
- Recovery works after new heartbeat

---

### Test 6: Fault Detection Circuit

**Objective**: Verify fault input causes motor disable

**Hardware Setup**:
```
Temporarily disconnect one TMC2209 EN pin from ESP32 GPIO6.
Connect GPIO pin to:
  - Pin to 3.3V (normal) = no fault
  - Pin to GND via 470Ω (fault) = fault detected
```

**Procedure**:
```
1. Heartbeat enabled, EN pin = HIGH
2. Simulate fault: Connect GPIO to GND
3. Observe EN pin → LOW within 10 ms
4. Clear fault, EN pin → HIGH (if heartbeat still valid)
5. Repeat with different fault types
```

**Acceptance**:
- Fault detection latency < 10 ms
- Motors disable immediately
- No false triggers

---

## System-Level Testing

### Test 7: Motor Motion Control

**Objective**: Verify motor position tracking via PID

**Hardware Setup**:
- Connect 24V PSU to drivers
- Mount lightweight test load (< 1 Nm)
- Attach rotary encoder to one motor (for feedback)

**Procedure**:
```bash
# Send motor target commands
{"cmd":"MOTOR_TARGET","seq":1,"targets":[0,0,0,0]}
# Wait 2 seconds for motion to settle
# Measure actual position (should be ~0°)

{"cmd":"MOTOR_TARGET","seq":2,"targets":[90,90,90,90]}
# Wait 2 seconds
# Measure actual position (should be ~90°)

{"cmd":"MOTOR_TARGET","seq":3,"targets":[180,180,180,180]}
# Verify position ~ 180°
```

**Acceptance Criteria**:
- Position error < 5° at steady-state
- Settling time < 3 seconds
- No oscillation or overshoot

---

### Test 8: Servo Motion Control

**Objective**: Verify servo position tracking

**Hardware Setup**:
- Verify 6.6V rail from onboard XL4016 is present at servo headers
- Attach visual target (e.g., printed degree scale)

**Procedure**:
```bash
# Send servo targets (PWM microseconds)
{"cmd":"SERVO_TARGET","seq":1,"pwm_us":[1000,1000,1000]}
# Observe: All servos move to fully counter-clockwise

{"cmd":"SERVO_TARGET","seq":2,"pwm_us":[1500,1500,1500]}
# Observe: All servos move to neutral (90°)

{"cmd":"SERVO_TARGET","seq":3,"pwm_us":[2000,2000,2000]}
# Observe: All servos move to fully clockwise

# Test individual servo control
{"cmd":"SERVO_TARGET","seq":4,"pwm_us":[1000,1500,2000]}
# Each servo should move to different position
```

**Acceptance**:
- Servo response time < 100 ms
- No jitter or vibration
- Repeatable positioning ±5°
- All three servos respond independently

---

### Test 9: Telemetry Stream

**Objective**: Verify status updates are sent correctly

**Procedure**:
```bash
# Enable high-rate telemetry
{"cmd":"TELEMETRY_RATE","seq":1,"hz":50}

# Monitor serial output for 5 seconds
# Should see ~250 messages

# Example messages:
# MOTOR_STATUS: {"cmd":"MOTOR_STATUS","motor":0,"pos":0.0,"vel":0,"current":0}
# SERVO_STATUS: {"cmd":"SERVO_STATUS","servo":0,"pwm_us":1500}
# STATUS_HEARTBEAT: {"cmd":"STATUS_HEARTBEAT","fault":0,"motors_en":1,"ros2_alive":1}
```

**Acceptance**:
- Telemetry messages arrive at > 90% of requested rate
- No malformed JSON
- Timestamps monotonically increase
- Data values within expected ranges

---

## Failure Mode Testing

### Test 10: Overcurrent Protection

**Objective**: Verify motor disable on overcurrent

**Procedure**:
```
1. Reduce PID gains to make motor more aggressive
2. Apply mechanical load (blocked rotor test)
3. Send motor target command
4. Observe current rise in TMC2209 status register
5. When current exceeds threshold (~1500 mA):
   - Device raises FAULT signal
   - SafetyTask detects and disables EN pin
```

**Expected**:
- Fault detected within 100 ms
- Motors disabled safely
- No damage to MOSFETs

---

### Test 11: Thermal Shutdown

**Objective**: Verify temperature protection

**Procedure** (requires thermal chamber or heating):
```
1. Continuous motor operation (100% PWM)
2. Monitor driver temperature (via temp sensor)
3. When temp reaches 150°C:
   - Driver likely raises FAULT
   - SafetyTask disables motors
   
Alternative: 
- Reduce thermal dissipation (cover heatsink)
- Run at full current with load
- Verify shutdown within safe limits
```

---

### Test 12: Communication Loss Recovery

**Objective**: Verify graceful handling of UART errors

**Procedure**:
```bash
# Simulate bad JSON
echo '{invalid json}' > /dev/ttyUSB0
Expected: Error logged, message ignored

# Simulate command with missing field
echo '{"cmd":"MOTOR_TARGET"}' > /dev/ttyUSB0
Expected: Error (missing "targets"), no motor motion

# Simulate rapid commands
for i in {1..100}; do
  echo '{"cmd":"HEARTBEAT","seq":'$i'}' > /dev/ttyUSB0
done
Expected: All parsed, no buffer overflows
```

**Acceptance**:
- System doesn't crash on bad input
- Graceful error messages
- Recovers to normal operation

---

## Performance Validation

### Test 13: Control Loop Frequency

**Objective**: Verify 400 Hz deterministic control loop

**Procedure**:
```
1. Add timing marker in control loop
2. Log timestamps every cycle (1000 cycles)
3. Calculate actual frequency

From telemetry:
STATUS_HEARTBEAT message includes "freq": 400.2

Check against specification: 400 ± 5 Hz
```

**Acceptance**:
- Measured frequency ≥ 395 Hz
- Jitter < ±5 Hz
- Consistent across temperature range

---

### Test 14: Latency Measurement

**Objective**: Verify end-to-end command latency

**Hardware**: Oscilloscope with dual channels

**Procedure**:
```
1. Channel A: Pulse generator sending command marker
2. Channel B: EN pin monitoring
3. Send: {"cmd":"ENABLE_MOTORS","seq":1,"enable":true}
4. Measure time from command reception to EN pin response

Expected: Command → Motor Enable: 2-5 ms
```

---

### Test 15: Memory Profiling

**Objective**: Verify no memory leaks or excessive allocation

**Procedure**:
```cpp
// During runtime, periodically query heap
uint32_t heap_start = esp_get_free_heap_size();

// Run for 1 hour
// Periodically log: esp_get_free_heap_size()

// Expected: Heap usage stable (no drift > 5%)
```

---

## Production Readiness

### Final Sign-Off Checklist

- [ ] **All Unit Tests Pass**
  - Zero failures in test_runner
  - All edge cases covered

- [ ] **Hardware Integration**
  - Motors respond to commands
  - Servos track targets
  - Telemetry streams reliably

- [ ] **Safety Verification**
  - Heartbeat timeout works
  - Fault detection operational
  - EN pin control reliable

- [ ] **Performance Targets**
  - Control loop: ≥ 395 Hz
  - Latency: < 10 ms
  - Memory: < 70% heap usage

- [ ] **Documentation Complete**
  - All test results documented
  - Failure analysis completed
  - Known issues listed

- [ ] **Code Quality**
  - No compiler warnings
  - Static analysis passed
  - Code reviewed

### Sign-Off Template

```
SixEyes Hardware Validation Sign-Off
═════════════════════════════════════

Hardware: ESP32-S3 DevKitC-1 Rev 1.2
Serial: [________________]
Date: February 25, 2026
Tester: [________________]

Unit Tests:     ✅ PASS
Integration:    ✅ PASS
Fault Testing:  ✅ PASS
Performance:    ✅ PASS

Memory Report:
  RAM Used:  6.1% (20,064 / 327,680 bytes)
  Flash Used: 10.0% (335,257 / 3,342,336 bytes)

Test Coverage: 15 major test scenarios
Defects Found: 0
Status: ✅ APPROVED FOR DEPLOYMENT

Tester Signature: _________________
Date: _________________
```

---

**Hardware validation baseline complete. Continue iterative bring-up and regression checks. 🚀**
