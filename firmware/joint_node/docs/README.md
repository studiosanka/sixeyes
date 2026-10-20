# firmware/joint_node

One PlatformIO project, four node roles (`v1_base` / `v1_shoulder_l` / `v1_shoulder_r` / `v1_elbow`), target module ESP32-C6 Super Mini. The list below shows what is and isn't implemented yet.

- Hardware spec: `docs/reference/v1.2_PCB_Design_Reference.md` (§3 pin map is mirrored in `src/modules/config/board_config.h`)
- Protocol spec: `docs/reference/CAN_MESSAGE_PROTOCOL.md`
- Build: `pio run -e v1_base` / `v1_shoulder_l` / `v1_shoulder_r` / `v1_elbow` (all four build; none has run on hardware yet)
- Flash/debug over each module's own USB-C (see [Flash the firmware](../../../docs/05-flash-the-firmware.md)); `Serial` is mapped to native USB CDC (`ARDUINO_USB_CDC_ON_BOOT=1`) so it does not claim UART0 (GPIO16/17)

## What's real vs. stub

Real (builds; interfaces and control flow match the docs, untested on hardware):
- node role selection, CAN ID allocation and message structs, the dual-timeout safety model (`can_safety_task`), ISR-decoupled E-stop handling (`estop_handler`)
- v1.2 pin map in `board_config.h`
- TMC2209 driver adapted for v1.2: single-wire PDN_UART on separate TX/RX GPIOs (TX on GPIO5 via 1 kΩ on the PCB, RX on GPIO17), real active-low EN pin held disabled at boot, and the E-stop path (`CanSafetyTask::emergencyDisable()`) now drops EN via `MotorController::disable()`
- **UART mode is the recommended mode.** At boot the driver probes UART (MS1 and MS2 held low, address 0). If it answers, the firmware forces digital current (the module's pot is ignored), starts at a safe **500 mA RMS** (`TMC2209_DEFAULT_RUN_MA`) and sets the microsteps in software. If it does not answer, it prints a warning and falls back to STEP/DIR only: the module's pot sets the current, MS1 (GPIO15) and MS2 (GPIO16) select 1/8 or 1/16 microsteps, and the driver is refused if the module's VREF (read on GPIO2 through a 10 kΩ) is above `TMC2209_VREF_MAX_V`
- AS5600 I²C encoder driver (`encoder_driver.cpp`): status + raw angle read, magnet-detect fault, multi-turn unwrap

Stubbed (compiles, does not work yet — see inline `TODO` comments):
- `motor_controller.cpp` — no step generation, no homing sequence; the divergence check converts counts to degrees of the magnet's own shaft only (magnet position — motor shaft vs 1:25 output — is undecided)
- `usb_bridge.cpp` — no JSON↔CAN translation; ROS2-side `usb_bridge_node` also needs its own rework
- `main.cpp`'s `loop()` — placeholder pacing via `delay()`, not the FreeRTOS-scheduled deterministic timing legacy firmware used (`MotorControlScheduler` equivalent not yet ported)
- CAN RX dispatch runs from a polled `loop()` call, not a true hardware ISR — the E-stop latency budget in the protocol doc assumes ISR-level dispatch, not yet wired that way
- servo disable on E-stop (Elbow) is not wired yet

No unit tests yet (legacy's `test/` directory with `MockSerial`/`MockGPIO`/`MockTimer` pattern is a reasonable model to reuse once real logic lands).
