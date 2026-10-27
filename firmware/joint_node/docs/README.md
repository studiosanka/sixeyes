# firmware/joint_node

One PlatformIO project, four node roles (`v1_base` / `v1_shoulder_l` / `v1_shoulder_r` / `v1_elbow`), target module ESP32-C6 Super Mini. The list below shows what is and isn't implemented yet.

- Hardware spec: `docs/reference/v1.2_PCB_Design_Reference.md` (§3 pin map is mirrored in `src/modules/config/board_config.h`)
- Protocol specs: `docs/reference/CAN_MESSAGE_PROTOCOL.md` (joint bus) and the SixEyes **host protocol v1** (USB link to a laptop or phone), defined in the `sixeyes-host` repo (`protocol/PROTOCOL.md`, `protocol/messages.yaml`)
- Build: `pio run -e v1_base` / `v1_shoulder_l` / `v1_shoulder_r` / `v1_elbow` (all four build; none has run on hardware yet)
- Flash/debug over each module's own USB-C (see [Flash the firmware](../../../docs/05-flash-the-firmware.md)); `Serial` is mapped to native USB CDC (`ARDUINO_USB_CDC_ON_BOOT=1`) so it does not claim UART0 (GPIO16/17)

## What's real vs. stub

Real (builds; interfaces and control flow match the docs, untested on hardware):
- node role selection, CAN ID allocation and message structs, the dual-timeout safety model (`can_safety_task`), ISR-decoupled E-stop handling (`estop_handler`)
- v1.2 pin map in `board_config.h`
- TMC2209 driver adapted for v1.2: single-wire PDN_UART on separate TX/RX GPIOs (TX on GPIO5 via 1 kΩ on the PCB, RX on GPIO17), real active-low EN pin held disabled at boot, and the E-stop path (`CanSafetyTask::emergencyDisable()`) now drops EN via `MotorController::disable()`
- **UART mode is the recommended mode.** At boot the driver probes UART (MS1 and MS2 held low, address 0). If it answers, the firmware forces digital current (the module's pot is ignored), starts at a safe **500 mA RMS** (`TMC2209_DEFAULT_RUN_MA`) and sets the microsteps in software. If it does not answer, it prints a warning and falls back to STEP/DIR only: the module's pot sets the current, MS1 (GPIO15) and MS2 (GPIO16) select 1/8 or 1/16 microsteps, and the driver is refused if the module's VREF (read on GPIO2 through a 10 kΩ) is above `TMC2209_VREF_MAX_V`
- AS5600 I²C encoder driver (`encoder_driver.cpp`): status + raw angle read, magnet-detect fault, multi-turn unwrap
- **Host link on the Base board** (`comms/host_link`, `comms/usb_bridge`, `arm/arm_state`), work package 1 of the host handoff: COBS + CRC-16 framing, `hello`/`hello_ack`, an `ack` for every command, the control-source and watchdog rules, and the 50 Hz `state` stream over the USB serial port. It reports the six host joints from the encoders and fans targets out to the nodes over CAN. **Capabilities: homing, BLE, echo, stop button and motion slots** (all five capability bits are now set). Builds; untested on hardware. The framing is checked by the golden vectors in `host_codec_selftest.cpp` (build `v1_base_selftest` and read the serial monitor)
- **Bluetooth link to a phone** (`comms/ble`, work package 3; Base builds only, `-DSIXEYES_BLE`, NimBLE-Arduino pinned in `platformio.ini`): the same frames as USB in one GATT value each, so the host protocol rules (control-source arbitration, the 200 ms watchdog, ack for every command) apply unchanged. A phone dropping makes the arm hold where it is, and a homing run in progress aborts. See "Bluetooth link" below. Builds; untested on a phone
- **Motion slots** (`storage/motion_slots`, `host_link`; work package 4): four motions saved on the arm in the flash filesystem (LittleFS in the 1.4 MB `spiffs` partition), written sample by sample with `slot_write` and played with `slot_play`. The capability is only switched on if the filesystem mounted. See "Motion slots" below
- **Echo** (`echo_targets`): the state stream goes to every host that said hello and carries the last accepted target, so a laptop listening on USB sees what a phone on BLE is commanding (Mode 2). This needed no extra code
- **Stop button** (`safety/stop_button`, `hw_estop` capability): the BOOT button on **every** board stops the whole arm (the pressed board stops itself and sends a bus-wide E-STOP, reason "manual button"); Base reports it to the host as fault `hw_estop`, and `clear_fault` is refused while Base's own button is held. It is a software stop, not a safety-rated emergency stop: the real one is a switch or plug in the 24 V line. An external push button to GND on JMC2 pin 9 (the GPIO9 line, unconnected on the PCB) does the same job when the boards are out of reach
- **Node telemetry** (`telemetry/node_telemetry.cpp`): the encoder is read once per loop on every node, and the non-Base nodes send `ENCODER_TELEMETRY` at 50 Hz and `NODE_STATUS` at 10 Hz over CAN. Because every node now reports `NODE_STATUS`, Base's liveness timeout is live: a node that goes silent for 500 ms triggers the bus-wide E-STOP and the host sees fault `bus_lost`
- **Encoder on the motor shaft** (decided 2026-10-27): counts are converted to output-shaft angle with the 1:25 ratio in `config/joint_config.h`. Counts are relative to power-on, so every boot starts unhomed

- **Motion** (`motor_control/step_generator`, `motor_controller`; work package 0 of the host handoff): STEP pulses from a hardware timer, and a trapezoid move to a target in steps, with per-joint speed and acceleration caps (placeholders in `config/joint_config.h`). A new target mid-move re-plans from the present speed, so streamed targets are followed smoothly (the profile lags a steady stream by about v²/2a, roughly 2 degrees at 20 deg/s with the placeholder caps; a velocity feed-forward would remove it). The AS5600 does two jobs: if it stops following the steps during a move the node latches a stall and stops, and **at rest it trims**: once the shaft has settled for 200 ms and the encoder says it is more than about 2 degrees (at the motor) short, the node redoes the missing steps (at most three tries). Targets are ignored until the node is homed. The loop is paced at a fixed 250 Hz; the STEP pulses do not depend on it
- **Homing** (work package 2 of the host handoff): each stepper drives slowly into its hard stop at low current, and either StallGuard (polled over UART: DIAG is not wired on v1.2) or an encoder check (the AS5600 stops following the steps) ends the search; it then backs off and takes the stop as its reference (step count and an encoder offset both set to the stop's angle). Base runs the sequence over all joints in the order gripper, wrist roll, wrist pitch, elbow, shoulder, base, and the host sees Unhomed, Homing, Ready (or Fault with `homing_failed`). Servos are homed open loop: a slow ramp to a reference angle. The Elbow turns its servos off after 100 ms of silence, so Base re-sends them every 50 ms. Any motor disable (E-STOP, bus fault, failed homing) forgets the reference. **Written and compiled only: the stop angles, `SGTHRS` and homing current are placeholders and have never run on a real arm. See "Before the first homing run" below**
- A new `MOTOR_FLAG_ABORT` bit in the CAN `MOTOR_TARGET` flags stops a node and aborts its homing; `NODE_STATUS` also reports homed, homing and homing-failed (see the CAN protocol doc §4.3 and §4.4)

Stubbed or not written (compiles, does not work yet — see inline `TODO` comments):
- BLE (work package 3) and the BLE echo and motion slots (work package 4) are not written; see `FIRMWARE_HANDOFF.md` in the host repos folder
- The motion-slot code has only been compiled: never run it on the arm without checking the first approach on a free joint, and test saving and replaying a short motion before relying on it
- The BLE and stop-button code has never run on a phone or a board. Check these on hardware first: pairing inside and outside the window, reconnecting a bonded phone, the MTU, and that a BLE drop stops the arm within 200 ms
- There is no 24 V-side emergency stop in the firmware's reach: that is a switch in the supply line
- The two shoulder motors home independently at the same speed: a stall found by one does not instantly stop the other (that needs a fast "stop found" CAN message). They share a gearbox, so both stall within a status period of each other
- A servo that loses its commands (Elbow watchdog, 100 ms) is driven to 90 degrees, not released. That is existing `ServoManager` behaviour and is not yet safe for a gripper holding something
- `main.cpp`'s `loop()` — placeholder pacing via `delay()`, not the FreeRTOS-scheduled deterministic timing legacy firmware used (`MotorControlScheduler` equivalent not yet ported)
- CAN RX dispatch runs from a polled `loop()` call, not a true hardware ISR — the E-stop latency budget in the protocol doc assumes ISR-level dispatch, not yet wired that way
- servo disable on E-stop (Elbow) is not wired yet

## Bluetooth link

- **GATT:** service `53495845-5945-5300-0001-000000000000`; `rx` (phone to arm; write or write without response) `...0002...`; `tx` (arm to phone; notify) `...0003...` (the dots stand for the rest of the service UUID, with only the group `0001` changed). The arm advertises as `SixEyes-XXXX` (XXXX from its MAC address). One frame per GATT value: the trailing `0x00` may be left off on writes, and notifications always carry it.
- **MTU:** the arm asks for 247. A `state` frame is about 62 bytes, so a phone that stays at the default 23-byte MTU gets each frame split over several notifications, and its code must then treat the link as a byte stream ending at `0x00`. Android Chrome normally negotiates a larger MTU; this needs checking on a real phone.
- **Who can connect:** one phone at a time, over an encrypted, bonded link. A phone that has paired before reconnects at any time. A **new** phone can pair only while the pairing window is open: for 60 s after power-up, and for 60 s after the BOOT button on Base is held for 3 s. Outside the window a new phone is disconnected at once. The arm remembers the last three phones (pairing a fourth replaces the oldest).
- **Default control source** is BLE once BLE is built in, as the protocol says; a laptop on USB must send `set_control_source usb` before it can home or move (the bring-up tool does).
- **Not done:** the pairing window is not shown to the phone (a refused connection just drops); the phone page needs to explain "switch the arm off and on to pair a new phone".

## Motion slots

The host protocol leaves the details of the draft slot messages open; this is what the firmware does.
- **Writing:** `slot_write` carries one sample: its index, the time since the previous sample in ms, and the six joint values. Index 0 starts the slot afresh; every later index must be the next free one (otherwise `bad_args`), and `dt_ms` must be above 0 after the first sample. At most 6000 samples per slot (two minutes at 50 Hz; four full slots use about 620 KB of the flash filesystem). A full or failed flash is answered `faulted`. The number of samples is worked out from the file size, so a power cut during saving leaves a shorter, valid motion.
- **Playing:** `slot_play` needs the arm Ready (otherwise `not_homed`), the source in control, and a motion of at least two samples (otherwise `bad_args`). The arm first moves **slowly** from where it is to the first pose (at least a second, at the per-joint `SLOT_APPROACH_SPEED`), then plays the samples at the recorded timing, with straight-line blending between them, and stops on the last pose. It plays once, at recorded speed: the protocol has no speed or loop field. Every pose is clamped to the joint limits, and the node motion profile still caps speed.
- **While it plays**, `targets` and `set_control_source` are refused `busy` and the 200 ms watchdog is off (the arm is driving itself). `stop`, a fault, a lost phone or `clear_fault` ends it and holds the arm where it is. The state stream's `target` follows the motion, so a listener sees it.
- **Erasing:** `slot_erase` removes a slot's file. Writes and erases are refused `busy` during a replay.
- **Not possible yet:**
  - Replaying from a physical button with no phone: the only button on the board is the stop button.
  - Reading a slot back, listing which slots are filled, or showing a playing flag in `state`. They need new protocol messages, which belong in `messages.yaml` in the host repo.
  - A loop or speed option for replay.

## Bench test (work package 0 "done when") and homing tuning

Flash the `v1_bench` environment to ONE board with ONE motor, no arm and no bus:

```
pio run -e v1_bench -t upload
pio device monitor
```

Type `help` for the commands. A first session:
1. `en`, then `zero` (call the present pose 0 degrees), then `goto 10`, `goto -10`, `rel 5`. Watch `status` or `trace on`: it shows the step count and the encoder side by side, in output-shaft degrees. The two should agree to within about a degree; if the encoder runs the other way, flip `ENCODER_SIGN_VS_STEPS`.
2. Check smoothness by ear and eye at several targets, then `stop` mid-move and re-target mid-move.
3. `home` runs the real homing code against a stop you provide (a hand on the output works for a first test) and prints `SG_RESULT` every 20 ms, so you can set `TMC2209_SGTHRS_HOMING` just above the stalled value. `cur <mA>` changes the current on the fly.
4. **The handoff's goal, "follows a target to ±0.5 degrees at the output", cannot be proved by this build:** the encoder sits on the motor shaft, so it cannot see backlash in the 1:25 gearbox. Measure the output with a protractor or a dial indicator on the real gearbox.

This build enables the motor without a bus heartbeat. It is for the bench only; never flash it to a board on the arm.

## Before the first homing run

Homing drives a joint into a mechanical stop. Do these first, in this order:
1. Set the real stop direction and angle for each joint in `config/joint_config.h` (`HOMING_DIRECTION_OUTPUT`, `HOMING_STOP_ANGLE_RAD`). Today they come from the placeholder joint limits.
2. Confirm UART answers on the supported driver module **at 24 V** (it has only been seen working on one unidentified module at 9 V and 12 V). Homing falls back to the encoder check without UART, but that is a weaker detector.
3. Run the motor alone first, off the arm: check which way it turns, that the AS5600 counts the same way as the steps (homing stops with an explicit message if it does not: flip `ENCODER_SIGN_VS_STEPS`), and that the encoder offset reads sensibly.
4. Tune `TMC2209_SGTHRS_HOMING` and `TMC2209_HOMING_RUN_MA` (`drivers/tmc2209/tmc2209_config.h`) and `HOMING_SPEED_OUTPUT_DEG_S`, with the gearbox and a soft stop, until the stall is clean and gentle. The hard stops must take repeated hits.
5. Calibrate each servo's reference angle (`servoHomeDeg`): never drive an MG996R past the joint's mechanical limit.

## Assumptions to confirm on the real arm

All of these are placeholders in `src/modules/config/joint_config.h`:
- the gear ratio (25.0, nominal) and the Shoulder R direction sign (mirrored, so negative)
- which servo header drives which joint (header 0 = wrist pitch, 1 = wrist roll, 2 = gripper)
- the servo angle for each joint's zero and the gripper's open and closed angles (calibrate before any servo moves a joint)
- the joint limits (the same placeholders the host simulator uses)
- how `Serial` behaves on the Base board when no host is connected, and when a host opens the port (the code never waits for the host, but this has not been run)

## Serial port on the Base board

The Base board's USB serial port carries the binary host protocol. Text log lines are printed at boot, but stop after the first valid host frame (`Logging::setQuiet`), so they never damage a frame. To read logs without a host program, open a serial monitor and do not start one. Build with `-DHOST_LINK_TEXT_LOGS` to keep logging on regardless (a host will then see an occasional bad frame).

No unit tests yet (legacy's `test/` directory with `MockSerial`/`MockGPIO`/`MockTimer` pattern is a reasonable model to reuse once real logic lands).
