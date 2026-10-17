# Cycloidal Gearbox Torque Budget

**Status**: Current — informs the printed cycloidal reduction ratio choice for the steppers driving v1's joints.
**Design target**: 500 g payload at 500 mm reach.

This is a first-pass budget using placeholder assumptions, not a validated engineering result — every number below is flagged with its uncertainty. Revisit once real motor current settings, printed gearbox efficiency measurements, and actual link masses exist.

## 1. Static holding torque, payload only

Worst case: arm fully extended horizontally, 500 g at 500 mm from the joint.

```
τ_payload = m · g · L = 0.5 kg × 9.81 m/s² × 0.5 m = 2.45 N·m
```

## 2. Design target, with margin

Two multipliers stack on top of the payload-only figure:

- **Arm self-weight**: unknown until real CAD exists (no mechanical model is in this repo yet). Placeholder: +50% (`×1.5`), a rough rule of thumb for a lightweight printed arm where distributed link mass contributes a comparable moment to the payload itself.
- **Application safety factor**: `×1.75`, covering dynamic loads (acceleration, not just static hold), backlash, and general print-quality uncertainty.

```
τ_design_target = 2.45 × 1.5 × 1.75 ≈ 6.4 N·m  →  rounded up to 7.0 N·m
```

**7.0 N·m at the joint output is the number the motor+gearbox combination needs to clear.**

## 3. Motor + gearbox torque available

**Motor**: NEMA17, 40 mm body class, driven by a TMC2209 — four of them, all behind 1:25 printed cycloidal reductions (June 2026 Technical Reference). A 40 mm NEMA17 is roughly **0.3–0.4 N·m** holding torque; 0.4 N·m is the Technical Reference figure and is used below, with 0.3 N·m as the pessimistic case (small NEMA17s run on a TMC2209 sustain closer to 1.0–1.5 A RMS, and the dense 4-driver PCB has no per-driver cooling). These are estimates, not a datasheet figure for a chosen motor part.

**Gearbox efficiency assumption**: FDM-printed cycloidal drives are not metal-gearbox efficient. Friction, layer adhesion, and tolerance stack typically put them in the **60–70%** range. Used **65%** below.

Per-joint output torque at 1:25 (`motor × 25 × 0.65`):

| Joint | Motors | 0.4 N·m motor | 0.3 N·m motor |
|---|---|---|---|
| Waist (Base) | 1 | 6.5 N·m | 4.9 N·m |
| Shoulder (L+R, shared gearbox) | 2 | 13.0 N·m | 9.75 N·m |
| Elbow | 1 | 6.5 N·m | 4.9 N·m |

## 4. Per-joint requirement and margin

The 7.0 N·m figure in §2 is the **shoulder** requirement (500 mm lever). Other joints see a shorter lever or no gravity load:

- **Waist**: rotates about the vertical axis — no gravity torque, only inertial load. Not the limiting joint.
- **Shoulder**: 7.0 N·m target vs. 13.0 N·m (0.4) / 9.75 N·m (0.3) available from two motors → **~1.9× / ~1.4×**. The torque split between Shoulder L/R is what makes this joint work.
- **Elbow**: lever to the tip is about 280 mm (forearm 180 + wrist/gripper 100), so payload torque is 0.5 × 9.81 × 0.28 ≈ 1.37 N·m; with the same ×1.5 self-weight and ×1.75 safety factors that is ≈ 3.6 N·m, vs. 6.5 N·m (0.4) / 4.9 N·m (0.3) available from one motor → **~1.8× / ~1.35×**.

A single NEMA17 at 1:25 could **not** carry the shoulder alone (6.5 N·m < 7.0 N·m), which is why the shoulder is a dual-motor joint.

## 5. Decision: 1:25 on all stepper joints

The single largest uncertainty is printed-gearbox efficiency — the term most likely to land worse than estimated on a first print. The system runs **open-loop steppers**: under-torque means missed steps, which v1's encoder-vs-command divergence check (see [CAN_MESSAGE_PROTOCOL.md](CAN_MESSAGE_PROTOCOL.md) §4.3a) catches *after the fact*, not before. Margins are healthy at 0.4 N·m and thin-but-positive at 0.3 N·m, so there is no room to drop to a lower ratio; going higher would cost joint speed.

**Trade-off accepted**: a higher ratio reduces maximum joint angular velocity; for a 500 g-class manipulator this is not expected to be speed-limiting.

## 6. Open items this budget does not resolve

- No real link masses exist yet (no mechanical CAD in-repo) — the "+50% self-weight" multiplier in §2 and the elbow lever above are placeholders, not measured.
- No specific motor part has been selected — the 0.3–0.4 N·m range is for "a 40 mm NEMA17," not a datasheet number. Redo with the real motor and TMC2209 RMS current setting.
- No printed gearbox has been built and load-tested — the 65% efficiency figure is a literature-typical estimate for FDM cycloidal drives generally.
- Once real numbers exist, update the joint effort limits in `sixeyes_description`'s URDF accordingly.
