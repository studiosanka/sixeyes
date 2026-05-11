# SixEyes Wiring & Hardware Assembly Guide

Complete reference for physically integrating the ESP32 controllers with motors, servos, and the onboard power architecture.

For dual-mode operation, hardware can be deployed in two configurations:
- **VLA Inference**: follower controller only
- **Teleoperation**: leader controller + follower controller + laptop bridge

---

## Table of Contents

1. [Parts List](#parts-list)
2. [Pinout Reference](#pinout-reference)
3. [Power Distribution](#power-distribution)
4. [Motor Wiring](#motor-wiring)
5. [Servo Wiring](#servo-wiring)
6. [Communication Wiring](#communication-wiring)
7. [Grounding Strategy](#grounding-strategy)
8. [Testing Checklist](#testing-checklist)
9. [Troubleshooting](#troubleshooting)

---

## Dual-Mode Hardware Topology

### VLA Inference Topology

```
Laptop ROS2 (USB)
  ↕
Follower ESP32-S3
  ↕
Motors + Servos
```

### Teleoperation Topology

```
Leader ESP32-C6 SuperMini (joint sensors) --USB--> Laptop Bridge --USB--> Follower ESP32-S3
                                     ↕
                                  Motors + Servos
```

**Teleoperation wiring additions**:
- Add one extra USB serial link for `leader_esp32`
- Route leader joint sensors (potentiometers/encoders) to leader ADC pins
- Keep follower motor/servo wiring unchanged

---

## Parts List

### Controller
- **Follower: ESP32-S3-WROOM-1 DevKit** (or ESP32-S3 SuperMini)
  - Dual-core 240 MHz
  - Native USB-CDC
  - 320 KB RAM, 8 MB Flash
  - GPIO: 3.3V logic

- **Leader: ESP32-C6 SuperMini**
  - Single-core RISC-V 160 MHz
  - Native USB Serial/JTAG
  - GPIO: 3.3V logic

### Stepper Motors & Drivers
- **4× NEMA23 bipolar steppers** (4-wire, 2.8A nominal)
  - Base rotation
  - Shoulder pitch (motor 1)
  - Shoulder roll (motor 2)
  - Elbow pitch (motor 3)
  - **Connectors**: 4-pin JST or terminal blocks
  
- **4× TMC2209 stepper drivers** (PDN_UART mode)
  - Integrated current sense (0.11Ω resistors installed)
  - StallGuard capability
  - **Pinout**: GND, VDD (24V), SLA (step), DIR, PDN
  - Note: PDN pin selects device on shared UART

### Servo Motors
- **3× MG996R servos** (digital, 6.6V rail, 0.17 sec/60°)
  - Wrist pitch (5-force)
  - Wrist yaw
  - Gripper control
  - **Connectors**: 3-pin header (GND, VCC, PWM)

### Power Architecture
- **24V 150W PSU input** (main rail, 6A capable)
  - Commonly found 24V industrial supplies
  - Must support 4× NEMA23 @ 1.5A each (6A total)
  - With barrel connector or terminal blocks

- **XL4016 buck converter on PCB** (servo rail)
  - Converts 24V input to 6.6V servo rail
  - Designed for high-current servo load

- **MP1584 buck converter on PCB** (logic rail)
  - Converts 24V input to 3.3V logic rail
  - Powers ESP32 logic + TMC2209 VIO domain
  
- **USB Cable** (for ESP32 power + programming)
  - Standard USB 3.0 A → micro-B or USB-C
  - Powers logic, does NOT power motors

### Wiring & Components
- **Wire gauge**:
  - 24V stepper bus: **12 AWG** (3mm²) minimum
  - 6.6V servo bus: **16 AWG** (1mm²) acceptable
  - 3.3V logic: **20 AWG** (0.5mm²)
  - Stepper UART: **22 AWG** (0.3mm²)
  
- **Connectors**:
  - JST-XH (100mils) for motors: 2-4 pin variants
  - 0.1" headers (2.54mm) for UART, GPIO
  - Power barrel connectors (5.5×2.1mm) optional
  - Terminal blocks (5mm pitch) for power distribution
  
- **Capacitors**:
  - **1000µF/35V** electrolytic (24V bulk, surge protection)
  - **1000µF/16V** electrolytic (6.6V servo rail bulk, near headers)
  - **100nF/16V** ceramic (bypass, near ESP32 VCC)
  
- **Other**:
  - Resistors: 100k/100k divider for battery voltage sensing (optional)
  - Fuses: 10A for 24V input bus, 5A for 6.6V servo rail (optional but recommended)
  - Breadboard or custom PCB for distribution point
  - Wire strippers, soldering iron, solder (60/40 Sn/Pb or lead-free)

---

## Pinout Reference

### ESP32-S3 Pin Assignments

```
┌────────────────────────────────────────────────────────┐
│  STEPPER CONTROL (TMC2209 drivers via PDN_UART)       │
├────────────────────────────────────────────────────────┤
│                                                         │
│  UART Communication (all 4 drivers on same bus):       │
│    GPIO17 ──→ Serial2 RX (TMC2209 Si input)          │
│    GPIO18 ──→ Serial2 TX (TMC2209 So output)         │
│    Speed: 115200 baud                                │
│                                                         │
│  Device Selection (PDN pins, active-low):            │
│    GPIO7  ──→ Motor 0 PDN (Base rotation)            │
│    GPIO11 ──→ Motor 1 PDN (Shoulder pitch)           │
│    GPIO15 ──→ Motor 2 PDN (Shoulder roll)            │
│    GPIO21 ──→ Motor 3 PDN (Elbow pitch)              │
│                                                         │
│  Motor Enable (all steppers):                         │
│    GPIO6  ──→ EN pin (active HIGH, drives all 4)    │
│                                                         │
└────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────┐
│  SERVO CONTROL (LEDC PWM, 50 Hz)                       │
├────────────────────────────────────────────────────────┤
│                                                         │
│    GPIO35 ──→ Servo 0 PWM (Pitch)                     │
│    GPIO36 ──→ Servo 1 PWM (Yaw)                       │
│    GPIO37 ──→ Servo 2 PWM (Gripper)                   │
│                                                         │
│  PWM Spec:                                             │
│    Frequency: 50 Hz                                    │
│    Pulse range: 500-2500 µs                           │
│    Resolution: 16-bit LEDC                           │
│                                                         │
└────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────┐
│  COMMUNICATION & DIAGNOSTICS                           │
├────────────────────────────────────────────────────────┤
│                                                         │
│  USB-CDC (native ESP32-S3):                           │
│    Micro-USB port ──→ Serial I/O                      │
│    Baud: 115200 (virtual)                             │
│    Format: ASCII heartbeat protocol                   │
│      RX: "HB:0,<seq>\n"                               │
│      TX: "SB:<fault>,<en>,<ros2>\n"                  │
│                                                         │
│  Optional LED:                                        │
│    GPIO46 ──→ Status LED (diagnostics)               │
│                                                         │
│  Optional ADC:                                        │
│    GPIO3  ──→ Battery voltage (divide by 2)          │
│    GPIO4  ──→ Current sense (TMC diagnostics)        │
│                                                         │
└────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────┐
│  POWER DISTRIBUTION                                    │
├────────────────────────────────────────────────────────┤
│                                                         │
│  VCC / GND (3.3V logic):                              │
│    GPIO: 3.3V (all GPIO operate at 3.3V)             │
│    USB or LDO regulated                               │
│                                                         │
│  Reserved / Strapping:                                │
│    GPIO1, GPIO2: Boot strapping pins                 │
│    GPIO8, GPIO9: Flash SPI (reserved)                │
│    GPIO42, GPIO43: UART console (optional)           │
│                                                         │
└────────────────────────────────────────────────────────┘
```

### TMC2209 Driver Pin Description

```
┌──────────────────────────────────────────────────────┐
│           TMC2209 Pin Mapping (PDN_UART Mode)       │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Pin 1 (GND):      Ground                           │
│  Pin 2 (VDD):      24V stepper supply               │
│  Pin 3 (SLA):      UART Si (shared bus)             │
│  Pin 4 (SOA):      UART So (shared bus)             │
│  Pin 5 (PDN):      Device select (GPIO to ESP32)   │
│                    Active low: pull to GND          │
│                    Idle high: leave floating        │
│  Pin 6 (GND):      Ground return                    │
│                                                      │
│  Configuration (hardware, not firmware):            │
│    ├─ Test pin grounded (always)                    │
│    ├─ Sense resistor: 0.11Ω between GND and motor  │
│    ├─ Flying cap across VDD-GND (100µF typical)    │
│    └─ UART protection: no extra filtering needed    │
│                                                      │
│  Operation:                                         │
│    • Default: PDN floating (device active)          │
│    • To disable device: Pull PDN to GND             │
│    • UART accessible on all 4 drivers             │
│    • CRC-8 calculation done automatically          │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## Power Distribution

### Main Power Bus Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                   POWER SUPPLY CONNECTIONS                      │
└─────────────────────────────────────────────────────────────────┘

24V STEPPER BUS (6A max)
═════════════════════════

    24V PSU
    +24V ────┬─── Filter Capacitor (1000µF/35V) ───┬─→ Power Bus
             │                                       │
             ├─ Thick wire (12 AWG) ─── ─ ─ ─ ──→ Distribution Point
             │
             └─ Return GND ── ═══════════════════════ ─ ─ ─ ─ → GND Bus

From Distribution Point → TMC2209 Driver Inputs:
  • VDD pin: 24V (tight tolerance, ±10%)
  • GND pin: Ground return (common star point)

STEPPER MOTORS:
  4× NEMA23 @ 1.5A each = 6A max bus current
  Connected via stepper drivers (not direct to bus)


6.6V SERVO BUS (XL4016, ~3A design target)
═══════════════════════════════════════════

    24V Input Bus
      │
      └─→ XL4016 Buck (on PCB) ──→ 6.6V Servo Bus
                                   │
                                   ├─ Filter Capacitor (≥1000µF/16V)
                                   ├─ Wire (16 AWG acceptable) → Servo Headers
                                   └─ Return GND → Common Star Ground

From Servo Bus → Servo Connectors:
  • VCC pin: 6.6V (target ±0.3V)
  • GND pin: Ground return (common star point)

SERVOS:
  3× MG996R class @ up to ~1A each (load-dependent)


3.3V LOGIC SUPPLY (MP1584 on PCB)
══════════════════════════════════

    24V Input Bus
      │
      └─→ MP1584 Buck (on PCB) ──→ 3.3V Logic Rail

Filter:
  • ≥22µF input + ≥22µF output at converter
  • 100nF bypass caps near MCU/logic loads

LOGIC SIGNALS:
  All GPIO: 3.3V
  TMC2209 UART: 3.3V (TTL compatible)
  Servo PWM: 3.3V (servos tolerate this)


EXTERNAL CONNECTION BLOCK (Ground Star Point)
════════════════════════════════════════════════

        24V PSU GND ──┐
                      ├─→ STAR POINT (chassis/PCB ground)
        6.6V rail GND ─┤
                      │
        USB GND ──────┤
                      │
        (No loops! Single point of reference)
                      │
                    From here:
                      ├─ Thick wire to ESP32 GND
                      ├─ Thick wire to TMC2209 GND inputs
                      ├─ Thick wire to servo power GND
                      └─ Return wire from motors/servos
```

### Recommended Power Supply Specifications

| Supply | Voltage | Current | Wire Gauge | Connector | Notes |
|:-------|:-------:|:-------:|:----------:|:---------:|:------|
| Main Input | 24V | 6A | 12 AWG (3mm²) | 5.5×2.1mm barrel | Feeds TMC2209 VM + onboard bucks |
| Servo Rail | 6.6V | 3A | 16 AWG (1mm²) | Terminal block | From onboard XL4016 buck |
| Logic Rail | 3.3V | 500mA+ | 20 AWG (0.5mm²) | PCB internal rail | From onboard MP1584 buck |

---

## Motor Wiring

### NEMA23 Stepper Motor Connection

```
┌────────────────────────────────────────────────────┐
│         NEMA23 STEPPER (4-wire Bipolar)           │
├────────────────────────────────────────────────────┤
│                                                    │
│  Connector (typical JST-XH 4-pin):                │
│    Pin 1: Motor A+ (red)                          │
│    Pin 2: Motor A- (blue)                         │
│    Pin 3: Motor B+ (yellow)                       │
│    Pin 4: Motor B- (green)                        │
│                                                    │
│  CRITICAL: DO NOT SWAP A/B PAIRS!                 │
│    (will cause motor to jitter instead of run)    │
│                                                    │
│  Connection to TMC2209:                           │
│    Motor A±  ──→ COIL A on driver                 │
│    Motor B±  ──→ COIL B on driver                 │
│                                                    │
│  Current Setting:                                 │
│    Standard NEMA23: 2.8A nominal                 │
│    Set VREF on TMC2209 for 1.5A running          │
│    Formula: AMPS = VREF × 2.0 (check datasheet) │
│    For 1.5A: Set VREF ≈ 0.75V potentiometer     │
│                                                    │
└────────────────────────────────────────────────────┘

ASSEMBLY FOR 4 MOTORS:
════════════════════════

Motor 0 (Base):
  JST-XH 4-pin ──→ TMC2209 Driver #0 COIL inputs
  TMC2209 PDN ──→ GPIO7 (pull to GND to disable)

Motor 1 (Shoulder Pitch):
  JST-XH 4-pin ──→ TMC2209 Driver #1 COIL inputs
  TMC2209 PDN ──→ GPIO11 (pull to GND to disable)

Motor 2 (Shoulder Roll):
  JST-XH 4-pin ──→ TMC2209 Driver #2 COIL inputs
  TMC2209 PDN ──→ GPIO15 (pull to GND to disable)

Motor 3 (Elbow):
  JST-XH 4-pin ──→ TMC2209 Driver #3 COIL inputs
  TMC2209 PDN ──→ GPIO21 (pull to GND to disable)

SHARED UART BUS (all 4 drivers):
  GPIO17 (RX) ──→ Si input on all TMC2209 boards
  GPIO18 (TX) ──→ So output on all TMC2209 boards
  
  Note: Si/So are UART daisy-chained internally.
        All drivers listen on same bus.
        PDN pin selection determines which responds.

MOTOR ENABLE:
  GPIO6 (EN) ──→ EN input on all 4 TMC2209 boards
               (pull HIGH to enable, LOW to disable)
```

### Proper Motor Polarity Testing

```
BEFORE POWERED-UP TESTING:
═════════════════════════════

1. Disconnect 24V power supply
2. Connect motor A+ and A- directly to battery (2-3V)
3. Motor shaft should rotate smoothly (no cogging)
4. If jittery: swap A± wires and test again
5. When smooth: mark correct polarity (red=+, black=-)
6. Repeat for B coil pair
7. Once confirmed, solder JST connectors

TIP: Use multimeter continuity to trace wires back to
     motor windings if color coding is unclear.

AFTER ASSEMBLY:
════════════════

If motor doesn't step:
  1. Check VREF pot setting (should be 0.75V for 1.5A)
  2. Verify UART communication (scope Si/So lines)
  3. Swap A± wires (coil polarity wrong)
  4. Swap B± wires (coil polarity wrong)
  5. Check PDN signal from ESP32 (should be HIGH/floating)
```

---

## Servo Wiring

### MG996R Servo Connection

```
┌────────────────────────────────────────────────────┐
│         MG996R SERVO (Digital, 50 Hz PWM)         │
├────────────────────────────────────────────────────┤
│                                                    │
│  Connector (3-pin header, 2.54mm spacing):        │
│    Pin 1: GND (brown, black)                      │
│    Pin 2: VCC (red)                               │
│    Pin 3: PWM (orange, yellow)                    │
│                                                    │
│  Signal Mapping:                                  │
│    500 µs  → -90° (far left)                      │
│    1500 µs → 0° (center, neutral)                 │
│    2500 µs → +90° (far right)                     │
│                                                    │
│  For 0-180° range (SixEyes):                      │
│    500 µs  → 0°                                   │
│    1500 µs → 90° (center, SAFE POWER-UP)         │
│    2500 µs → 180°                                │
│                                                    │
│  PWM Frequency: 50 Hz (20 ms period)             │
│    ├─ Pulse width: 0.5-2.5 ms (varies)           │
│    └─ Low period: fills rest of 20 ms            │
│                                                    │
│  Current Draw:                                    │
│    Idle: ~10 mA                                   │
│    Under load: ~1000 mA (1A peak)                │
│    Total for 3 servos: ~3A max                   │
│                                                    │
└────────────────────────────────────────────────────┘

ASSEMBLY FOR 3 SERVOS:
═══════════════════════

Servo 0 (Pitch):
  GND ─→ 6.6V Power Bus GND
  VCC ─→ 6.6V Power Bus +6.6V
  PWM ─→ GPIO35 (LEDC channel 0)

Servo 1 (Yaw):
  GND ─→ 6.6V Power Bus GND
  VCC ─→ 6.6V Power Bus +6.6V
  PWM ─→ GPIO36 (LEDC channel 1)

Servo 2 (Gripper):
  GND ─→ 6.6V Power Bus GND
  VCC ─→ 6.6V Power Bus +6.6V
  PWM ─→ GPIO37 (LEDC channel 2)

POWER-UP SEQUENCE:
═════════════════

1. All servos should initialize to 90° (neutral)
2. Verify mechanically: all servo arms point straight
3. If not: adjust trim on servo potentiometer (inside)
   (Or adjust in code if SixEyes uses offset calibration)

CONNECTING SERVOS UNDER LOAD:
═════════════════════════════

SAFETY CRITICAL:
  1. Always start at 90° (neutral) position
  2. Ramp PWM smoothly to target angle (no jerks)
  3. Monitor current draw (should stay <1A per servo)
  4. Do NOT enable servos while gripper has load
     (high stall current, ~2A)
  5. Use mechanical limit stops, not software only
```

### Servo Power Management

```
VOLTAGE SAFETY:
════════════════

Servo rail target for this PCB revision: 6.6V (from XL4016)
  ├─ Verify servo rail before plugging servos
  ├─ Use neutral startup pose and current-limited bring-up
  └─ Keep bulk capacitance near servo headers (≥1000µF)

CURRENT SPIKE PROTECTION:
═════════════════════════

Each servo can draw 1A briefly when stalled.
3 servos = 3A peak possible.

Recommended power budget:
  ├─ Normal operation: ~100 mA idle + load
  ├─ Smooth motion: ~300 mA continuous
  ├─ Peak spike: ~3 A (brief, <1 sec)
  └─ 6.6V rail path must handle 3A continuous at minimum

If using a temporary external servo source for bench checks:
  ├─ Regulated bench supply set to 6.6V (current-limited)
  ├─ Verify polarity and rail stability before connecting servos
  └─ Keep the same common-ground star-point strategy

RECOMMENDED: onboard XL4016 6.6V rail fed from stable 24V input
```

---

## Communication Wiring

### USB-CDC Connection

```
┌──────────────────────────────────────────────────────┐
│         USB-CDC COMMUNICATION (Native ESP32-S3)     │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Physical Connector:                                │
│    Micro-USB-B or USB-C (depending on board)       │
│    Type: Standard USB 3.0 compatible               │
│                                                      │
│  Connection to Laptop:                             │
│    USB-A ←→ USB-B/C cable (quality matters!)       │
│    Typical distance: 1-3 meters (shielded cable)   │
│                                                      │
│  Electrical Spec:                                  │
│    Power: 5V @ 500mA (USB device)                  │
│    Data: Full-speed USB (12 Mbps)                 │
│    Protocol: CDC/ACM virtual COM port             │
│                                                      │
│  Virtual COM Port (Windows/Linux/Mac):             │
│    Windows: COM3, COM4, etc. (auto-assign)         │
│    Linux: /dev/ttyACM0, /dev/ttyUSB0              │
│    Mac: /dev/tty.usbmodem*, /dev/tty.usbserial*  │
│                                                      │
│  Baud Rate (virtual, doesn't matter):             │
│    115200 baud (standard for compatibility)       │
│    Hardware is full USB speed regardless          │
│                                                      │
└──────────────────────────────────────────────────────┘

HEARTBEAT PROTOCOL:
═════════════════════

Direction: ROS2 (laptop) → Firmware (ESP32)
Format: "HB:0,<sequence>\n"
Example: "HB:0,42\n"
Speed: 50 Hz recommended

Direction: Firmware → ROS2 (laptop)
Format: "SB:<fault>,<motors_en>,<ros2>\n"
Example: "SB:0,1,1\n"
Speed: 10 Hz

TESTING CONNECTION:
═══════════════════

Linux:
  $ picocom /dev/ttyACM0 -b 115200
  (or minicom, screen, etc.)

Windows (PowerShell):
  > [System.IO.Ports.SerialPort]::GetPortNames()
  > py -c "import serial; s=serial.Serial('COM3',115200); print(s.readline())"

macOS:
  $ ls /dev/tty.usb*
  $ screen /dev/tty.usbmodem* 115200
```

### Optional UART for Expansion

```
┌──────────────────────────────────────────────────────┐
│    OPTIONAL: Serial2 (GPIO17/18) for Future Use    │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Currently Used For:                                │
│    TMC2209 UART (Si/So daisy-chain)                │
│    All 4 stepper drivers on same bus               │
│                                                      │
│  Future Expansion:                                 │
│    Not available (fully allocated to TMC2209)      │
│    Would need GPIO17/18 for other purposes         │
│                                                      │
│  Available GPIO for Custom Expansion:              │
│    GPIO1, GPIO2: Strapping pins (use with care)   │
│    GPIO8, GPIO9: Usually SPI Flash (reserved)      │
│    GPIO42, GPIO43: UART monitor console (free)    │
│                                                      │
│  If Serial2 is needed for something else:          │
│    ├─ Move TMC2209 to SPI mode (need SPI pins)    │
│    └─ Requires hardware changes & firmware update  │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## Grounding Strategy

### Star Grounding (Critical for Safety)

```
PROBLEM: Ground Loops
═════════════════════

Multiple disconnected GND connections can cause:
  ├─ Voltage offsets (3.3V logic sees different reference than 24V stepper GND)
  ├─ Noise coupling (high-current stepper ripple affects logic)
  ├─ False fault conditions (noise triggers stall detection)
  └─ Unpredictable electromagnetic behavior

SOLUTION: STAR GROUNDING
═════════════════════════

Single reference point (STAR):
  
        24V PSU GND ──┐
                      ├──→ STAR POINT (central node)
        6.6V rail GND ─┤
                      │
        USB GND ──────┘


From STAR POINT (busbar or PCB pad):

  • Thick wire (12 AWG) to ESP32 GND pin
  • Thick wire (12 AWG) to all TMC2209 GND inputs
  • Thick wire (10 AWG) to servo power distribution GND
  • Thick wire (10 AWG) back to all PSU GND returns

KEY PROPERTIES:
  ├─ All GND connections originate from ONE point
  ├─ No loops (no multiple paths for current)
  ├─ High current (24V stepper) has dedicated thick wire
  ├─ All logic signals reference same GND
  └─ Noise stays isolated to each rail

IMPLEMENTATION:
════════════════

Option 1: Busbar (recommended)
  • Copper busbar with multiple screw terminals
  • All grounds screw into same busbar
  • Bolted to chassis (might add capacitance,  low impedance)

Option 2: PCB pad (if building custom)
  • Dedicated large polygon fill on PCB
  • All grounds connect to edge of polygon
  • Via stitching prevents loop currents

Option 3: Breadboard (temporary testing only)
  • All GND lines to red rail (same row)
  • Then one thick wire from rail to PSU GND
  • NOT suitable for actual robot (too much noise)

WIRE SELECTION:
════════════════

24V stepper GND: 12 AWG (3.3mm²) or thicker
6.6V servo GND:  12 AWG (3.3mm²) or thicker
3.3V logic GND:  16 AWG (1.3mm²) acceptable
UART signal:     22 AWG (0.3mm²) standard

(Smaller gauge = higher resistance = larger voltage drop)
```

### Shielding (Optional but Recommended)

```
HIGH-CURRENT POWER LINES:
═════════════════════════

24V stepper bus can cause EMI:
  ├─ Inductor (stepper coil) switching at high frequency
  ├─ Sharp current transients (PWM)
  └─ Can couple into logic signals nearby

MITIGATION:
  ├─ Route 24V wires away from logic signals
  ├─ Use shielded cable for UART (Si/So) if long runs
  ├─ Keep ESP32 board away from stepper drivers
  ├─ Add ferrite cores on UART wires (optional)
  └─ Proper grounding is most important

FERRITE CORE USAGE:
═════════════════════

If using, choose:
  ├─ Type: FT240-43 or similar (MHz range)
  ├─ Location: Near TMC2209 driver outputs
  ├─ Winding: Wrap UART wires 2-3 times
  └─ Benefit: Attenuates high-frequency noise

But: Proper PSU with bulk caps usually sufficient.
```

---

## Testing Checklist

### Pre-Power Checks

- [ ] No visible solder bridges between pins
- [ ] All JST connectors fully seated and latched
- [ ] Motor A/B coils confirmed correct polarity (continuity test)
- [ ] Servo range verified (500-2500 µs with multimeter)
- [ ] CPU and driver boards don't touch (no shorts)
- [ ] No loose wires dangling near power rails
- [ ] All capacitors oriented correctly (stripe = negative)
- [ ] Fuses rating correct (10A for 24V input, 5A for 6.6V rail)

### Power-On Sequence

1. **USB Only (no stepper/servo power)**:
   - [ ] ESP32 board powers up (LED blinks if present)
   - [ ] Check Device Manager / `/dev/ttyACM0`
   - [ ] Serial monitor shows startup messages
   - [ ] No shorts detected (board doesn't brown out)

2. **Add 24V Stepper Power**:
   - [ ] LED doesn't change (isolated supplies)
   - [ ] No audible buzz from stepper drivers
   - [ ] Measure: 24V ± 0.5V at TMC2209 VDD pins
   - [ ] Measure: 0V at all GND (no offset)

3. **Verify 6.6V Servo Rail (XL4016)**:
   - [ ] Servos should initialize to 90° (neutral)
   - [ ] No servo jerks or movement
  - [ ] Measure: 6.6V ± 0.3V at servo VCC pins
   - [ ] Current draw: ~100 mA idle (3 servos)

### Communication Verification

- [ ] Send manual heartbeat: `echo "HB:0,0" > /dev/ttyACM0`
- [ ] Monitor serial output: `picocom /dev/ttyACM0 -b 115200`
- [ ] Observe "EN pin LOW" message (waiting for heartbeat)
- [ ] Send another heartbeat: observe "EN pin HIGH"
- [ ] Wait 500 ms without sending: observe "HEARTBEAT_TIMEOUT"
- [ ] Send heartbeats continuously: see `SB:0,1,1` (status packets)

### Motor Verification (No Load)

- [ ] Stepper motors: slight resistance to manual rotation (detent torque)
- [ ] No audible clicking or grinding
- [ ] Servo arms move smoothly from 0° to 180°
- [ ] Servo current stays under 1A per servo

### Final Integration

- [ ] Firmware compiles without warnings
- [ ] Memory usage < 50% (verify via `pio run` output)
- [ ] All modules initialized in startup log
- [ ] Control loop runs at stable 400 Hz
- [ ] No spurious fault conditions from noise

---

## Troubleshooting

### Symptom: ESP32 Board Won't Power Up

**Checks**:
1. USB cable: Try different cable (some cheap cables power only)
2. USB port: Try different PC port
3. Micro-USB connector: May be loose, jiggle gently
4. Solder joints: Check for whiskers or bridges near USB pins
5. LDO regulator: Feel if it's getting hot (indicates short)

**Fix**:
- Replace USB cable first (most common)
- Clean USB connector with alcohol & lint-free cloth
- If still fails: board may be DOA (return/replace)

---

### Symptom: No Serial Port Visible in Device Manager (Windows)

**Checks**:
1. Install CH340 driver (common for cheap ESP32 boards)
2. Check Device Manager for "Unknown Device" or "Other devices"
3. Verify USB cable is data-capable (not power-only)

**Fix**:
```powershell
# Download driver from: https://github.com/WCHSoftware/ch341ser_windows
# Install and reboot
# Then check:
[System.IO.Ports.SerialPort]::GetPortNames()
```

---

### Symptom: Stepper Motors Don't Step

**Sequence of Checks**:
1. **Verify UART connection**:
   - Scope GPIO17/18 at UART pins
   - Should see TTL data at 115200 baud
   - If silent: check wire connections

2. **Check VREF potentiometer**:
   - Measure with multimeter between pot GND and wiper
   - Should be ~0.75V (for 1.5A setting)
   - Adjust if needed (tiny potentiometer, use fine tool)

3. **Verify motor coil polarity**:
   - Do OFF test: disconnect driver, connect 3V directly
   - Motor should rotate smoothly (no cogging)
   - If jittery: swap A± or B± wires

4. **Check PDN signals**:
   - GPIO7/11/15/21 should be HIGH when idle (3.3V)
   - Should pulse LOW during UART communication
   - If stuck LOW: GPIO might be shorted to GND

5. **Confirm firmware mode**:
   - Serial monitor should show control loop running
   - Should show "EN pin" status messages
   - If stuck initializing: check FreeRTOS task creation

**Last resort**: Use TMCStepper library diagnostic mode to read DRV_STATUS register

---

### Symptom: Servos Twitch or Move Unexpectedly

**Root Causes**:
1. **Power supply sagging**:
  - Measure 6.6V rail under load
  - Should stay approximately 6.3V-6.9V
  - If it drops significantly below target: check 24V source, XL4016 settings, and wiring

2. **Noise coupling from stepper PWM**:
   - PWM switching noise affects servo signal
   - Solution: **Shielded cable** for servo PWM lines
   - Or: **Ferrite cores** on servo connectors

3. **Unstable ESP32 supply**:
   - If 3.3V rail has noise, all GPIO become noisy
   - Check: 100nF bypass cap directly at ESP32 VCC
   - Add larger 10µF cap if still noisy

4. **Software issue** (PID tuning, interrupt conflicts):
   - Unlikely if other motors work fine
   - Check if only one servo affected (pinpoint issue)

**Fix**:
1. Check voltage sag with multimeter under load  
2. Add shielded cable to servo PWM lines
3. Verify 100nF bypass cap is present & tight

---

### Symptom: Intermittent Communication Losses

**Symptoms**: Heartbeat timeout even when sending heartbeats consistently

**Root Causes**:
1. **USB cable quality**:
   - Bad USB cable = dropped packets
   - Try different cable (new, thick, well-insulated)

2. **EMI from stepper PWM**:
   - High current switching couples into UART/USB
   - Solution: Better shielding or separate power grounds

3. **Timing glitch** (rare):
   - Control loop jitter might cause misalignment
   - Measure actual loop period with scope on GPIO
   - Should be exactly 2.5 ms ± 10 µs

**Fix Priority**:
1. Replace USB cable (cheapest, most likely)
2. Verify USB shielded, proper routing away from 24V
3. Add ferrite cores to UART / USB lines
4. Check ESP32 power supply quality (noise on scope)

---

### Symptom: Motor Enable Pin (GPIO6) Stuck LOW

**Checks**:
1. Measure GPIO6 voltage:
   - Should be HIGH (3.3V) at startup
   - If stuck LOW: GPIO shorted to GND

2. Check PCB/breadboard for bridges:
   - Visual inspection near GPIO6
   - Use multimeter continuity test from GPIO6 to GND
   - Check underside of wires (might be soldered by accident)

3. Verify firmware initialization:
   - Serial monitor should show "EN pin HIGH" message
   - If message missing: SafetyTask didn't init properly

**Fix**:
- Remove any solder bridges / shorts
- Reflow GPIO6 connection if using soldered header
- If still stuck: GPIO pin might be damaged (replace board)

---

### Symptom: 24V PSU Buzzing Under Load

**Cause**: Insufficient capacitance or weak PSU

**Fixes**:
1. Add larger bulk capacitor (2200µF/35V instead of 1000µF)
2. Verify PSU can supply 6A continuous
3. Check wire gauge (12 AWG minimum for 24V bus)
4. Ensure good solder joints on capacitor leads

**Result**: Should be quiet or very soft hum (normal)

---

## Quick Reference Card

### Power Wiring Jumper

```
24V PSU (+):       12 AWG (THICK) ──→ Main input bus (TMC2209 VM + onboard bucks)
6.6V rail (+):     16 AWG (MEDIUM) ──→ Servo VCC & capacitor  
3.3V rail:         PCB-distributed ──→ ESP32/TMC2209 logic domain

STAR GROUND:       12 AWG (THICK) ────┬─→ ESP32 GND
(all come here)                        ├─→ All TMC2209 GND
                                       ├─→ Servo power GND
                                       └─→ All PSU GND returns
```

### GPIO Quick Map

```
GPIO6   = Motor EN (active HIGH)
GPIO7   = Motor 0 PDN (active LOW)
GPIO11  = Motor 1 PDN (active LOW)
GPIO15  = Motor 2 PDN (active LOW)
GPIO21  = Motor 3 PDN (active LOW)
GPIO17  = UART RX (TMC2209)
GPIO18  = UART TX (TMC2209)
GPIO35  = Servo 0 PWM
GPIO36  = Servo 1 PWM
GPIO37  = Servo 2 PWM
```

### Power Consumption Budget

```
Idle (no motors):             low current on 24V input (logic + idle rails)
Smooth motion (1 stepper):    ~1.5 A @ 24V + servo rail dynamic current
Peak (all steppers + servos): ~6 A @ 24V + ~3 A on 6.6V servo rail path
```

---

**Wiring complete! Proceed to Flashing Docs for deployment. ✅**
