# SixEyes Flashing & Deployment Guide

Complete instructions for programming the ESP32-S3 firmware and deploying to hardware.

This guide now covers **dual-mode firmware setup**:
- **VLA Inference mode**: Laptop ROS2 → `follower_esp32`
- **Teleoperation mode**: `leader_esp32` → Laptop bridge → `follower_esp32`

---

## Table of Contents

1. [Pre-Deployment Checklist](#pre-deployment-checklist)
2. [Firmware Build & Compilation](#firmware-build--compilation)
3. [Flashing Methods](#flashing-methods)
4. [Serial Monitor & Diagnostics](#serial-monitor--diagnostics)
5. [Hardware Bring-Up](#hardware-bring-up)
6. [Troubleshooting](#troubleshooting)
7. [Release & Version Management](#release--version-management)

---

## Pre-Deployment Checklist

### Mode Selection Checklist

- [ ] **Select deployment mode**:
  - `MODE_VLA_INFERENCE` (default) for laptop/ROS2 task execution
  - `MODE_TELEOPERATION` for leader/follower mirroring and data collection

- [ ] **Set follower mode in build flags** (`sixeyes/firmware/follower_esp32/platformio.ini`):
  ```ini
  build_flags =
    -DCORE_MHZ=240
    -DOPERATION_MODE=1   ; 1=VLA_INFERENCE, 2=TELEOPERATION
  ```

- [ ] **If teleoperation mode**:
  - Build/flash `sixeyes/firmware/leader_esp32`
  - Run laptop bridge: `python sixeyes/tools/teleoperation_bridge.py --leader-port COMx --follower-port COMy`

### Development Environment

- [ ] **Git repository cloned**:
  ```bash
  git clone https://github.com/SixEyes-Open-Source/sixeyes.git
  cd sixeyes/firmware/follower_esp32
  ```

- [ ] **PlatformIO installed**:
  ```bash
  pip install platformio
  # Or: https://platformio.org/install  
  ```

- [ ] **Python 3.7+ available**:
  ```bash
  python --version    # Should be 3.7+
  ```

- [ ] **Git user configured**:
  ```bash
  git config --global user.email "your_email@example.com"
  git config --global user.name "Your Name"
  ```

### Hardware Preparation

- [ ] **Follower: ESP32-S3 DevKitC-1 board** (or ESP32-S3 SuperMini)
  - Identify USB port for programming
  - Good quality USB cable available (not cheap knockoff)

- [ ] **Leader: ESP32-C6 SuperMini board**
  - Identify USB port for programming
  - Good quality USB cable available (not cheap knockoff)

- [ ] **Wiring complete**:
  - Stepper motors connected to TMC2209 drivers
  - Servo motors connected to GPIO via PWM
  - Star grounding done
  - All power supplies assembled

- [ ] **Pre-power hardware tests**:
  - No solder bridges visible
  - All connectors fully seated
  - Motor coil polarity verified (dry test with battery)
  - Servo range confirmed (multimeter or oscilloscope)

### Firmware Prerequisites

- [ ] **Latest firmware source available**:
  ```bash
  git status  # Should show working directory clean
  git log --oneline -1  # Latest commit hash
  ```

- [ ] **Dependencies installed** (platformio handles this):
  ```bash
  cd sixeyes/firmware/follower_esp32
  pio run --list-targets  # Should list esp32-s3
  ```

---

## Firmware Build & Compilation

### Dual-Target Build Matrix

| Target | Purpose | Build Command |
|:-------|:--------|:--------------|
| `follower_esp32` | Main control firmware (VLA or Teleoperation mode) | `cd sixeyes/firmware/follower_esp32 && pio run` |
| `leader_esp32` | Teleoperation JOINT_STATE streamer | `cd sixeyes/firmware/leader_esp32 && pio run` |

### Method 1: PlatformIO CLI (Recommended for CI/CD)

#### Build Leader Streamer (Teleoperation)

```bash
cd sixeyes/firmware/leader_esp32
pio run

# Output: .pio/build/leader_esp32/firmware.bin
```

**Expected Output**:
```
Processing leader_esp32 (platform: espressif32; board: esp32-c6-devkitc-1; ...)
Building in release mode
====== BUILD SUCCESSFUL ======
```

#### Build Only (No Flashing)

```bash
cd sixeyes/firmware/follower_esp32

# Build firmware
pio run

# Output: .pio/build/follower_esp32/firmware.bin
#         .pio/build/follower_esp32/firmware.elf
```

**Expected Output**:
```
Processing follower_esp32 (platform: espressif32; board: esp32-s3-devkitc-1; ...)
Building in release mode
RAM:   [=         ]   6.1% (used 20064 bytes from 327680 bytes)
Flash: [=         ]  10.0% (used 335257 bytes from 3342336 bytes)
====== BUILD SUCCESSFUL in 7.49s ======
```

**Meaning**:
- ✅ Compilation success
- ✅ Memory usage excellent (6.1% RAM, 10% Flash)
- ✅ Ready for flashing

#### Build with Clean (Full Rebuild)

```bash
# Clean build artifacts
pio run --target clean

# Full rebuild
pio run

# Use this if you:
# - Change configuration files
# - Update dependencies
# - Have stale object files
```

#### Verbose Compilation (Debug Issues)

```bash
# Show all compiler commands and warnings
pio run --verbose

# Use this if:
# - Compilation fails with cryptic error
# - Want to understand build process
# - Debugging linker issues
```

#### Build Specific Target (Advanced)

```bash
# Build bootloader only
pio run --target bootloader

# Build app partition only  
pio run --target app

# Build partition table
pio run --target partitions

# Normal: builds app (most common)
```

---

### Method 2: VSCode IDE Integration

#### Install PlatformIO Extension

1. **Open VSCode Extensions (Ctrl+Shift+X)**
2. **Search**: "PlatformIO IDE"
3. **Install**: Official Microsoft/PlatformIO extension
4. **Reload**: VSCode will install dependencies

#### Build from IDE

1. **Click PlatformIO icon** (alien) in left sidebar
2. **Expand project**: `sixeyes/firmware/follower_esp32`
3. **Build**:
   - Click: `Build` → watch output below
   - Shortcut: `Ctrl+Alt+B` (Windows/Linux) or `Cmd+Alt+B` (Mac)

#### Run Tasks

```
PlatformIO Home icon (left sidebar)
  └─ Current Project → follower_esp32
     ├─ Build
     ├─ Upload  
     ├─ Monitor (serial)
     ├─ Clean
     └─ Remove
```

---

### Method 3: ArduinoIDE (Not Recommended, Legacy)

Arduino IDE is NOT recommended for SixEyes (tool pain, limited debugging). Use PlatformIO instead.

---

## Flashing Methods

### Teleoperation Deployment Order (Phase 3)

1. Flash `leader_esp32`
2. Flash `follower_esp32` with `OPERATION_MODE=2`
3. Start laptop bridge utility (`sixeyes/tools/teleoperation_bridge.py`)
4. Validate `JOINT_STATE` and `TELEMETRY_STATE` traffic in serial logs

### Method 1: PlatformIO USB Auto-Flash (Easiest)

#### Prerequisites

- [ ] ESP32-S3 connected via USB
- [ ] Device appears in `/dev/ttyUSB*` (Linux) or `COM*` (Windows)
- [ ] Firmware built (`pio run` completed)

#### Flash Command

```bash
# Automatic port detection + full flash
pio run --target upload

# Or all-in-one: build + flash
pio run --target upload --verbose
```

**What Happens**:
1. Detects USB port automatically
2. Puts ESP32-S3 into bootloader mode (automatic reset)
3. Erases flash memory
4. Writes bootloader + firmware
5. Starts application

**Expected Output**:
```
esptool.py v4.9.0
Serial port COM3
Entering download mode, resetting...
Chip ID: 0x12ab
Uploading 335 KB...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

#### Multiple Boards on Same Machine

```bash
# List available ports
pio device list

# Specify port explicitly
pio run --upload-port COM4 --target upload

# Or set in platformio.ini:
# [env:follower_esp32]
# upload_port = COM4
```

---

### Method 2: Manual Flashing with esptool.py

#### When to Use

- [ ] Auto-flash fails
- [ ] Non-standard board
- [ ] Want explicit control
- [ ] CI/CD pipeline needs portability

#### Installation

```bash
pip install esptool
# Or: already included with PlatformIO
```

#### Manual Flash Steps

```bash
# 1. Put board in boothold mode
#    Hold BOOT button + press RESET (or from firmware via RTS pulse)

# 2. Create flash command
python -m esptool write_flash \
  --chip esp32s3 \
  --port /dev/ttyUSB0 \
  --baud 921600 \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin

# or shortened:
esptool.py -p COM3 write_flash 0x10000 firmware.bin

# 3. Wait for "Hash of data verified"
# 4. Press RESET button to start application
```

#### Build Directory

Binaries are in:
```
.pio/build/follower_esp32/
├── bootloader.bin      (optional, only if re-flashing bootloader)
├── partitions.bin      (optional)
├── firmware.bin        (main application - THIS IS WHAT YOU NEED)
├── firmware.elf        (for debugging)
└── firmware.map        (symbol map)
```

---

### Method 3: UF2 Bootloader (if available)

#### Requires

- [ ] ESP32-S3 with native USB bootloader (or UF2 installed)
- [ ] Not all boards support this (Advanced, optional)

#### Flash Steps

```bash
# Convert firmware to UF2 format
pio run --target upload --upload-protocol=uf2

# Drag firmware.uf2 to ESP32 mount point (like USB drive)
# Board auto-flashes and reboots
```

**Note**: Most ESP32-S3 DevKits don't have this. Standard USB bootloader is more common.

---

### Method 4: Cloud OTA Updates (Advanced, Future)

```bash
# NOT YET IMPLEMENTED
# Future feature: firmware updates via ROS2 topic
#
# Process:
#   1. Upload firmware.bin to cloud storage
#   2. ROS2 publishes download URL
#   3. ESP32 requests download over WiFi
#   4. Verifies signature
#   5. Writes to OTA partition
#   6. Reboots with new firmware
```

---

## Serial Monitor & Diagnostics

### Opening Serial Monitor

#### Method 1: PlatformIO Monitor

```bash
# Auto-detect port + open monitor
pio device monitor

# Or with explicit port:
pio device monitor --port /dev/ttyUSB0 --baud 115200

# Expected output from firmware:
# SixEyes follower_esp32 starting
# [HAL_GPIO] Initializing
# [MotorController] Initialized
# ...
# Initialization complete; control loop running on FreeRTOS
```

#### Method 2: Linux/Mac Terminal

```bash
# picocom (install: sudo apt install picocom)
picocom /dev/ttyUSB0 -b 115200

# Or minicom
minicom -D /dev/ttyUSB0 -b 115200

# Or screen (install: sudo apt install screen)
screen /dev/ttyUSB0 115200

# Or cu (built-in on Mac, apt install cu on Linux)
cu -l /dev/ttyUSB0 115200
```

#### Method 2: Windows PowerShell

```powershell
# List ports
[System.IO.Ports.SerialPort]::GetPortNames()

# PowerShell script
$port = new-Object System.IO.Ports.SerialPort COM3,115200,None,8,one
$port.Open()
while ($port.IsOpen) { 
  if ($port.BytesToRead) { 
    Write-Host -NoNewline $port.ReadExisting() 
  } 
}
$port.Close()
```

#### Method 3: VSCode

1. **Install**: "Serial Monitor" extension
2. **Command Palette**: `Ctrl+Shift+P` → "Serial Monitor: Open"
3. **Select port**: COM3 (or auto-detect)
4. **Set baud**: 115200

---

### Diagnostic Output

#### Expected Startup Sequence

```
Reset ──→ Bootloader ──→ FreeRTOS ──→ application

SERIAL OUTPUT:
═════════════

rst:0x1 (POWERON_RESET),boot:0xd (SPI_FAST_FLASH_BOOT)
mode:DIO, clock div:1
load:0x4037cff4,len:0x1fbc
load:0x4037e060,len:0x2c88
load:0x40380400,len:0x1ea0
entry 0x4037e068
[0;32mI (30) boot: ESP-IDF v4.4.7-dirty 2nd stage bootloader[0m
[0;32mI (30) boot: compile time 12:34:56[0m

SixEyes follower_esp32 starting         ← FirmwareSTART
[HAL_GPIO] Initializing                 ← Module init
[TMC2209Driver] Initialized
[MotorController] Initialized
[ServoManager] Initialized
[SafetyTask] initialized                ← Safety system ready
  - Heartbeat timeout: 500 ms
  - Motors disabled by default
  - Fault latch enabled (clear required)
  - ROS2 heartbeat bridge: ENABLED
[MotorControlScheduler] Initialized     ← Task scheduling ready
[MotorControlScheduler] starting control loop task...
[MotorControlScheduler] control loop task created successfully
Initialization complete; control loop running on FreeRTOS  ← READY

[Now waiting for ROS2 heartbeat...]
```

#### Real-Time Status Output

Once running, you'll see periodic status updates:

```
[SafetyTask] EN pin HIGH - motors enabled      ← Heartbeat received
SB:0,1,1                                        ← Status packet (10 Hz)
SB:0,1,1
...

[SafetyTask] HEARTBEAT_TIMEOUT raised          ← Timeout triggered  
[SafetyTask] EN pin LOW - motors disabled      ← Safety shutdown
SB:1,0,0                                        ← Fault status
SB:1,0,0
```

#### Manual Testing

Send heartbeat from serial monitor:

```
Type: HB:0,0
Press: Enter
Expected: [SafetyTask] EN pin HIGH - motors enabled
```

---

### Exiting Serial Monitor

| Tool | Exit Command |
|:-----|:------------|
| picocom | Ctrl+A then Ctrl+X |
| minicom | Ctrl+A then Z then X |
| screen | Ctrl+A then `:quit` |
| cu | `~.` (tilde-dot) |
| PowerShell | Ctrl+C (close window) |
| VSCode Serial Monitor | Click close button |

---

## Hardware Bring-Up

### Power-On Verification (First Time)

#### Step 1: USB Only

```
1. Connect ESP32 via USB (no other power yet)
2. LED should blink if present
3. Open serial monitor → should see startup messages
4. If no output: check USB cable, board may be DOA
```

#### Step 2: Add 24V Stepper Power

```
1. Plug in 24V PSU (motors still disabled by default)
2. Measure voltage at TMC2209 VDD: should be 24V ± 0.5V
3. Serial output should not change (isolated supplies)
4. Send heartbeat: EN pin should go HIGH
5. Stepper drivers should activate (no motion yet, just enable)
```

#### Step 3: Verify Onboard 6.6V/3.3V Rails

```
1. Keep 24V input connected (onboard bucks derive low-voltage rails)
2. Measure voltage at servo VCC: should be 6.6V ± 0.3V (XL4016 output)
3. Measure logic rail near ESP32/TMC2209 VIO: should be 3.3V ± 0.1V (MP1584 output)
4. Servos initialize to neutral/safe position
5. Ready for controlled testing
```

#### Step 4: Motor Startup Test (Unloaded)

```bash
# Terminal 1: Monitor output
pio device monitor

# Terminal 2: Send heartbeats
while true; do
  echo "HB:0,42"
  sleep 0.05  # 20 ms = 50 Hz
done > /dev/ttyUSB0

# OBSERVE:
# ✓ EN pin HIGH
# ✓ Motors spin (shouldn't move, but should step)
# ✓ Status packets appear (SB:0,1,1)
# ✓ No fault alerts
# ✓ No thermal warnings (drivers should be cool)

# STOP: Ctrl+C in heartbeat terminal
# Motors should disable, see: EN pin LOW
# Fault should clear automatically
```

---

### Calibration & Tuning

#### Motor Orientation Verification

```bash
# For each motor (0-3), test individually:

# Only Motor 0 enabled (PDN0 pulled LOW):
# Send UART command to motor 0:
# Result: Motor 0 should step, others inactive

# Perform manual rotation test:
# - No load attached
# - Rotate shaft: should have detent torque
# - Rotate smoothly: no grinding or cogging
# - If jerky: swap motor coil A± or B±
```

#### Servo Neutral Position

```bash
# After power-up, all servos at 90° (neutral):
# 1. Measure PWM on GPIO35/36/37: should be 1500 µs
# 2. Servo arm should point forward/straight
# 3. If tilted: adjust mechanical trim (internal pot)
#    Or: adjust code offset in servo_manager.cpp

# For gripper: 90° = fully open (safe state)
#              0-45° = partially closed
#              45-180° = full grip
```

#### PID Controller Tuning

```cpp
// In motor_controller.h, current gains:
static constexpr float Kp = 2.0;   // Proportional
static constexpr float Ki = 0.05;  // Integral (anti-windup)
static constexpr float Kd = 0.1;   // Derivative

// Tuning procedure (advanced):
// 1. Increase Kp until oscillation starts
// 2. Back off Kp by 30%
// 3. Increase Ki for steady-state error
// 4. Add Kd for damping
// 5. Validate with scope: measure step response

// For SixEyes arm: defaults are good starting point
// Only tune if motion is sluggish or unstable
```

---

## Troubleshooting

### Build Failures

#### Error: "Cannot find board esp32-s3-devkitc-1"

```bash
# Solution: Update PlatformIO
pio upgrade

# Or specify platform explicitly in platformio.ini:
[env:follower_esp32]
platform = espressif32 @ 6.12.0
board = esp32-s3-devkitc-1
board_build.mcu = esp32s3
```

#### Error: "ESP-IDF version mismatch"

```bash
# Solution: Let PlatformIO manage it (don't install ESP-IDF manually)
pio run --target clean

# If still fails:
rm -rf .pio/
pio run  # Full rebuild from scratch
```

#### Compilation Errors: Undefined Symbol

```bash
# Error: undefined reference to `_Z....'
#
# Cause: Missing library or linker issue
# Solution:
#   1. Check platformio.ini [lib_deps] section
#   2. Verify all required libraries installed:
#      - TMCStepper (stepper drivers)
#      - FreeRTOS (usually included)
#   3. Full clean rebuild:

pio run --target clean
pio run --verbose

# If error persists: check git commit
# (may be incomplete merge)
```

---

### Flashing Failures

#### Error: "No Port Found" or "Cannot Open Port"

```bash
# Cause: USB not detected or wrong driver

# Windows:
#   1. Check Device Manager for "COM" ports
#   2. If missing: install CH340 driver
#      https://github.com/WCHSoftware/ch341ser_windows
#   3. If "Unknown Device": right-click → Update Driver
#   4. Select driver from USB info

# Linux:
#   $ ls -la /dev/ttyUSB*     # Check permissions
#   $ sudo usermod -aG dialout $USER  # Add to group
#   # Log out and back in

# Mac:
#   # Older boards may need:
#   # https://github.com/WCHSoftware/ch34x_install

# General:
#   - Try different USB cable
#   - Try different USB port
#   - Try different PC (temporarily test)
```

#### Error: "Failed to Connect to Espressif Device"

```bash
# Cause: Bootloader communication problem

# Solution 1: Manual bootload mode
#   1. Press BOOT + RESET on board
#   2. Keep BOOT held, release RESET
#   3. LED should be off or dim
#   4. Run flash command again
#   5. After completion: press RESET

# Solution 2: Slow baud rate
pio run --target upload --upload-speed 115200

# Solution 3: Check board
#   - Measure 3.3V on board: should be ~3.3V
#   - Check USB power: at least 500mA available
#   - Verify stable 24V input and onboard buck outputs if instability persists
```

#### Error: "Hash Mismatch" or "CRC Error"

```bash
# Cause: Corrupted firmware binary or USB noise

# Solution:
#   1. Verify .pio/build/.../firmware.bin is not corrupted
#      $ md5sum firmware.bin  # Should match expected
#   2. Try different USB cable (poor shielding?)
#   3. Rebuild firmware from clean:
pio run --target clean
pio run
pio run --target upload --verbose
```

---

### Runtime Failures

#### Serial Monitor Shows Garbage / Corrupt Text

```
✗ Wrong baud rate (using 9600 instead of 115200)
✗ Conflict with another application using COM port

Solution:
  1. Close all other serial apps (Arduino IDE, terminals, etc.)
  2. Try different baud rate in serial monitor:
     - 115200 (correct for ESP32-S3)
     - 921600 (sometimes shows alternative output)
  3. Replace USB cable (corruption = EMI)
```

#### Firmware Crashes on Startup

```
Serial Output:
  [many messages...]
  rst:0x8 (SW_CPU_RESET)
  Rebooting...

Cause: Fault in FreeRTOS task or assert triggered

Solution:
  1. Check initialization order in main.cpp
  2. Verify all modules init successfully (look for error messages)
  3. Check stack sizes (task stack overflow?)
  4. Full clean rebuild
```

#### Control Loop Missing (No Heartbeat Timeout)

```
Serial Output:
  SixEyes follower_esp32 starting
  [all modules initialized]
  [But no "EN pin" messages ever appear]

Cause: MotorControlScheduler task not created

Solution:
  1. Check FreeRTOS xTaskCreatePinnedToCore() result
     (return value checked?)
  2. Verify configTOTAL_HEAP_SIZE is large enough
  3. Check if all modules in control loop()
     (SafetyTask, MotorController, ServoManager)
```

#### Motors Don't Enable Despite Heartbeat

```
Serial Output:
  SB:0,0,1  ← ROS2 alive, but motors disabled
  SB:0,0,1

Cause: Hardware fault or safety latch active

Debug:
  1. Send manual heartbeat
  2. Check if motors enable within 1 cycle
  3. If not, check FaultManager status
     (add debug print to SafetyTask::update())
  4. Verify GPIO6 (EN pin) is wired correctly
```

---

## Release & Version Management

### Creating a Release Build

#### Versioning Scheme

```
Semantic Versioning: MAJOR.MINOR.PATCH

Examples:
  v1.0.0: Initial release with heartbeat hooks
  v1.0.1: Bug fix for timeout timing
  v1.1.0: New feature (e.g., advanced PID)
  v2.0.0: Major firmware rewrite (breaking changes)

For SixEyes:
  Current: v1.0.0 (stable, ROS2 heartbeat integrated)
  Next: v1.1.0 (unit tests + CI)
  Future: v2.0.0 (vision integration)
```

#### Build Release Artifact

```bash
# 1. Tag the commit
git tag -a v1.0.0 -m "Release: ROS2 heartbeat integration"
git push origin v1.0.0

# 2. Build final binary
cd sixeyes/firmware/follower_esp32
pio run --target clean
pio run

# 3. Copy artifacts
mkdir -p releases/v1.0.0
cp .pio/build/follower_esp32/firmware.bin releases/v1.0.0/
cp .pio/build/follower_esp32/firmware.elf releases/v1.0.0/
cp platformio.ini releases/v1.0.0/
cp src/modules/config/board_config.h releases/v1.0.0/

# 4. Create checksum
cd releases/v1.0.0
sha256sum *.bin > CHECKSUMS.txt

# 5. Create release notes
cat > RELEASE_NOTES.txt << EOF
SixEyes Firmware v1.0.0 - Release Notes
═══════════════════════════════════════

## Features
- 400 Hz deterministic control loop with FreeRTOS
- ROS2 heartbeat integration with safety hooks
- 4× NEMA23 stepper motor control via TMC2209
- 3× MG996R servo control via LEDC PWM
- USB-CDC telemetry streaming at 10 Hz

## Known Limitations
- No visual feedback from camera yet (placeholder)
- PID tuning values are defaults (may need adjustment)
- OTA updates not yet implemented

## Hardware Requirements
- ESP32-S3-WROOM-1 (or compatible)
- 24V PSU (6A minimum)
- Onboard XL4016 6.6V servo buck (from 24V input)
- Onboard MP1584 3.3V logic buck (from 24V input)
- TMC2209 stepper drivers (×4)
- MG996R servo motors (×3)

## Installation Instructions
1. Connect ESP32-S3 via USB
2. Run: pio run --target upload --upload-port COM3
3. Open serial monitor: pio device monitor
4. Send heartbeat: echo "HB:0,0" > /dev/ttyUSB0
5. Observe: EN pin should go HIGH

## Support
GitHub Issues: https://github.com/SixEyes-Open-Source/sixeyes/issues
Wiki: https://github.com/SixEyes-Open-Source/sixeyes/wiki

## Changelog
### v1.0.0 (Feb 25, 2026)
- Initial stable release
  * Safety task with heartbeat monitoring
  * Motor control with closed-loop PID
  * Servo manager with LEDC WM
  * ROS2 heartbeat protocol (ASCII format)
  * Complete documentation & guides
EOF
```

#### Upload Release to GitHub

```bash
# Using gh CLI:
gh release create v1.0.0 \
  --title "v1.0.0: ROS2 Heartbeat Integration" \
  --notes-file releases/v1.0.0/RELEASE_NOTES.txt \
  releases/v1.0.0/firmware.bin \
  releases/v1.0.0/firmware.elf \
  releases/v1.0.0/CHECKSUMS.txt

# Or manually on GitHub web:
# 1. Go to https://github.com/SixEyes-Open-Source/sixeyes
# 2. Releases → Create new release
# 3. Tag: v1.0.0
# 4. Upload binaries
# 5. Copy release notes
# 6. Publish
```

---

### Continuous Integration (GitHub Actions)

#### Auto-Build on Push

```yaml
# .github/workflows/platformio.yml

name: Build Firmware

on:
  push:
    branches: [ main, develop ]
    paths:
      - 'sixeyes/firmware/follower_esp32/src/**'
      - 'sixeyes/firmware/follower_esp32/platformio.ini'
      - '.github/workflows/platformio.yml'
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.9'
    
    - name: Install PlatformIO
      run: |
        python -m pip install --upgrade pip
        pip install platformio
    
    - name: Build Firmware
      run: |
        cd sixeyes/firmware/follower_esp32
        pio run
    
    - name: Check Memory Usage
      run: |
        cd sixeyes/firmware/follower_esp32
        pio run --target size
        # Fail if memory usage > 80%
    
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: firmware-binaries
        path: sixeyes/firmware/follower_esp32/.pio/build/follower_esp32/firmware.*
```

---

### Rollback Procedure

If a release has critical bugs:

```bash
# Identify last good version
git log --oneline | head -20

# Revert to previous version
git checkout v0.9.3  # Go back to known-good tag

# Build that version
pio run

# Flash to board
pio run --target upload

# Or: build complete new release fixing the bug
git checkout main
# [Fix bug in code]
git commit -am "fix: critical issue in v1.0.0"
git tag -a v1.0.1 -m "Hotfix for v1.0.0"
git push origin v1.0.1
```

---

## Final Checklist Before Deployment

- [ ] Firmware compiles without warnings: `pio run`
- [ ] Memory usage < 50%: ✓ (currently 6.1% RAM, 10% Flash)
- [ ] All modules initialized: `[Module] Initialized` in serial output
- [ ] Control loop running: `Initialization complete; control loop running`
- [ ] Hardware powered up correctly (24V input, 6.6V rail, 3.3V rail)
- [ ] Serial monitor shows status packets: `SB:0,1,1`
- [ ] Manual heartbeat test works: `echo "HB:0,0"` enables motors
- [ ] Motors respond to heartbeat (EN pin toggles)
- [ ] No thermal issues (drivers/PSU cool to touch)
- [ ] Documentation up to date
- [ ] Release tag created and pushed to GitHub
- [ ] Artifacts archived for posterity

---

**Firmware deployed! Ready for hardware testing. 🚀**

For questions or issues, refer to TROUBLESHOOTING section or GitHub Issues.
