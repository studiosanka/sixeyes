# v1 Rework — TODO

Tracks the v1 Universal Joint PCB rework: eliminating the leader arm (future IMU + inverse kinematics input), moving from one monolithic follower MCU to 4 distributed CAN nodes. See `docs/hardware/v1/v1.2_PCB_Design_Reference.md` (hardware spec, with `v1.2_BOM_LCSC.csv`; the v1.1 reference is kept unchanged for history) and `docs/protocols/CAN_MESSAGE_PROTOCOL.md` (protocol design, draft).

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
- [x] Repo cleanup: deleted `tools/ros2_heartbeat_node.py` (a standalone script duplicating `usb_bridge_node`'s real heartbeat implementation, misleadingly named/documented as a ROS2 node despite never importing `rclpy`) and the empty top-level `simulation/` folder (the actual Gazebo sim lives in `ros2_ws/src/sixeyes_description/` + `sixeyes_bringup/launch/sim.launch.py`, the ROS2-idiomatic location — a separate top-level folder would just be a second, competing answer to "where does sim stuff go"). Fixed the dangling references in `docs/ros2/legacy/ROS2_INTEGRATION.md`, root `README.md`, `docs/PROJECT_SCOPE_AND_REPO_MAP.md`.
- [x] Scaffolded `firmware/v1/joint_node/` — one PlatformIO project, 4 envs (`v1_base`/`v1_shoulder_l`/`v1_shoulder_r`/`v1_elbow`) selected via `-DJOINT_NODE_ID=N`. **All 4 environments build clean** (verified with `pio run -e <env>` for each, ESP32-C6). See `firmware/v1/joint_node/docs/README.md` for exactly what's real vs. stubbed.
  - Reused from legacy Beta: TMC2209 driver (unchanged, already generic over driver count), servo_manager (open-loop PWM math is generation-agnostic), logging util
  - New: CAN/TWAI driver + protocol structs matching CAN_MESSAGE_PROTOCOL.md exactly, dual-timeout safety task, ISR-decoupled E-stop handler, encoder driver interface (stub), motor controller with encoder-divergence stall check (stub), Base-only USB bridge (stub)
  - Two real platform bugs caught by build-testing (not scaffold logic, ESP32-S3→C6 + arduino-esp32 core version differences): TMC2209 UART defaulted to `Serial2` which doesn't exist on C6 (fixed to `Serial1`); servo PWM used the old channel-based LEDC API (`ledcSetup`/`ledcAttachPin`) which this core version replaced with a pin-keyed API (`ledcAttach`/`ledcWrite`)
  - Dropped (all legacy-only now): leader-comms code, closed-loop servo feedback, USB-CDC JSON follower protocol
  - **Stubbed, not real yet** (as of the scaffold; see the v1.2 block below for what has since changed): encoder register-level driver, step generation, homing sequence, USB↔CAN JSON translation, deterministic FreeRTOS-scheduled control loop (currently a placeholder `delay()` pace in `loop()`), true ISR-driven CAN RX dispatch (currently polled from `loop()`, not from TWAI's actual interrupt path)
- [x] **v1.2 hardware redesign (2026-10-01)** — new `docs/hardware/v1/v1.2_PCB_Design_Reference.md` + `v1.2_BOM_LCSC.csv` (v1.1 untouched). Fully through-hole so builders hand-solder the bare board; SGD 250 whole-arm budget. ESP32-C6 Super Mini on sockets; XL4016E1 buck (6.42 V) + LM2940CT-5.0 (5 V for the MCP2562-E/P CAN transceiver); AS5600 I²C module replaces the SPI encoder; 5.08 mm (24 V) + 3.5 mm (CAN) screw terminals replace XT30; CAN termination by a JCN3 jumper. Part numbers, stock and prices checked against LCSC public pages.
- [x] Motors corrected to **4× NEMA17** (earlier draft of the torque budget wrongly assumed NEMA23); `GEARBOX_TORQUE_BUDGET.md` redone per joint, URDF effort limits updated (6.5 N·m waist/elbow, 13 N·m shoulder).
- [x] `firmware/v1/joint_node` updated for v1.2, all four envs build: new pin map (CAN 14/20, servos 6/7/4, I²C 18/19, TMC2209 EN 3, UART TX 2 / RX 17), single-wire TMC2209 driver with real EN, AS5600 I²C driver with magnet-detect fault and multi-turn unwrap, E-stop path drops TMC2209 EN, `Serial` on native USB CDC. Not run on hardware.
- [x] NodeMesh compatibility note added to `PROJECT_SCOPE_AND_REPO_MAP.md`
- [ ] New KiCad project: `hardware_assets/pcb_project_files/SixEyes Joint PCB v1/` — one board design, 4 populated variants (Base/Shoulder L/Shoulder R/Elbow) per the v1.2 doc. **Blocked on confirming the real module footprints**: Super Mini (row spacing, length, pin order — buy a sample first), TMC2209 module and AS5600 module pin orders
- [ ] Bench-check USB-with-24 V behaviour (V5 diode OR-ing into the Super Mini 5 V pad), the XL4016 buck under 6 A servo bursts, and the 5 V rail at the real V6

## Gazebo virtual arm (new track, Gazebo-only for now — no physical bridge)

- [x] Wrote `docs/ros2/RPI5_SETUP_GUIDE.md` — ROS2-from-zero primer, Ubuntu 24.04 + ROS2 Jazzy + Gazebo Harmonic install/verify on a Raspberry Pi 5
- [x] **Design target set**: 500 g payload at 500 mm reach, 4× NEMA17 behind 1:25 printed cycloidal reductions. Torque budget in `docs/hardware/v1/GEARBOX_TORQUE_BUDGET.md` — static payload torque (2.45 N·m) plus self-weight/dynamic-load margins gives a 7.0 N·m shoulder design target; per joint at 0.4 N·m/motor: shoulder (two motors, shared gearbox) ~13 N·m, elbow ~6.5 N·m vs ~3.6 N·m needed, waist has no gravity load. All figures are placeholder estimates pending a real motor selection and a load-tested printed gearbox.
- [x] Scaffolded `ros2_ws/src/sixeyes_description/` — URDF/Xacro (`urdf/sixeyes.urdf.xacro`) with placeholder link dimensions (no mechanical CAD exists in-repo yet — `hardware_assets/3d_print_stl/` is an empty placeholder, same as `simulation/`; revisit once Ren Jie's mechanical design lands). Kinematic chain modeled correctly, **not** 1:1 with the 4 CAN nodes: waist (Base) → shoulder (Shoulder L/R lockstep, 1 DOF, not 2) → elbow → wrist pitch/yaw/gripper (3× open-loop servo stages). Reach sums to the 500mm target (220+180+100mm). Joint effort limits pulled from the torque budget above (6.5 N·m waist/elbow, 13 N·m shoulder, ~1.2 N·m representative MG996R servo joints). Includes a fixed 500g `payload_link` at the gripper tip so simulated gravity/torque loading matches the design target.
- [x] `launch/display.launch.py` — RViz-only URDF verification (robot_state_publisher + joint_state_publisher_gui + rviz2, no Gazebo yet) so the placeholder xacro can be sanity-checked before the full sim stack exists
- [x] Added `<ros2_control>` + `<gazebo>` (`gz_ros2_control`) blocks to `sixeyes.urdf.xacro`, one `command_interface` (position) + two `state_interface`s (position, velocity) per actuated joint, limits mirroring each joint's URDF `<limit>`
- [x] `config/sixeyes_controllers.yaml` — `joint_state_broadcaster` + `arm_controller` (`joint_trajectory_controller`), `update_rate: 250` matching the resolved `CONTROL_LOOP_HZ` decision so sim and real firmware target the same control rate
- [x] `sixeyes_bringup/launch/sim.launch.py` — starts Gazebo Harmonic (empty world), bridges `/clock` for `use_sim_time`, spawns the arm from `/robot_description`, spawns both controllers. Not yet run/tested (no ROS2/Gazebo tooling on this dev machine) -- all XML validated well-formed and both launch `.py` files validated as syntactically correct Python, but the actual `ros2 launch` invocation is unverified until it runs on the Pi.
- [ ] **First real test, once SSH to the Pi 5 is set up** (user will provide access): `ros2 launch sixeyes_bringup sim.launch.py`, confirm Gazebo opens with the arm spawned, `ros2 control list_controllers` shows both controllers active, and a manual `FollowJointTrajectory` goal (example in the launch file's docstring) actually moves the sim arm
- [ ] Verify `joint_state_node` can read simulated joint states with no changes, and `vla_inference_node` can drive the sim arm end-to-end
- [ ] Once this works, extend `docs/ros2/RPI5_SETUP_GUIDE.md` §8 with the actual `ros2 launch` command and expected output

## Next up

- [ ] Wire the CAN RX dispatch onto TWAI's actual interrupt path (or a high-priority pinned FreeRTOS task) instead of the current polled `loop()` call — needed to actually hit the ISR-driven E-stop latency budget in CAN_MESSAGE_PROTOCOL.md §5, not just approximate it
- [ ] Port a `MotorControlScheduler`-equivalent (deterministic FreeRTOS task at `CONTROL_LOOP_HZ`) — `loop()` currently paces itself with `delay()`, not a real timing guarantee
- [x] ~~Implement `encoder_driver.cpp`~~ — done for the AS5600 (I²C); still needs on-hardware validation and the magnet-position decision (motor shaft vs 1:25 output) that sets the counts→joint-degrees conversion in `motor_controller.cpp`
- [ ] Implement `motor_controller.cpp` step generation and homing sequence
- [ ] Implement `usb_bridge.cpp` JSON↔CAN translation (pairs with the `usb_bridge_node` ROS2 rework below)
- [ ] Bench-tune `MotorController::STALL_DIVERGENCE_THRESHOLD_DEG` once encoder units/gear ratio are known
- [x] ~~TMC2209 EN pin~~ — resolved: GPIO3, active low, 10 kΩ pull-up on the PCB (v1.2 doc §3/§7)
- [x] ~~Verify GPIO4/5 for CAN~~ — moot: CAN moved to GPIO14/20 (safe pins) in v1.2; GPIO4/5 are strapping pins and are not used for CAN
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
