# SixEyes — Open-Loop Stepper Mitigation Strategies

**Status**: LEGACY — describes Alpha/Beta firmware only (`firmware/legacy/beta/follower_esp32/`). v1 hardware adds a per-joint SPI position encoder, so open-loop drift mitigation is no longer the primary strategy; see `docs/hardware/v1/v1_PCB_Design_Reference.md`.  
**Scope**: Documents every strategy added to compensate for the inherent limitations of open-loop stepper control — no shaft encoder, no ground-truth position verification  
**Related**: [Technical Reference — Beta Rev2](../../references/SixEyes%20Technical%20Reference%20June%202026.md) · [Teleoperation Mode Architecture (legacy)](TELEOPERATION_MODE_ARCHITECTURE.md)

---

## Why Open-Loop Steppers Drift

A stepper motor moves by advancing its rotor in discrete steps driven by a rotating magnetic field. If the commanded torque ever exceeds what the motor can deliver — due to mechanical load, acceleration, gearbox friction, or a stall — the rotor misses one or more steps and the firmware's commanded position silently diverges from the physical shaft angle. There is no native feedback path to detect or correct this.

For SixEyes specifically, three factors make this worse than a standard CNC application:

| Factor | Effect |
|---|---|
| 3D-printed cycloidal gearboxes | Variable backlash; compliant under load |
| 25:1 gear reduction | Motor-side slip is amplified at output |
| VLA dataset collection | Position error in training data degrades policy quality |

The strategies below do not eliminate open-loop drift — only shaft encoders can do that. They detect, bound, and recover from it as robustly as possible without additional hardware.

---

## Strategy 1 — StallGuard2 Sensorless Homing

**File**: `motor_controller.cpp` — `startStallGuardHoming()`, `runHomingStep()`  
**Driver support**: `tmc2209_driver.cpp` — `enableStallGuard()`, `isStalled()`

### Problem addressed
Without limit switches or encoders, the arm has no way to establish a known reference position after power-on. Any positional error from that point forward is absolute and unrecoverable without re-homing.

### How it works
The TMC2209 StallGuard2 feature measures back-EMF load continuously. When a joint is driven toward its mechanical hard stop, motor load spikes, and the SGTHRS threshold trip produces a stall signal. The firmware uses this as a sensorless end-stop.

**Three-phase homing sequence per joint:**

```
SEEK_STALL  →  BACKOFF  →  APPROACH  →  COMPLETE
```

| Phase | Speed | Action |
|---|---|---|
| SEEK_STALL | 180 °/s | Drive toward hard stop; wait for 3 consecutive stall detections (debounce) |
| BACKOFF | 180 °/s | Reverse 800 steps to clear the mechanical stop |
| APPROACH | 60 °/s | Slow re-approach; second stall detection latches the zero reference |

The slow APPROACH pass produces a more repeatable zero than the initial SEEK, since high-speed stall detection has velocity-dependent detection lag.

### Parameters
| Constant | Value | Location |
|---|---|---|
| Seek speed | 180 °/s | `motor_controller.h:HOMING_SEEK_SPEED_DEG_PER_S` |
| Approach speed | 60 °/s | `motor_controller.h:HOMING_APPROACH_SPEED_DEG_PER_S` |
| Backoff steps | 800 | `motor_controller.h:HOMING_BACKOFF_STEPS` |
| Stall debounce | 3 cycles | `motor_controller.h:HOMING_STALL_DEBOUNCE_CYCLES` |
| Default SGTHRS | 100 | `tmc2209_config.h:TMC2209_SGTHRS_DEFAULT` |

### Tuning note
`SGTHRS` (0–255) sets how aggressively StallGuard2 triggers. Too high → false stalls mid-move. Too low → missed stalls at the hard stop. Tune per-joint via the `HOME_STALLGUARD` command's `sensitivity` field. Expect to reduce sensitivity for the shoulder (higher inertia) relative to the elbow.

---

## Strategy 2 — SG_RESULT Continuous Load Monitoring

**File**: `tmc2209_driver.cpp` — `readSGResult()`  
**Register**: `DRV_STATUS[9:0]`

### Problem addressed
The DIAG pin (Strategy 3) is binary — it only asserts at the stall threshold. SG_RESULT gives the full 10-bit load value (0–1023) before a stall occurs, enabling pre-stall warning and speed reduction.

### How it works
`readSGResult()` reads `DRV_STATUS` and masks bits [9:0]. Values:

| SG_RESULT range | Interpretation |
|---|---|
| 1023 | No load — motor spinning freely |
| 512–1022 | Normal operating load |
| 1–511 | High load — approaching stall |
| 0 | Stall — motor not keeping up |

