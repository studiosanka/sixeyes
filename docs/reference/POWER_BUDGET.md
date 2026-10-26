# Power and current budget (v1.2, draft)

**Status: draft, all numbers are estimates.** Nothing here has been measured on a real arm yet. Each number says where it comes from. Replace the estimates with bench measurements when you have them (see [what to measure](#what-to-measure)).

This page answers four questions: how much current each board draws from the 24 V supply, how much each wire segment of the daisy chain carries, what wire and fuse that needs, and whether the servo rail copes. The board design is in the [PCB design reference](v1.2_PCB_Design_Reference.md) (§2 power, §9 connectors); the torque side is in the [gearbox torque budget](GEARBOX_TORQUE_BUDGET.md).

## 1. The power path

```
24 V supply ── fuse ── Base ── Shoulder L ── Shoulder R ── Elbow
                       (each hop: JPW2 out of one board to JPW1 in of the next)

On every board:
24 V in ─┬─ TMC2209 module VM  ──► stepper motor
         └─ XL4016 buck ──► V6 = 6.42 V ─┬─ servo headers (Elbow only)
                                         └─ LM2940 ──► 5 V ──► CAN transceiver, Super Mini 5 V pad
```

The 24 V wire runs in a **line**, so the first segment (supply to Base) carries the current of all four boards, and the last segment (Shoulder R to Elbow) carries only the Elbow's.

## 2. What each board draws from 24 V

All values are **per board, on the 24 V side**, in amps. They are estimates.

| Load | Idle / holding | Typical moving | Worst case | How it was estimated |
|---|---|---|---|---|
| Stepper (TMC2209) | 0.15 to 0.3 A | 0.5 A | 1.0 A | A stepper's supply current is far below its coil current, because the driver chops a low coil voltage out of 24 V. Holding at 1 A RMS in a 1.5 Ω coil is about 3 W, so about 0.14 A at 24 V. A moving motor draws more because of back-EMF. The worst case, 1.0 A, is a deliberately conservative figure (the design reference uses 1 to 1.5 A). The firmware default run current is 500 mA RMS; the TMC2209 chip maximum is 1.77 A RMS. |
| Logic (buck, 5 V, Super Mini, CAN, AS5600, TMC2209 logic) | 0.05 A | 0.1 A | 0.1 A | About 0.15 to 0.3 A at 5 V, which is about 0.05 to 0.1 A at 24 V. |
| **Board without servos** | **about 0.2 to 0.35 A** | **about 0.6 A** | **about 1.1 A** | Sum of the rows above. |
| Servo rail (Elbow only) | 0.01 A | 0.1 to 0.3 A | 1.9 A | The servos run from V6 (6.42 V). Draw at 24 V is I_servo × 6.42 V ÷ 24 V ÷ 0.85. At 6 A from the buck this is 1.9 A at 24 V. See §4. |

Servo figures for the MG996R are the **commonly quoted vendor figures, not verified**: about 10 mA idle, a few hundred mA while moving, about 2.5 A stalled at 6 V.

## 3. Current in each wire segment

Using the 24 V per-board numbers above. The Elbow board includes the servo rail.

| Segment | Idle / holding | Typical moving | Worst case | What it carries |
|---|---|---|---|---|
| Supply to Base | about 1.0 A | about 2.5 A | about 6.3 A | All four boards, with the Elbow's servos at 6 A |
| Base to Shoulder L | about 0.8 A | about 1.9 A | about 5.2 A | Shoulder L, Shoulder R, Elbow |
| Shoulder L to Shoulder R | about 0.6 A | about 1.3 A | about 4.1 A | Shoulder R, Elbow |
| Shoulder R to Elbow | about 0.3 A | about 0.7 A | about 3.0 A | Elbow only |

The worst case of about 6 to 8 A at the supply matches the design reference. Treat about **7.5 A** (the fuse rating) as the planning limit for the supply segment.

## 4. The servo rail

- The 6.42 V rail comes from the XL4016 buck, which the design treats as good for **5 to 6 A continuous** (the datasheet advertises 12 A).
- Three MG996R servos stalled at once would ask for about **7.5 A**, which is **above the rail's usable limit**.
- In normal use the servos move one or two at a time and are not stalled, so the rail is fine. The risk is the gripper clamping on an object or a wrist joint hitting its end stop and stalling.
- Mitigations to decide on: limit the number of servos that move at the same time in firmware, limit the servo travel so none can stall against a hard stop, and keep an eye on the buck temperature and the input capacitor (CIN is rated 2.05 A ripple and is already near its limit at 6 A).
- The MG996R takes 4.8 to 7.2 V, so 6.42 V is inside its range.

## 5. Wire gauge, voltage drop and fuse

| Item | Value | Notes |
|---|---|---|
| 24 V wire | **16 AWG** stranded wire (silicone or PVC), one gauge for the whole chain | 16 AWG carries roughly 10 A conservatively. 14 AWG is not needed. |
| Wire resistance | about 13 mΩ per metre (16 AWG copper) | Standard table value. |
| Segment lengths (placeholders) | supply to Base 1.0 m, other hops 0.4 m | **To be confirmed from the real arm.** |
| Worst-case drop, supply to Elbow | about 0.3 V in total | 2 conductors × length × 13 mΩ/m × segment current, summed over the four segments. The Elbow sees about 23.7 V of 24 V. |
| Terminal | KF128-5.08, 12 to 22 AWG, 24 A | One wire per terminal, separate IN and OUT blocks, ferrules on the ends. |
| Fuse | **7.5 A** mini blade fuse in an inline holder at the supply (the design reference suggested 8 A); for the first bench test use a current-limited supply or a 5 A blade fuse | Outside the board. The inline fuse is required by the [build guide](../04-build-the-pcb.md). |
| 24 V supply | at least 6.5 A (about 156 W) to cover the worst case; 10 A gives margin | The supply is not in the cost budget. |

Limits to keep in mind: TMC2209 maximum VM is about 29 V, XL4016 maximum input is 40 V, and the board TVS (P6KE30A) cannot protect both parts from a big transient. Use a regulated 24 V supply that stays at or below about 26 V.

## 6. What to measure

Fill this in with a bench supply that shows current, one board at a time, then the whole chain.

| Measurement | Estimate | Measured | Date |
|---|---|---|---|
| One board, motor holding at 500 mA RMS | 0.2 to 0.35 A | | |
| One board, motor moving at the planned speed | 0.6 A | | |
| One motor, run current raised to its limit | up to 1.1 A | | |
| Elbow board, one MG996R moving, no load | 0.1 to 0.3 A | | |
| Elbow board, one MG996R stalled (briefly) | up to about 0.8 A at 24 V | | |
| Buck temperature at 6 A on V6 | not known | | |
| Total at the supply, whole arm moving | about 2.5 A | | |

## 7. Open items

- Motor current per joint is not chosen yet: the 500 mA firmware default is a safe start, and the torque budget needs a higher figure for the shoulder.
- Whether more than one servo can move at once, and what limit the firmware applies.
- Real segment lengths, to settle the voltage drop.
- Measurements for §6.
- A supply-side reverse-polarity protection is not on the board (see the design reference §2).
