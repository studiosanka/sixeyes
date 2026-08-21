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

## Open decisions (blocking firmware work — see CAN protocol doc §6)

- [ ] StallGuard-equivalent sensorless stall detection: keep alongside new SPI encoders, or fully replace with encoder-vs-command divergence?
- [ ] v1 control loop frequency — not yet fixed anywhere (Alpha=500Hz, Beta=400Hz precedent). Sets the E-stop latency budget.
- [ ] App-layer checksum on top of CAN's built-in 15-bit CRC — needed given safety-critical E-STOP/MOTOR_TARGET frames, or is transport-layer CRC sufficient?
- [ ] TWAI bus-off recovery behavior: stay disabled requiring manual recovery, or attempt auto-recovery?

## Next up

- [ ] Resolve open decisions above
- [ ] Scaffold `firmware/v1/joint_node/` — single PlatformIO project, role selected by `-DJOINT_NODE_ID=N` build flag
  - [ ] Reuse from Beta follower: TMC2209 driver, config patterns
  - [ ] New: CAN/TWAI driver + node protocol module (per CAN_MESSAGE_PROTOCOL.md)
  - [ ] New: SPI encoder driver (MT6835 / AS5048A)
  - [ ] New: open-loop servo PWM (Elbow only, subset of existing servo_control)
  - [ ] New: distributed safety/heartbeat module (bus heartbeat + node liveness, dual timeout model)
  - [ ] Drop: leader-comms code, closed-loop servo feedback, USB-CDC JSON follower protocol (all legacy-only now)
- [ ] New KiCad project: `hardware_assets/pcb_project_files/SixEyes Joint PCB v1/` — one board design, 4 populated variants (Base/Shoulder L/Shoulder R/Elbow) per BOM DNP table in v1 hardware doc
- [ ] `usb_bridge_node` (ROS2): rework to a Base-only USB link; Base translates ROS2 JSON/ASCII commands to CAN fan-out (see CAN protocol doc §1)
- [ ] `joint_state_node` / `safety_node`: update message assumptions once CAN protocol is implemented and Base-side translation exists
- [ ] Root `README.md` + `docs/README.md`: rewrite around v1 as primary generation, collapse Alpha/Beta into a single "Legacy Hardware" section instead of side-by-side comparison
- [ ] `docs/PROJECT_SCOPE_AND_REPO_MAP.md`: add NodeMesh compatibility note — "targets legacy Alpha/Beta hardware; not yet compatible with v1's distributed CAN architecture"
- [ ] Consider merging `docs/README.md` + `docs/PROJECT_SCOPE_AND_REPO_MAP.md` into one index (deferred from initial cleanup pass)

## Deferred (not in scope until IMU+IK exists)

- [ ] NodeMesh Node1 rework: swap potentiometer/leader-arm sampling for IMU stream feeding the same `ExperiencePacket.joints[6]` slot
- [ ] NodeMesh Node0 rework: from direct-GPIO single-board control to CAN-relay role talking to v1 Base node
- [ ] IMU + inverse-kinematics design doc (no design started yet — replaces the leader arm as control input)

## Repos unaffected by v1

- CycloCAD — pure gear geometry tooling, no dependency on electrical/firmware architecture. No action needed.