This is available via `readSGResult(motor_index, sg_result)` and can be polled in any diagnostic or telemetry path. At 400 Hz reading SG_RESULT for all four motors via PDN_UART would cost ~20% of the loop budget, so it is intentionally not polled every cycle — use the DIAG GPIO path (Strategy 3) for real-time stall detection.

### Practical use
- Read SG_RESULT during slow moves to calibrate per-joint SGTHRS before homing
- Log SG_RESULT to MicroSD in experience packets for offline load analysis
- Build a pre-stall speed reduction: if SG_RESULT < threshold, reduce commanded velocity before DIAG asserts

---

## Strategy 3 — DIAG Pin Stall Detection and Skipped Step Estimation

**File**: `motor_controller.cpp` — `update()` stall detection block  
**Driver support**: `tmc2209_driver.cpp` — `isDiagAsserted()`  
**GPIO**: `GPIO1` (J1) · `GPIO2` (J2) · `GPIO3` (J3) · `GPIO48` (J4)

### Problem addressed
When a stall occurs during normal motion (not homing), the firmware continues sending STEP pulses that the motor is not executing. Without detection, the position error accumulates silently and the arm's internal coordinate frame diverges from physical reality.

### How it works
`isDiagAsserted()` is a bare `digitalRead()` — no UART transaction, no PDN selection overhead. It is polled for all four motors every control loop cycle at negligible cost.

**Stall event tracking in `update()`:**

```
DIAG LOW  →  DIAG HIGH:  record stall_start_steps_[i]
DIAG HIGH →  DIAG LOW:   slipped = |current_steps_[i] - stall_start_steps_[i]|
                          estimated_skipped_steps_[i] += slipped
                          if cumulative > kSkippedStepWarningThreshold → warn
```

This gives a **conservative upper-bound estimate** of skipped steps — it counts all STEP pulses sent while DIAG was asserted. The true slip is less than this because:
- DIAG has detection latency (steps were already slipping before assertion)
- The motor may partially execute steps during a stall at high microstep count

### Skipped step threshold
| Constant | Value | Meaning |
|---|---|---|
| `kSkippedStepWarningThreshold` | 200 microsteps | ~1.25 full steps → ~0.45° at motor shaft → ~0.018° at output after 25:1 |

When the cumulative estimate exceeds this threshold, the firmware logs a re-home recommendation. The estimate is reset on `RESET_FAULT` (which should precede a re-home anyway).

### Accessor
```cpp
std::array<int32_t, NUM_STEPPERS> skipped = MotorController::instance().getEstimatedSkippedSteps();
```

---

## Strategy 4 — Backlash Compensation on Direction Reversal

**File**: `motor_controller.cpp` — `stepTimerCallback()` ISR, `doSingleMotorStep()`  
**Config**: `tmc2209_config.h:TMC2209_BACKLASH_STEPS` (default: 50)

### Problem addressed
3D-printed cycloidal gearboxes have mechanical backlash — a dead zone at the input shaft where the motor can rotate without moving the output. When a joint reverses direction, the first N steps do not produce output motion; the firmware's position counter advances while the physical joint does not. This introduces systematic position error on every direction change.

### How it works
On every direction reversal, extra steps are injected in the new direction **without advancing the firmware's step position counter**. These pulses take up the mechanical slack before position tracking resumes.

**Two injection sites:**

#### Normal motion (ISR — `stepTimerCallback`)
Direction change is detected by comparing the sign of `commanded_step_rate_steps_s_` against `last_isr_dir_positive_`. On mismatch, `backlash_remaining_[i]` is armed.

Each ISR cycle (100 µs) drains **one backlash pulse** before emitting normal steps. At `TMC2209_BACKLASH_STEPS = 50` this spreads injection across 50 ISR cycles (~5 ms) — well within normal motion timescales and consistent with the control loop rate.

```
Direction reversal detected
  → backlash_remaining_ = 50
  → each 100 µs ISR: emit 1 backlash pulse (no position update), then normal pulses
  → after 50 cycles: backlash_remaining_ = 0, normal stepping resumes
```

#### Homing (synchronous — `doSingleMotorStep`)
The BACKOFF → APPROACH transition is the only direction reversal in the homing sequence. At the start of APPROACH, all backlash steps are emitted synchronously before position-counted steps begin. This prevents the approach stall detection from triggering prematurely due to the shaft still being in the backlash dead zone.

### Tuning
`TMC2209_BACKLASH_STEPS` must be measured empirically per gearbox print:

1. Home an axis to establish zero
2. Command a move in the positive direction, then immediately reverse
3. Observe how many steps elapse before the output shaft visibly starts moving (use a dial indicator or mark on the output)
4. Set `TMC2209_BACKLASH_STEPS` to that count plus ~10% margin

