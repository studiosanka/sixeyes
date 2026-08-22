# v1 Rework — TODO

Tracks the v1 Universal Joint PCB rework: eliminating the leader arm (future IMU + inverse kinematics input), moving from one monolithic follower MCU to 4 distributed CAN nodes. See `docs/hardware/v1/v1_PCB_Design_Reference.md` (hardware spec) and `docs/protocols/CAN_MESSAGE_PROTOCOL.md` (protocol design, draft).

## Done

- [x] Legacy move: `firmware/{alpha,beta}` → `firmware/legacy/{alpha,beta}`, CI path fixed
- [x] Legacy move: Alpha/Beta Follower + Leader PCB KiCad projects → `hardware_assets/pcb_project_files/legacy/`
- [x] Legacy move: `docs/hardware/{alpha,beta}` → `docs/hardware/legacy/{alpha,beta}`
- [x] Legacy move + LEGACY banners: `TELEOPERATION_MODE_ARCHITECTURE.md`, `OPEN_LOOP_STEPPER_STRATEGIES.md`, `JSON_MESSAGE_PROTOCOL.md`
- [x] Deleted 3 stale duplicate `SixEyes Technical Reference*.txt` files; kept June 2026 `.md`, marked LEGACY
- [x] Repo-wide link repair (README.md, docs/README.md, docs/PROJECT_SCOPE_AND_REPO_MAP.md, technical reference doc) — verified zero remaining broken references
- [x] Drafted `docs/protocols/CAN_MESSAGE_PROTOCOL.md` — node addressing, CAN ID allocation, message formats, dual-timeout safety model, E-stop latency budget
- [x] Resolved all 4 blocking design decisions (2026-08-22) — see CAN protocol doc §6 for full rationale:
  - Stall detection: StallGuard for homing only; runtime stall/fault = encoder-vs-commanded-position divergence
  - Control loop frequency: **250 Hz** (down from legacy 400/500 Hz — CAN bus bandwidth + single-core ESP32-C6 headroom); E-stop handling decoupled from control loop tick (ISR-driven)
  - Checksum: no app-layer checksum, rely on CAN's CRC-15 + existing `seq` fields — **note: revisit if a formal functional-safety certification (IEC 61508/ISO 13849) is ever pursued for this bus, which would require diverse redundancy, not just CAN's CRC**
  - Bus-off recovery: manual/power-cycle only, no auto-recovery (Base's node-liveness timeout fires a bus-wide E-STOP regardless)
- [x] Rewrote root `README.md` and `docs/README.md` around v1 as primary generation, Alpha/Beta collapsed into "Legacy Hardware" sections
- [x] Fixed `.gitignore` UTF-16 encoding corruption (bare `*` pattern was silently matching everything)
- [x] Found and fixed a stale doc hiding outside `legacy/`: `docs/ros2/ROS2_INTEGRATION.md` claimed "hardware-agnostic" but documents the legacy ASCII/JSON protocol only — moved to `docs/ros2/legacy/ROS2_INTEGRATION.md` with a LEGACY banner, all links repaired
- [x] Scaffolded `firmware/v1/joint_node/` — one PlatformIO project, 4 envs (`v1_base`/`v1_shoulder_l`/`v1_shoulder_r`/`v1_elbow`) selected via `-DJOINT_NODE_ID=N`. **All 4 environments build clean** (verified with `pio run -e <env>` for each, ESP32-C6). See `firmware/v1/joint_node/docs/README.md` for exactly what's real vs. stubbed.
  - Reused from legacy Beta: TMC2209 driver (unchanged, already generic over driver count), servo_manager (open-loop PWM math is generation-agnostic), logging util
  - New: CAN/TWAI driver + protocol structs matching CAN_MESSAGE_PROTOCOL.md exactly, dual-timeout safety task, ISR-decoupled E-stop handler, encoder driver interface (stub), motor controller with encoder-divergence stall check (stub), Base-only USB bridge (stub)
  - Two real platform bugs caught by build-testing (not scaffold logic, ESP32-S3→C6 + arduino-esp32 core version differences): TMC2209 UART defaulted to `Serial2` which doesn't exist on C6 (fixed to `Serial1`); servo PWM used the old channel-based LEDC API (`ledcSetup`/`ledcAttachPin`) which this core version replaced with a pin-keyed API (`ledcAttach`/`ledcWrite`)
  - Dropped (all legacy-only now): leader-comms code, closed-loop servo feedback, USB-CDC JSON follower protocol
  - **Stubbed, not real yet**: encoder register-level driver (blocked on MT6835 vs AS5048A part selection), step generation, homing sequence, USB↔CAN JSON translation, deterministic FreeRTOS-scheduled control loop (currently a placeholder `delay()` pace in `loop()`), true ISR-driven CAN RX dispatch (currently polled from `loop()`, not from TWAI's actual interrupt path)
- [ ] New KiCad project: `hardware_assets/pcb_project_files/SixEyes Joint PCB v1/` — one board design, 4 populated variants (Base/Shoulder L/Shoulder R/Elbow) per BOM DNP table in v1 hardware doc

## Next up

- [ ] Wire the CAN RX dispatch onto TWAI's actual interrupt path (or a high-priority pinned FreeRTOS task) instead of the current polled `loop()` call — needed to actually hit the ISR-driven E-stop latency budget in CAN_MESSAGE_PROTOCOL.md §5, not just approximate it
- [ ] Port a `MotorControlScheduler`-equivalent (deterministic FreeRTOS task at `CONTROL_LOOP_HZ`) — `loop()` currently paces itself with `delay()`, not a real timing guarantee
- [ ] Implement `encoder_driver.cpp` once MT6835 vs AS5048A is chosen (blocks real stall detection and `ENCODER_TELEMETRY`)
- [ ] Implement `motor_controller.cpp` step generation and homing sequence
- [ ] Implement `usb_bridge.cpp` JSON↔CAN translation (pairs with the `usb_bridge_node` ROS2 rework below)
- [ ] Bench-tune `MotorController::STALL_DIVERGENCE_THRESHOLD_DEG` once encoder units/gear ratio are known
- [ ] Resolve the TMC2209 EN pin placeholder in `tmc2209_config.h` (v1 hardware doc §3 pin map doesn't define one yet) once the KiCad schematic exists
- [ ] Verify GPIO4/5 (used for CAN TX/RX) against the ESP32-C6 TRM boot-config table before PCB layout — flagged as open since the original hardware doc, not yet re-verified
- [ ] Add a `test/` directory once real logic lands, following legacy's `MockSerial`/`MockGPIO`/`MockTimer` pattern
- [ ] `usb_bridge_node` (ROS2): rework to a Base-only USB link; Base translates ROS2 JSON/ASCII commands to CAN fan-out (see CAN protocol doc §1)
- [ ] `joint_state_node` / `safety_node`: update message assumptions once CAN protocol is implemented and Base-side translation exists
- [ ] `docs/PROJECT_SCOPE_AND_REPO_MAP.md`: add NodeMesh compatibility note — "targets legacy Alpha/Beta hardware; not yet compatible with v1's distributed CAN architecture"
- [ ] Bench-validate the 250 Hz control loop choice and ISR-driven E-stop latency figures against real ESP32-C6 hardware once firmware exists; revisit rate upward if headroom allows
- [ ] Consider merging `docs/README.md` + `docs/PROJECT_SCOPE_AND_REPO_MAP.md` into one index (deferred from initial cleanup pass)

## Deferred (not in scope until IMU+IK exists)

- [ ] NodeMesh Node1 rework: swap potentiometer/leader-arm sampling for IMU stream feeding the same `ExperiencePacket.joints[6]` slot
- [ ] NodeMesh Node0 rework: from direct-GPIO single-board control to CAN-relay role talking to v1 Base node
- [ ] IMU + inverse-kinematics design doc (no design started yet — replaces the leader arm as control input)

## Repos unaffected by v1

- CycloCAD — pure gear geometry tooling, no dependency on electrical/firmware architecture. No action needed.
