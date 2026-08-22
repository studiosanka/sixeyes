# Cycloidal Gearbox Torque Budget

**Status**: Current — informs both the URDF placeholder joint limits (`ros2_ws/src/sixeyes_description/`) and the printed cycloidal reduction ratio choice for the steppers driving v1's joints.
**Design target**: 500 g payload at 500 mm reach.

This is a first-pass budget using placeholder assumptions, not a validated engineering result — every number below is flagged with its uncertainty. Revisit once real motor current settings, printed gearbox efficiency measurements, and actual link masses exist.

## 1. Static holding torque, payload only

Worst case: arm fully extended horizontally, 500 g at 500 mm from the joint.

```
τ_payload = m · g · L = 0.5 kg × 9.81 m/s² × 0.5 m = 2.45 N·m
```

## 2. Design target, with margin

Two multipliers stack on top of the payload-only figure:

- **Arm self-weight**: unknown until real CAD exists (no mechanical model is in this repo yet — see `docs/V1_TODO.md`). Placeholder: +50% (`×1.5`), a rough rule of thumb for a lightweight printed arm where distributed link mass contributes a comparable moment to the payload itself.
- **Application safety factor**: `×1.75`, covering dynamic loads (acceleration, not just static hold), backlash, and general print-quality uncertainty.

```
τ_design_target = 2.45 × 1.5 × 1.75 ≈ 6.4 N·m  →  rounded up to 7.0 N·m
```

**7.0 N·m at the joint output is the number the motor+gearbox combination needs to clear.**

## 3. Motor + gearbox torque available

**Motor torque assumption**: NEMA23 driven by TMC2209. TMC2209's spec ceiling is ~2A RMS, but on a dense 4-driver v1 PCB without dedicated per-driver cooling, realistic *sustained* current is closer to 1.4–1.7A RMS before thermal derating matters. At that current, a representative NEMA23 gives roughly **0.6 N·m** motor-side holding torque — this is an estimate, not a datasheet figure for a specific chosen motor part; revisit once a motor is actually selected.

**Gearbox efficiency assumption**: FDM-printed cycloidal drives are not metal-gearbox efficient. Friction, layer adhesion, and tolerance stack typically put them in the **60–70%** range. Used **65%** below as a middle estimate.

| Reduction | Motor torque | Gearbox efficiency | Output torque | Margin vs. 7.0 N·m target |
|---|---|---|---|---|
| 1:20 | 0.6 N·m | 65% | 0.6 × 20 × 0.65 ≈ **7.8 N·m** | ~1.1× — thin |
| 1:25 | 0.6 N·m | 65% | 0.6 × 25 × 0.65 ≈ **9.75 N·m** | ~1.4× — comfortable |

## 4. Decision: 1:25

The single largest uncertainty in this whole budget is printed-gearbox efficiency — it's the term most likely to land worse than estimated on a first print (poor layer adhesion, insufficient lubrication, tolerance drift with wear). The system runs **open-loop steppers**: under-torque means missed steps, which v1's encoder-vs-command divergence check (see `docs/protocols/CAN_MESSAGE_PROTOCOL.md` §4.3a) catches *after the fact*, not before. Given that, trading some maximum joint speed for stall-margin headroom against the biggest unknown is the right call.

**Trade-off accepted**: 1:25 vs 1:20 reduces maximum joint angular velocity by a further ~20% (25/20 speed ratio) for the same motor speed. For a 500 g-class manipulator this is not expected to be speed-limiting, but flagging it as a real trade, not a free upgrade.

## 5. Open items this budget does not resolve

- No real link masses exist yet (no mechanical CAD in-repo) — the "+50% self-weight" multiplier in §2 is a placeholder, not measured.
- No specific motor part has been selected — §3's 0.6 N·m figure is a representative estimate for "a NEMA23 at ~1.5A," not a datasheet number.
- No printed gearbox has been built and load-tested — the 65% efficiency figure is a literature-typical estimate for FDM cycloidal drives generally, not this design specifically.
- Once a real motor, TMC2209 current setting, and a printed/tested gearbox exist, redo this calc with real numbers and update the joint effort limits in `sixeyes_description`'s URDF accordingly.
