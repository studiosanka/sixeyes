# firmware/v1/joint_node

Scaffold only — see `docs/V1_TODO.md` at the repo root for what is and isn't implemented yet.

- Hardware spec: `docs/hardware/v1/v1_PCB_Design_Reference.md`
- Protocol spec: `docs/protocols/CAN_MESSAGE_PROTOCOL.md`
- Build: `pio run -e v1_base` / `v1_shoulder_l` / `v1_shoulder_r` / `v1_elbow`

## What's real vs. stub in this scaffold

Real (interfaces and control-flow match the protocol doc): node role selection, CAN ID allocation and message structs, the dual-timeout safety model (`can_safety_task`), ISR-decoupled E-stop handling (`estop_handler`), pin map from the v1 hardware doc.

Stubbed (compiles, does not work yet — see inline `TODO` comments):
- `encoder_driver.cpp` — no register-level implementation; final part (MT6835 vs AS5048A) not yet chosen
- `motor_controller.cpp` — no step generation, no homing sequence, divergence check uses placeholder unit conversion
- `usb_bridge.cpp` — no JSON↔CAN translation; ROS2-side `usb_bridge_node` also needs its own rework
- `main.cpp`'s `loop()` — placeholder pacing via `delay()`, not the FreeRTOS-scheduled deterministic timing legacy firmware used (`MotorControlScheduler` equivalent not yet ported)
- CAN RX dispatch runs from a polled `loop()` call, not a true hardware ISR — the E-stop latency budget in the protocol doc assumes ISR-level dispatch, not yet wired that way

No unit tests yet (legacy's `test/` directory with `MockSerial`/`MockGPIO`/`MockTimer` pattern is a reasonable model to reuse once real logic lands).