Expected range for FDM-printed cycloidals: **30–150 microsteps** depending on print tolerances and material. Resin-printed cycloidals will be lower (~10–30).

---

## Strategy 5 — MSCNT Microstep Counter Access

**File**: `tmc2209_driver.cpp` — `readMSCNT()`  
**Register**: `MSCNT` (0x6A), range 0–1023

### Problem addressed
`current_steps_` in firmware is a software counter incremented with every commanded pulse. MSCNT is the TMC2209's internal microstep position within the current electrical cycle. Reading both provides a cross-check: if software steps advanced by N but MSCNT advanced by fewer than N (mod 1024), the motor slipped within that cycle.

### How it works
```cpp
uint16_t mscnt;
TMC2209Driver::instance().readMSCNT(motor_index, mscnt);
```

MSCNT wraps every 1024 counts (4 full steps at 256 microsteps, or 64 full steps at 16 microsteps). At 16 microsteps, one full electrical cycle = 256/16 = 16 full steps. Tracking rollover correctly requires monitoring the counter over time, not single-shot reads.

### Current status
MSCNT reading is implemented and available. Active cross-checking against `current_steps_` is not yet integrated into the control loop — it is available for diagnostic logging and future closed-loop correction work when shaft encoders are added.

---

## Strategy 6 — CoolStep Adaptive Current Scaling

**File**: `tmc2209_driver.cpp` — `configureMotor()`  
**Registers**: `COOLCONF` — `semin`, `semax`, `seup`, `sedn`

### Problem addressed
Running steppers at fixed high current wastes power and generates heat. But running at too-low current increases the probability of missed steps under load. CoolStep dynamically scales motor current based on measured load.

### Configuration
| Parameter | Value | Meaning |
|---|---|---|
| `semin` | 5 | Enable CoolStep when SG_RESULT drops below 5 × 32 |
| `semax` | 2 | Disable CoolStep when SG_RESULT rises above (semin + semax + 1) × 32 |
| `seup` | 3 | Current increment step size |
| `sedn` | 0 | Current decrement step size |

Under light load, CoolStep reduces RMS current — reducing heat and improving stall margin reserve (cooler motors have more torque headroom). Under heavy load, current automatically returns to the configured maximum.

---

## Strategy 7 — Software Zero Reference (HOME_ZERO)

**File**: `motor_controller.cpp` — `setCurrentPositionAsZero()`

### Problem addressed
Even without a stall-based homing sequence, the arm needs a way to declare "current position = zero" after manual positioning. This is the fallback reference operation when StallGuard homing is not appropriate (e.g., mid-session re-reference after a known move).

### How it works
`HOME_ZERO` sets `zero_offsets_steps_[i] = current_steps_[i]` for all axes simultaneously. All subsequent position commands are interpreted relative to this reference. The ISR step counter is also synced to prevent transient commanded motion after zeroing.

---

## Summary Table

| Strategy | Addresses | When Active | Hardware Required |
|---|---|---|---|
| StallGuard homing | No absolute reference after power-on | On `HOME_STALLGUARD` command | DIAG pins (Beta) or PDN_UART (Alpha) |
| SG_RESULT monitoring | Pre-stall load visibility | On-demand / diagnostic | PDN_UART |
| DIAG stall detection + skipped step estimation | Silent position drift during motion | Every control loop cycle | DIAG GPIO pins (Beta only) |
| Backlash compensation | Direction-reversal dead zone | Every direction change | None (firmware only) |
| MSCNT cross-check | Per-cycle slip detection | On-demand / diagnostic | PDN_UART |
| CoolStep | Thermal headroom for stall margin | Always (automatic) | None (TMC2209 internal) |
| HOME_ZERO | Manual re-reference | On `HOME_ZERO` command | None |

---

## Known Limitations

These strategies improve robustness significantly but do not make the system fully closed-loop. Remaining gaps:

- **Skipped step estimation is an upper bound**, not an exact count. The actual slip during a stall event may be less than reported.
- **Backlash compensation assumes constant backlash**. 3D-printed gearboxes have pose-dependent and wear-dependent backlash. The single `TMC2209_BACKLASH_STEPS` constant is a fixed approximation.
- **MSCNT cross-checking is not yet active** in the control loop. Slip smaller than one full electrical cycle goes undetected.
- **None of these strategies can correct accumulated position error** — they can only detect it and prompt a re-home. True closed-loop correction requires shaft encoders (AS5600 or equivalent mounted on motor rear shaft via satellite PCB).

See the [Technical Reference](../references/SixEyes%20Technical%20Reference%20June%202026.md) §14 for future encoder integration planning.

---

*Last updated: June 2026*
